/*
 * ReflowCurves.c
 *
 *  Created on: May 3, 2017
 *      Author: Sean Harrington
 */

#define LOOKUP_SIZE	(7)
#include "stm32f3xx_hal.h"
#include "main.h"
#include "ReflowCurves.h"

typedef struct
{
	uint16_t time;
	float temp;
} timeToTemp_t;


// Leaded solder paste lookup table
static timeToTemp_t leadedLookup[LOOKUP_SIZE] =
{
		{0, 23},
		{180, 150},
		{360, 180},
		{400, 210},
		{440, 210},
		{480, 180},
		{540, 25}
};

// Lead-free solder paste lookup table
static timeToTemp_t leadFreeLookup[LOOKUP_SIZE] =
{
		{0, 23},
		{180, 150},
		{360, 217},
		{400, 240},
		{440, 240},
		{480, 217},
		{540, 25}
};

float ReflowCurves_GetDesiredTempLeaded(uint16_t halfSecond)
{
	for(uint8_t i = 0; i < (LOOKUP_SIZE - 1); i++)
	{
		if (halfSecond >= leadedLookup[i].time && halfSecond < leadedLookup[i + 1].time)
		{
			uint16_t diffTime = halfSecond - leadedLookup[i].time;
			uint16_t diffRange = leadedLookup[i+1].time - leadedLookup[i].time;

			return leadedLookup[i].temp + (leadedLookup[i+1].temp - leadedLookup[i].temp) * ((float) diffTime/ (float)diffRange);
		}
	}

	// We should never get here.. if we do, return 0 so heaters turn off
	return 0.0;
}

float ReflowCurves_GetDesiredTempLeadFree(uint16_t halfSecond)
{
	for(uint8_t i = 0; i < (LOOKUP_SIZE - 1); i++)
	{
		if (halfSecond >= leadFreeLookup[i].time && halfSecond < leadFreeLookup[i + 1].time)
		{
			uint16_t diffTime = halfSecond - leadFreeLookup[i].time;
			uint16_t diffRange = leadFreeLookup[i+1].time - leadFreeLookup[i].time;

			return leadFreeLookup[i].temp + (leadFreeLookup[i+1].temp - leadFreeLookup[i].temp) * ((float) diffTime/ (float)diffRange);
		}
	}

	// We should never get here.. if we do, return 0 so heaters turn off
	return 0.0;
}
