/*
 * st7032.hpp
 *
 *  Created on: 18 nov 2025
 *      Author: kiker
 */

#ifndef ST7032_HPP_
#define ST7032_HPP_

#include "cyhal_i2c.h"
#include <stdint.h>

class ST7032 
{
	cyhal_i2c_t *i2c;
	uint8_t i2c_addr;
	void send(uint8_t byte, uint8_t data);
	
		public:
	ST7032(cyhal_i2c_t *i2c_instance, uint8_t address = 0x3E);

	void init();
	void clear();
	void setCursor(uint8_t row, uint8_t col);
	void print(const char *text);
	void command(uint8_t cmd);

};

#endif /* ST7032_HPP_ */
