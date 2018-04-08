/*
 * pid.h
 *
 *  Created on: May 1, 2017
 *      Author: Sean Harrington
 */

#ifndef PID_H_
#define PID_H_

void PID_Init(uint8_t pConstant, uint8_t iConstant, uint8_t dConstant);

float PID_Update(float desiredTemp, float actualTemp);

#endif /* PID_H_ */
