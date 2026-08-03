/*
 * adapter_680.cpp
 *
 *  Created on: 28 may 2026
 *      Author: kiker
 */


#include"adapter_680.hpp"
#include "bme68x.hpp"
#include "bme68x_defs.hpp"
#include "cybsp.h"
#include "cyhal_i2c.h"
#include "cyhal_system.h"
#include "cyhal_system_impl.h"
#include <cstdint>
#include <cstdio>


#define HEATER_TEMP 320
#define HEATER_DUR 150

adapter_680::adapter_680(cyhal_i2c_t* i,  uint8_t ad)
{
	con.i2c = i;
	con.address = ad;
}

void adapter_680::init()
{
	//bme68x_soft_reset(&dev);
	
	// Asignación de las funciones de lectura, escritura y delay
	dev.read = i2c_read;
	dev.write = i2c_write;
	dev.delay_us = delay_us;
	// Asignación de la interfaz del dispositivo
	dev.intf = BME68X_I2C_INTF;
	dev.intf_ptr = &con;
	// Asignación del dispositivo final
	bme68x_init(&dev);
	// Configuración de las mediciones del dispositivo
	conf.os_temp = BME68X_OS_8X;
	conf.os_hum = BME68X_OS_2X;
	conf.os_pres = BME68X_OS_4X;
	conf.filter = BME68X_FILTER_SIZE_3;
	conf.odr = BME68X_ODR_NONE;
	bme68x_set_conf(&conf, &dev);
	
	heatr_conf.enable = BME68X_ENABLE;
	heatr_conf.heatr_temp = (uint16_t)HEATER_TEMP;
	heatr_conf.heatr_dur = (uint16_t)HEATER_DUR;
	heatr_conf.profile_len = BME68X_DISABLE;
	heatr_conf.shared_heatr_dur = BME68X_DISABLE;
	int8_t bme680OK = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &dev);
	if(bme680OK != 0)
	{
		printf("Set Operation Error.");
	}
	
	return ;
}

uint8_t adapter_680::take_measure()
{
	uint8_t error = 0;
	uint8_t nw_measure = 0;
	
	int8_t bme680OK = bme68x_set_op_mode(BME68X_FORCED_MODE, &dev);
	if(bme680OK != 0)
	{
		error = 1;
		printf("Set Operation Error.");
		return error;
	}
	
//	uint32_t measure_time = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &dev);
//	measure_time += heatr_conf.heatr_dur * 1000;

	uint32_t measure_time = (uint32_t)bme68x_get_meas_dur( BME68X_FORCED_MODE, &conf, &dev);
	measure_time += (uint32_t)HEATER_DUR * 1000;
	delay_us(measure_time * 2, &con);
	
	bme680OK = bme68x_get_data(BME68X_FORCED_MODE, &data, &nw_measure, &dev);
	if(nw_measure != 1) //Comprueba que ha habido una nueva medición que leer.
	{
		error = 1;
		printf("Measurement not detected.\n");
		return error;
	}
	if(bme680OK != 0) //Comprueba que se ha realizado la medición correctamente.
	{	
		error = 1;
		printf("Measurement Error.\n");
		return error;
	}
	
	m.set_temperature(data.temperature);
	m.set_presure(data.pressure);
	m.set_humidity(data.humidity);
	m.set_resistance(data.gas_resistance);
		
	return error;
}

void adapter_680::show()
{
	m.show_measurements();
	
	return ;
}

int8_t adapter_680::i2c_read(uint8_t addr, uint8_t* data, uint32_t lenght, void* intf_ptr)
{
	struct context* cntx = (context*)intf_ptr;	
	uint8_t result;
	result = cyhal_i2c_master_write(cntx->i2c, cntx->address, &addr, 1, 0, false);
	result = cyhal_i2c_master_read(cntx->i2c, cntx->address, data, lenght, 0, true);
	return result;
	
}

int8_t adapter_680::i2c_write(uint8_t addr, const uint8_t* data, uint32_t length, void* intf_ptr)
{
	struct context* cntx = (context*)intf_ptr;	
	uint8_t result;
	uint8_t buffer[length + 1];
	buffer[0] = addr;
	for(uint8_t i = 0; i < length; i++)
	{buffer[i + 1] = data[i];}
	result = cyhal_i2c_master_write(cntx->i2c, cntx->address, buffer, length + 1, 0, true);
	return result;
	
}

void adapter_680::delay_us( uint32_t period, void* intf_ptr)
{
	intf_ptr = (context*)intf_ptr;
	while(period > 65000)
	{
		cyhal_system_delay_us(65000);
		period -= 65000;
	}
	cyhal_system_delay_us(period);
	
	return;
}

