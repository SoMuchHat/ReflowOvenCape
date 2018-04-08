/*
 * button.h
 *
 *  Created on: Apr 29, 2017
 *      Author: Sean Harrington
 */

#ifndef BUTTON_H_
#define BUTTON_H_

typedef enum
{
	buttonOK,
	buttonUp,
	buttonDown,
	buttonSTM
} button_t;

typedef enum
{
	OPEN = 0U,
	PRESSED,
	UNKNOWN
} Button_State;

void Button_Init(void);

Button_State Get_Button_State(button_t button);

void Button_Update_States(void);

#endif /* BUTTON_H_ */
