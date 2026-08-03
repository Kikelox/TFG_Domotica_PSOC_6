/*
 * measure.cpp
 *
 *  Created on: 28 may 2026
 *      Author: kiker
 */

#include "measure.hpp"
#include <cstdio>

measure::measure()
{
	temperature = 0.0f;
	pressure = 0.0f;
	humidity = 0.0f;
	air_resistance = 0.0f;
}

void measure::set_temperature(float t)
{ temperature = t; }

void measure::set_presure(float p)
{ pressure = p; }

void measure::set_humidity(float h)
{ humidity = h; }

void measure::set_resistance(float r)
{ air_resistance = r; }

float measure::get_temperature()
{ return temperature; }

float measure::get_pressure()
{ return pressure; }

float measure::get_humidity()
{ return humidity; }

float measure::get_resistance()
{ return air_resistance; }

void measure::show_measurements()
{
	printf("\nLas mediciones son las siguientes:\n");
	printf("Temperatura: %.2f ºC\t", temperature);
	printf("Humedad: %.2f %%\t", humidity);
	printf("Presión: %.2f Pascales\t", pressure);
	printf("Resistencia del aire %.2f Ohmios\n", air_resistance);
	return ;
}


