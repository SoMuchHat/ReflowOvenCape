/*
 * pid.c
 *
 *  Created on: May 1, 2017
 *      Author: Sean Harrington
 */

#include "stm32f3xx_hal.h"
#include "main.h"
#include "pid.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// P constant
static uint8_t k_p = 0;

// I constant
static uint8_t k_i = 0;

// D constant
static uint8_t k_d = 0;

static float lastError = 0;

static float integral = 0;

void PID_Init(uint8_t pConstant, uint8_t iConstant, uint8_t dConstant)
{
	k_p = pConstant;
	k_i = iConstant;
	k_d = dConstant;
}

float PID_Update(float desiredTemp, float actualTemp)
{
	float error = desiredTemp - actualTemp;

	integral += lastError;
	if (integral > 50)
	{
		integral = 50.0;
	}
	else if (integral < -50)
	{
		integral = -50.0;
	}
	float derivative = error - lastError;
	float dutyCycle = (k_p * error) + (k_i * integral) + (k_d * derivative);

	dutyCycle = MAX(MIN(100.0, dutyCycle), 0.0);
	lastError = error;

	return dutyCycle;
}

