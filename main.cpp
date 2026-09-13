/*******************************************************************************
 *        Header Files
 *******************************************************************************/
#include "FreeRTOSConfig.h"
#include "cyabs_rtos_impl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "cybt_platform_trace.h"
#include "cyhal.h"
#include "cyhal_gpio.h"
#include "cyhal_system.h"
#include "portmacro.h"
#include "stdio.h"
#include "cyabs_rtos.h"
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>
#include <timers.h>
#include "GeneratedSource/cycfg_gatt_db.h"
#include "app_bt_gatt_handler.h"
#include "app_bt_utils.h"
#include "wiced_bt_ble.h"
#include "wiced_bt_uuid.h"
#include "wiced_memory.h"
#include "wiced_bt_stack.h"
#include "cycfg_bt_settings.h"
#include "cycfg_gap.h"
#include "cybsp_bt_config.h"


// Include Sensor BME680
#include "adapter_680.hpp"
#include "measure.hpp"
#include "cyhal_hw_types.h"
#include "cyhal_i2c.h"
#include "cyhal_system_impl.h"
#include "stdint.h"

// Include Pantalla LCD
#include "st7032.hpp"

/*******************************************************************************
 *        Macro Definitions
 *******************************************************************************/

/* This is the temperature measurement interval which is same as configured in
 * the BT Configurator - The variable represents interval in milliseconds.
 */
 // Cuentas del tasker 29999 = 3s
#define POLL_TIMER_IN_MSEC              (29999)
// Frecuencia de conteo 10000 = 1s
#define POLL_TIMER_FREQ                 (10000)

/* Temperature Simulation Constants */
#define DEFAULT_TEMPERATURE             (2500u)
#define MAX_TEMPERATURE_LIMIT           (3000u)
#define MIN_TEMPERATURE_LIMIT           (2000u)
#define DELTA_TEMPERATURE               (100u)

/* Number of advertisment packet */
#define NUM_ADV_PACKETS                 (3u)

/* Absolute value of an integer. The absolute value is always positive. */
#ifndef ABS
#define ABS(N) ((N<0) ? (-N) : (N))
#endif

/* Check if notification is enabled for a valid connection ID */
#define IS_NOTIFIABLE(conn_id, cccd) (((conn_id)!= 0)? (cccd) & GATT_CLIENT_CONFIG_NOTIFICATION: 0)

#define CMD_TO_CMD_DELAY        (1000UL)

/* Packet positions */
#define PACKET_SOP_POS          (0UL)
#define PACKET_CMD_POS          (1UL)
#define PACKET_EOP_POS          (2UL)

/* Start and end of packet markers */
#define PACKET_SOP              (0x01UL)
#define PACKET_EOP              (0x17UL)

/* I2C slave address to communicate with */
#define I2C_SLAVE_ADDR          (0x76)

/* I2C bus frequency */
#define I2C_FREQ                (400000UL)

/* Command valid status */
#define STATUS_CMD_DONE         (0x00UL)

/* Packet size */
#define PACKET_SIZE             (3UL)

/******************************************************************************
 *                                 TYPEDEFS
 ******************************************************************************/

/*******************************************************************************
 *        Variable Definitions
 *******************************************************************************/
/* Configuring Higher priority for the application */
volatile int uxTopUsedPriority;

/* Manages runtime configuration of Bluetooth stack */
extern const wiced_bt_cfg_settings_t wiced_bt_cfg_settings;

/* FreeRTOS variables */

TaskHandle_t ess_task_handle;

QueueHandle_t lcd_queue;

// Mutex para I2C, para que no haya condición de carrera en la utilización del bus I2C.
static SemaphoreHandle_t i2c_mutex = nullptr;

/* Status variable for connection ID */
uint16_t app_bt_conn_id;

/* Variable for 5 sec timer object */
static cyhal_timer_t ess_timer_obj;
/* Configure timer for 5 sec */
const cyhal_timer_cfg_t ess_timer_cfg =
    {
		.is_continuous = true,                 /* Run timer indefinitely */
    	.direction = CYHAL_TIMER_DIR_UP,       /* Timer counts up */
        .is_compare = false,                   /* Don't use compare mode */
        .period = POLL_TIMER_IN_MSEC, /* Defines the timer period */
        .compare_value = 0,                    /* Timer compare value, not used */   
        .value = 0                             /* Initial value of counter */
};
/*******************************************************************************
 *        Function Prototypes
 *******************************************************************************/

/* Callback function for Bluetooth stack management type events */
static wiced_bt_dev_status_t
app_bt_management_callback(wiced_bt_management_evt_t event,
                           wiced_bt_management_evt_data_t *p_event_data);

/* This function sets the advertisement data */
static wiced_result_t app_bt_set_advertisement_data(void);

/* This function initializes the required BLE ESS & thermistor */
static void bt_app_init(void);

/* Task to send notifications with dummy temperature values */
void ess_task(void *pvParam);

/* Tarea del funcionamiento de la pantalla*/
void lcd_task(void *pvParam);

/* HAL timer callback registered when timer reaches terminal count */
void ess_timer_callb(void *callback_arg, cyhal_timer_event_t event);

/* This function starts the advertisements */
static void app_start_advertisement(void);

/******************************************************************************
 *                          Function Definitions
 ******************************************************************************/

/*
 *  Entry point to the application. Set device configuration and start BT
 *  stack initialization.  The actual application initialization will happen
 *  when stack reports that BT device is ready.
 */
 
 void handle_error(uint32_t status)
{
    if (status != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
}
 
int main(void)
{
    uxTopUsedPriority = configMAX_PRIORITIES - 1;
    
	cy_rslt_t result;
    wiced_result_t wiced_result;
    BaseType_t rtos_result;
    
    
	result = cybsp_init();
    handle_error(result);
    
    // Hace que se pueda comunicar por UART para escribir mensajes por serial.
	// El baudrate típico suele ser de 115200 baudios  
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    
    // Inicialización del semaforo.
    i2c_mutex = xSemaphoreCreateMutex();
    configASSERT(i2c_mutex != nullptr);
     
    /* Initialising the HCI UART for Host contol */
    cybt_platform_config_init(&cybsp_bt_platform_cfg);
    
    /* Inicialización del I2C */
	__enable_irq();
	    
	static cyhal_i2c_t i2c;
	cyhal_i2c_cfg_t i2c_inst = {
		.is_slave = CYHAL_I2C_MODE_MASTER, // = false
		.address = 0,
		.frequencyhal_hz = 100000
	};
	
	//Inicialización por defecto de la comunicación I2C
	result = cyhal_i2c_init(&i2c, CYBSP_I2C_SDA, CYBSP_I2C_SCL, NULL);
	if(result != CY_RSLT_SUCCESS)
		printf("Error en I2C.");
	else printf("\nInicialización I2C exitosa.\n");
	
    result = cyhal_i2c_configure(&i2c, &i2c_inst);
	if(result != CY_RSLT_SUCCESS)
		printf("Error en I2C.");
	else printf("Configuración I2C exitosa\n");
	
	// Creación e inicialización de la Pantalla LCD
    static ST7032 lcd(&i2c);
    lcd.init();
	
	// Creación e inicialización del sensor BME680
    static adapter_680 sensor1(&i2c);
    sensor1.init();

    printf(" Sensor BME_680\n");

    /* Register call back and configuration with stack */
    wiced_result = wiced_bt_stack_init(app_bt_management_callback, &wiced_bt_cfg_settings);

    /* Check if stack initialization was successful */
    if (WICED_BT_SUCCESS == wiced_result) {
        printf("Bluetooth Stack Initialization Successful \n");
    } else {
        printf("Bluetooth Stack Initialization failed!!\n");
    }
        
    // Creación de la cola
    lcd_queue = xQueueCreate(1, sizeof(measure));
    configASSERT(lcd_queue != nullptr);
    
	// Creación de la tarea del sensor
    rtos_result = xTaskCreate(ess_task, 
    						  "ESS Task",
    						  (configMINIMAL_STACK_SIZE * 4),
    						  &sensor1,
    						  (configMAX_PRIORITIES - 3),
    						  &ess_task_handle);
    if(pdPASS == rtos_result)
    {
        printf("ESS task creado correctamente\n");
    }
    else
    {
        printf("ESS task no ha podido ser creado correctamente\n");
    }
    
    //Creación de la tarea de la pantalla
    rtos_result = xTaskCreate(lcd_task,
    						  "LCD Task",
    						  configMINIMAL_STACK_SIZE * 4,
    						  &lcd,
    						  configMAX_PRIORITIES - 4,
    						  nullptr);
    						  
    if(pdPASS == rtos_result)
    {
        printf("LCD task creado correctamente\n");
    }
    else
    {
        printf("LCD task no ha podido ser creado correctamente\n");
    }

    /* Start the FreeRTOS scheduler */
    vTaskStartScheduler();

    /* Should never get here */
    CY_ASSERT(0);
}

/*
 * Function Name: app_bt_management_callback()
 *
 *@brief
 *  This is a Bluetooth stack event handler function to receive management events
 *  from the Bluetooth LE stack and process as per the application.
 *
 * @param wiced_bt_management_evt_t  Bluetooth LE event code of one byte length
 * @param wiced_bt_management_evt_data_t  Pointer to Bluetooth LE management event
 *                                        structures
 *
 * @return wiced_result_t Error code from WICED_RESULT_LIST or BT_RESULT_LIST
 *
 */
static wiced_result_t
app_bt_management_callback(wiced_bt_management_evt_t event,
                           wiced_bt_management_evt_data_t *p_event_data)
{
    wiced_bt_dev_status_t status = WICED_ERROR;


    switch (event) {

    case BTM_ENABLED_EVT:
    {
        printf("Discover this device with the name:%s\n", app_gap_device_name);

        print_local_bd_address();

        printf("\nBluetooth Management Event: \t");
        printf("%s\n", get_btm_event_name(event));

        /* Perform application-specific initialization */
        bt_app_init();
    }break;

    case BTM_DISABLED_EVT:
        /* Bluetooth Controller and Host Stack Disabled */
        printf("\n");
        printf("Bluetooth Management Event: \t");
        printf("%s", get_btm_event_name(event));
        printf("\n");
        printf("Bluetooth Disabled\n");
        break;

    case BTM_BLE_ADVERT_STATE_CHANGED_EVT:
    {
        wiced_bt_ble_advert_mode_t *p_adv_mode = &p_event_data->ble_advert_state_changed;
        /* Advertisement State Changed */
        printf("\n");
        printf("Bluetooth Management Event: \t");
        printf("%s", get_btm_event_name(event));
        printf("\n");
        printf("\n");
        printf("Advertisement state changed to ");
        printf("%s", get_btm_advert_mode_name(*p_adv_mode));
        printf("\n");
    }break;

    default:
        printf("\nUnhandled Bluetooth Management Event: %d %s\n",
                event,
                get_btm_event_name(event));
        break;
    }

    return (status);
}

/*
 Function name:
 bt_app_init

 Function Description:
 @brief    This function is executed if BTM_ENABLED_EVT event occurs in
           Bluetooth management callback.

 @param    void

 @return    void
 */
static void bt_app_init(void)
{
    wiced_bt_gatt_status_t gatt_status = WICED_BT_GATT_ERROR;
    cy_rslt_t rslt;

    /* Register with stack to receive GATT callback */
    gatt_status = wiced_bt_gatt_register(app_bt_gatt_event_callback);
    printf("\n gatt_register status:\t%s\n",get_gatt_status_name(gatt_status));

    /* Initialize the User LED */
    cyhal_gpio_init(CONNECTION_LED,
                    CYHAL_GPIO_DIR_OUTPUT,
                    CYHAL_GPIO_DRIVE_STRONG,
                    CYBSP_LED_STATE_OFF);

    /* Initialize the HAL timer used to count 5 seconds */
    rslt = cyhal_timer_init(&ess_timer_obj, NC, NULL);
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("ESS timer init failed !\n");
    }
    /* Configure the timer for 5 seconds */
    cyhal_timer_configure(&ess_timer_obj, &ess_timer_cfg);
    rslt = cyhal_timer_set_frequency(&ess_timer_obj, POLL_TIMER_FREQ);
    if (CY_RSLT_SUCCESS != rslt)
    {
        printf("ESS timer set freq failed !\n");
    }
    /* Register for a callback whenever timer reaches terminal count */
    cyhal_timer_register_callback(&ess_timer_obj, ess_timer_callb, NULL);
    cyhal_timer_enable_event(&ess_timer_obj, CYHAL_TIMER_IRQ_TERMINAL_COUNT, 3, true);

    /* Start the timer */
    if (CY_RSLT_SUCCESS != cyhal_timer_start(&ess_timer_obj))
    {
        printf("ESS timer start failed !");
    }

    /* Initialize GATT Database */
    gatt_status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);
    if (WICED_BT_GATT_SUCCESS != gatt_status) {
        printf("\n GATT DB Initialization not successful err 0x%x\n", gatt_status);
    }

    /* Start Bluetooth LE advertisements */
    app_start_advertisement();

}


/**
 * @brief This function starts the Blueooth LE advertisements and describes
 *        the pairing support
 */
static void app_start_advertisement(void)
{
    wiced_result_t wiced_status;

    /* Set Advertisement Data */
    wiced_status = app_bt_set_advertisement_data();
    if (WICED_SUCCESS != wiced_status) {
        printf("Raw advertisement failed err 0x%x\n", wiced_status);
    }

    /* Do not allow peer to pair */
    wiced_bt_set_pairable_mode(WICED_FALSE, FALSE);

    /* Start Undirected LE Advertisements on device startup. */
    wiced_status = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,
                                                 BLE_ADDR_PUBLIC,
                                                 NULL);

    if (WICED_SUCCESS != wiced_status) {
        printf( "Starting undirected Bluetooth LE advertisements"
                "Failed err 0x%x\n", wiced_status);
    }
}

/*
 Function Name:
 app_bt_set_advertisement_data

 Function Description:
 @brief  Set Advertisement Data

 @param void

 @return wiced_result_t WICED_SUCCESS or WICED_failure
 */
static wiced_result_t app_bt_set_advertisement_data(void)
{

    wiced_result_t wiced_result = WICED_SUCCESS;
    wiced_result = wiced_bt_ble_set_raw_advertisement_data( NUM_ADV_PACKETS,
                                                            cy_bt_adv_packet_data);

    return (wiced_result);
}

/*
 Function name:
 ess_timer_callb

 Function Description:
 @brief  This callback function is invoked on timeout of 5 seconds timer.

 @param  void*: unused
 @param cyhal_timer_event_t: unused

 @return void
 */
void ess_timer_callb(void *callback_arg, cyhal_timer_event_t event)
{
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(ess_task_handle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 Function name:
 ess_task

 Function Description:
 @brief  This task updates dummy temperature value every time it is notified
         and sends a notification to the connected peer

 @param  void*: unused

 @return void
 */
void ess_task(void *pvParam)
{
	adapter_680 *sensor = static_cast<adapter_680*>(pvParam);
	
	uint8_t error = 0;
	
	int i = 1;
	
    while(true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Le dice al sensor que tome las medidas estando protegido el bus i2c por i2c_mutex
		xSemaphoreTake(i2c_mutex, portMAX_DELAY);
		error = sensor->take_measure();
		xSemaphoreGive(i2c_mutex);
    	if(error != 0)
   		{
			printf("Error en la medición.");
			return ;
		}
		
		// Mide y muestra el número de la medida y sus valores
		printf("\nMedición %d\n", i++);
		sensor->show();
        
        // Medidas contenidas en m
        measure m = sensor->getm();
        
        // Manda la medida realizada a la cola
        xQueueOverwrite(lcd_queue, &m);
        
        int16_t temperatura = static_cast<int16_t>(m.get_temperature()*100);
        uint16_t humedad = static_cast<int16_t>(m.get_humidity()*100);
        uint32_t presion = static_cast<int32_t>(m.get_pressure()*10);
        uint32_t resistencia = static_cast<int32_t>(m.get_resistance());

		memcpy(app_ess_temperature, &temperatura, sizeof(temperatura));
		memcpy(app_ess_humidity, &humedad, sizeof(humedad));
		memcpy(app_ess_pressure, &presion, sizeof(presion));
		memcpy(app_custom_service_custom_characteristic, &resistencia, sizeof(resistencia));

            if(!app_bt_conn_id)
            {
                printf("This device is not connected to a central device\n");
            }else{
				/* Temperatura */
    			if (IS_NOTIFIABLE(app_bt_conn_id, app_ess_temperature_client_char_config[0]))
   				{
        			wiced_bt_gatt_status_t gatt_status;

       				gatt_status = wiced_bt_gatt_server_send_notification(app_bt_conn_id,
            													 		HDLC_ESS_TEMPERATURE_VALUE,
            													 		app_ess_temperature_len,
            															 app_ess_temperature,
            															 NULL);
    			}

    			/* Humedad */
    			if (IS_NOTIFIABLE(app_bt_conn_id, app_ess_humidity_client_char_config[0]))
    			{
       				wiced_bt_gatt_status_t gatt_status;

        			gatt_status = wiced_bt_gatt_server_send_notification(app_bt_conn_id,
            															HDLC_ESS_HUMIDITY_VALUE,
            															app_ess_humidity_len,
            															app_ess_humidity,
            															NULL);
    			}

    			/* Presión */
    			if (IS_NOTIFIABLE(app_bt_conn_id, app_ess_pressure_client_char_config[0]))
    			{
        			wiced_bt_gatt_status_t gatt_status;

        			gatt_status = wiced_bt_gatt_server_send_notification(app_bt_conn_id,
            															HDLC_ESS_PRESSURE_VALUE,
            															app_ess_pressure_len,
            															app_ess_pressure,
            															NULL);
    			}

   				/* Resistencia de gas */
    			if (IS_NOTIFIABLE(app_bt_conn_id, app_custom_service_custom_characteristic_client_char_config[0]))
    			{
        			wiced_bt_gatt_status_t gatt_status;

        			gatt_status = wiced_bt_gatt_server_send_notification(app_bt_conn_id,
            															HDLC_CUSTOM_SERVICE_CUSTOM_CHARACTERISTIC_VALUE,
            															app_custom_service_custom_characteristic_len,
            															app_custom_service_custom_characteristic,
            															NULL);
     			}
            }
        }
    }
    
    void lcd_task(void *pvParam)
    {
		// Conversión de pvParam a ST7032
		ST7032 *lcd = static_cast<ST7032 *>(pvParam);
        configASSERT(lcd != nullptr);
        
        // Variable donde se copian las mediciones
        measure m;
        float valor = 0;
        
        while(true)
        {

			if (xQueueReceive(lcd_queue, &m, portMAX_DELAY) == pdPASS)
			{
				char cad1[8], cad2[8], cad3[10], cad4[10];
				float p = 0.1;
				
				valor = m.get_temperature();
				
				// Caracteres de salida por pantalla
				cad1[0] = 'T';
				cad1[1] = '=';
				for(int i = 0; i<2; i++)
				{
					cad1[i+2] = ((int)(valor*p) % 10) + '0';
					p *= 10;
				}
				cad1[4] = '.';
				cad1[5] = ((int)(valor*p) % 10) + '0';
				cad1[6] = '\0';
				
				valor = m.get_humidity();
				p = 0.1;
				
				cad2[0] = 'H';
				cad2[1] = '=';
				for(int i = 0; i<2; i++)
				{
					cad2[i+2] = ((int)(valor*p) % 10) + '0';
					p *= 10;
				}
				cad2[4] = '.';
				cad2[5] = ((int)(valor*p) % 10) + '0';
				cad2[6] = '\0';
				
				valor = m.get_pressure();
				p = 0.00001;
				
				cad3[0] = 'P';
				cad3[1] = '=';
				for(int i = 0; i<6; i++)
				{
					cad3[i+2] = ((int)(valor*p) % 10) + '0';
					p *= 10;
				}
				cad3[8] = '\0';
				
				valor = m.get_resistance();
				p = 0.00001;
				
				cad4[0] = 'R';
				cad4[1] = '=';
				for(int i = 0; i<6; i++)
				{
					cad4[i+2] = ((int)(valor*p) % 10) + '0';
					p *= 10;
				}
				cad4[8] = '\0';
				
				// Mutex del bus i2c
				xSemaphoreTake(i2c_mutex, portMAX_DELAY);
				
				// Código de actualización de la pantalla
				lcd->clear();
				lcd->setCursor(0, 0);
				lcd->print(cad1);
				lcd->setCursor(0, 8);
				lcd->print(cad3);
				lcd->setCursor(1, 0);
				lcd->print(cad2);
				lcd->setCursor(1, 8);
				lcd->print(cad4);
				
				//Liberación del mutex i2c
				xSemaphoreGive(i2c_mutex);
				
			}
			
		}
	}
/* [] END OF FILE */
