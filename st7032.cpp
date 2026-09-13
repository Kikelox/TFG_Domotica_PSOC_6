/*
 * st7032.cpp
 *
 *  Created on: 18 nov 2025
 *      Author: kiker
 */

#ifndef ST7032_CPP_
#define ST7032_CPP_

#include "st7032.hpp"
#include "cy_syslib.h"
#include "cyhal_i2c.h"
#include <string.h>
#include "cyhal.h"
#include "cy_pdl.h"
#include "cyhal_system_impl.h"

ST7032::ST7032(cyhal_i2c_t *i2c_instance, uint8_t address)
{
	i2c = i2c_instance;
	i2c_addr = address;
}

void ST7032::command(uint8_t cmd)
{
	send(0x00, cmd); // 0x00 para comandos
}

void ST7032::send(uint8_t byte, uint8_t data)
{
	uint8_t pack[2] = { byte, data };
	cyhal_i2c_master_write(i2c, i2c_addr, pack, 2, 0, true);
}

void ST7032::init()
{
	Cy_SysLib_Delay(50);
	command(0x38); // Basic Configuration
	command(0x39); // IS = 1 Advanced configuration Mode
	command(0x14); // Internal Clock Set
	command(0x73); // Contrast set
	command(0x56); // Power/Icon/Contrast control
	command(0x6C); // Voltage Follower 
	Cy_SysLib_Delay(200); // Voltage Follower Slow Command
	command(0x38); // IS = 0 Basic Mode
	command(0x0C); // Display ON, CURSOR OFF, BLINK OFF
	command(0x01); // Clear display
	Cy_SysLib_Delay(50); // Clear Display Slow Command
	
}

void ST7032::clear()
{
	command(0x01);
	Cy_SysLib_Delay(50);
}

void ST7032::setCursor(uint8_t row, uint8_t col)
{
	uint8_t addr = col + (row == 0 ? 0x00 : 0x40); // 0x00 fila 1, 0x40 fila 2
	command(0x80 | addr);
}

void ST7032::print(const char *text)
{
	while (*text != '\0') 
	{
		send(0x40, *text++);
	}
}

#endif /* ST7032_CPP_ */


