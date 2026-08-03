/*
 * adapter_680.hpp
 *
 *  Created on: 28 may 2026
 *      Author: kiker
 */

#ifndef ADAPTER_680_HPP_
#define ADAPTER_680_HPP_

#include"bme68x.hpp"
#include"bme68x_defs.hpp"
#include "cyhal.h"
#include "cyhal_i2c.h"
#include "cyhal_hw_types.h"
#include <cstdint>
#include "measure.hpp"

struct context
{
	cyhal_i2c_t* i2c;
	uint8_t address;
};

class adapter_680
{
	measure m;
	struct context con;
	struct bme68x_dev dev;
	struct bme68x_data data;
	struct bme68x_conf conf;
	struct bme68x_heatr_conf heatr_conf;
	static int8_t i2c_read(uint8_t addr, uint8_t* data, uint32_t lenght, void* intf_ptr);
	static int8_t i2c_write(uint8_t addr, const uint8_t* data, uint32_t lenght, void* intf_ptr);
	static void delay_us(uint32_t period, void* intf_ptr);

	public:
	adapter_680(cyhal_i2c_t* i,  uint8_t ad = 0x76);
	void init();
	uint8_t take_measure();
	void show();
	

};

#endif /* ADAPTER_680_HPP_ */
