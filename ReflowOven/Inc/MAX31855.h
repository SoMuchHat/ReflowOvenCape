/*
 * MAX31855.h
 *
 *  Created on: May 3, 2017
 *      Author: Sean Harrington
 */

#ifndef MAX31855_H_
#define MAX31855_H_

uint16_t MAX31855_ReadData(void);

float MAX31855_GetTemp(void);

float MAX31855_GetInternalTemp(void);

float MAX31855_CorrectedTemp(float internalTemp, float rawTemp);

#endif /* MAX31855_H_ */
