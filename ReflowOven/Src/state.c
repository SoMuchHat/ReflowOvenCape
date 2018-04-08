/*
 * state.c
 *
 *  Created on: Apr 29, 2017
 *      Author: Sean Harrington
 */

#include "state.h"
#include "button.h"
#include "spi.h"
#include "usart.h"
#include "tim.h"
#include "MAX31855.h"
#include "ReflowCurves.h"
#include "pid.h"
#include "heater.h"

#define HEARTBEAT_MESSAGE	(0x10 | reflowControllerState)
#define DEBUG_MESSAGE		(0xD0 | reflowControllerState)
#define TRUE				1
#define FALSE				0

#define DEBUG_MODE			1


// Bitmask of states the system can be in
typedef enum
{
	INIT,
	STANDBY,
	CONFIG,
	PREHEAT,
	REFLOW,
	STATE_ERROR
} state_t;

extern TIM_HandleTypeDef htim1;
extern uint8_t timeElapsed;

static state_t reflowControllerState = INIT;
static Button_State buttonOKState = UNKNOWN;
static Button_State buttonUpState = UNKNOWN;
static Button_State buttonDownState = UNKNOWN;

// Holds the data that will be sent to the control app
static uint8_t dataToApp[3] = {0, 0, 0};

// Holds the data that was received by the control app
static uint8_t dataFromApp[3] = {0, 0, 0};

//
static uint8_t reflowStart = FALSE;

// The amount of reflow time that has elapsed in half seconds
static uint16_t reflowTime = 0;

static uint16_t triggerCount = 0;

void Update_State(void)
{
	/*if (HAL_OK == HAL_UART_Receive(&huart1, (uint8_t *)dataFromApp, 3, 10))
	{
		if (dataFromApp[0] == 0x8C)
		{
			reflowControllerState = REFLOW;
		}
	}*/
	switch(reflowControllerState)
	{
		case INIT:
			HEATERS_OFF;
			buttonOKState = Get_Button_State(buttonOK);
			buttonUpState = Get_Button_State(buttonUp);
			buttonDownState = Get_Button_State(buttonDown);

			PID_Init(15,2,0);
			HAL_TIM_Base_Start_IT(&htim1);

			reflowControllerState = STANDBY;
			break;	// Could realistically remove this break as we want to enter standby

		case STANDBY:
			HEATERS_OFF;
			buttonOKState = Get_Button_State(buttonOK);
			if (buttonOKState == PRESSED)
			{
				reflowControllerState = PREHEAT;
				reflowStart = TRUE;
			}

			if (timeElapsed == 1)
			{
				timeElapsed = 0;

				uint16_t temperatureData = MAX31855_ReadData();
				dataToApp[0] = HEARTBEAT_MESSAGE;
				dataToApp[1] = (temperatureData & 0xFF00) >> 8;
				dataToApp[2] = (temperatureData & 0x00FF) | ((reflowControllerState == STATE_ERROR) << 1);
				HAL_UART_Transmit(&huart1, (uint8_t *)dataToApp, 3, 0xFFFF);
			}
			break;

		case CONFIG:

			break;

		case PREHEAT:
			if (timeElapsed == 1)
			{
				HEATERS_ON;
				timeElapsed = 0;

				// Check to see if preheating required
				// Read data from MAX31855 and convert to temperature in degrees C
				uint16_t rawData = MAX31855_ReadData();
				float actualTemp = MAX31855_GetTemp();
				float internalTemp = MAX31855_GetInternalTemp();
				float correctedTemp = MAX31855_CorrectedTemp(internalTemp, actualTemp);
				correctedTemp += 100.0;
				// Stop preheat if temperature is greater than 30 degrees C
				if (actualTemp >= 25.0)
				{
					reflowControllerState = REFLOW;
				}

				dataToApp[0] = HEARTBEAT_MESSAGE;
				dataToApp[1] = (rawData & 0xFF00) >> 8;
				dataToApp[2] = (rawData & 0x00FF) | ((reflowControllerState == STATE_ERROR) << 1);
				HAL_UART_Transmit(&huart1, (uint8_t *)dataToApp, 3, 0xFFFF);
			}

			buttonOKState = Get_Button_State(buttonOK);
			if (buttonOKState == PRESSED)
			{
				reflowControllerState = STANDBY;
			}

			break;
		case REFLOW:
			// If we are just starting a reflow, there's additional work to do.
			if (reflowStart == TRUE) {
				// Send reflow start command to app
				// Reset timer
				htim1.Instance->CNT = 0;
				// Reset current time of reflow process
				reflowTime = 0;

			}
			// If count is 500, it's been half a second, resample and transmit
			if (timeElapsed == 1)
			{
				timeElapsed = 0;
				// Increment total reflow time
				reflowTime++;

				// Lookup current desired temperature
				float desiredTemp = ReflowCurves_GetDesiredTempLeaded(reflowTime);

				// Read data from MAX31855 and convert to temperature in degrees C
				uint16_t rawData = MAX31855_ReadData();
				float actualTemp = MAX31855_GetTemp();

				// Send current measured temperature
				dataToApp[0] = HEARTBEAT_MESSAGE;
				dataToApp[1] = (rawData & 0xFF00) >> 8;
				dataToApp[2] = (rawData & 0x00FF) | ((reflowControllerState == STATE_ERROR) << 1);
				HAL_UART_Transmit(&huart1, (uint8_t *)dataToApp, 3, 0xFFFF);

				if (DEBUG_MODE)
				{
					dataToApp[0] = DEBUG_MESSAGE;
					dataToApp[1] = (uint8_t) ((uint16_t)(desiredTemp) >> 8);
					dataToApp[2] = (uint8_t) ((uint16_t)desiredTemp & 0x00FF);
					HAL_UART_Transmit(&huart1, (uint8_t *)dataToApp, 3, 0xFFFF);

				}

				// Run PID controller, get duty cycle for next half second for when heaters will be on
				float dutyCycle = PID_Update(desiredTemp, actualTemp);
				triggerCount = TIMER_PERIOD * (dutyCycle/100.0);

				// Heaters on!
				HEATERS_ON;
			}
			else if(htim1.Instance->CNT >= triggerCount)
			{
				// Duty cycle has been met, turn off heaters for remaining period
				HEATERS_OFF;
			}

			//uint16_t tempData = MAX31855_readData();
			//dataToApp[0] =
			//HAL_UART_Transmit(&huart1, (uint8_t *)tempData, 3, 0xFFFF);
			//for (int i = 0; i < 1000000; i++);
			buttonOKState = Get_Button_State(buttonOK);
			if (buttonOKState == PRESSED)
			{
				reflowControllerState = STANDBY;
			}

			reflowStart = FALSE;
			break;

		case STATE_ERROR:

			break;

		default:
			reflowControllerState = ERROR;
	}
}
