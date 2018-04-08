/*
 * heater.h
 *
 *  Created on: May 3, 2017
 *      Author: Sean Harrington
 */

#ifndef HEATER_H_
#define HEATER_H_

#define HEATERS_ON { \
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_SET); \
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_SET); \
	}


#define HEATERS_OFF { \
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET); \
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET); \
	}

#endif /* HEATER_H_ */
