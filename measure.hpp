/*
 * measure.hpp
 *
 *  Created on: 28 may 2026
 *      Author: kike
 */

#ifndef MEASURE_HPP_
#define MEASURE_HPP_

#include "stdio.h"

class measure
{
	float temperature;
	float humidity;
	float pressure;
	float air_resistance;
	
	public:
	
	measure();
	void set_temperature(float t);
	void set_humidity(float h);		  
	void set_presure(float p);		  
	void set_resistance(float r);		  
		  
	float get_temperature();
	float get_humidity();
	float get_pressure();
	float get_resistance();
	
	void show_measurements();

};


#endif /* MEASURE_HPP_ */
