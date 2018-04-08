/*
 * button.c
 *
 *  Created on: Apr 29, 2017
 *      Author: Sean Harrington
 */
#include "button.h"
#include "gpio.h"

#define BUTTON_OK_PORT


Button_State Button_Debounce(Button_State buttonState, button_t button);

static Button_State buttonOKState = UNKNOWN;
static Button_State buttonUpState = UNKNOWN;
static Button_State buttonDownState = UNKNOWN;
static Button_State buttonSTMState = UNKNOWN;

void Button_Init(void)
{
	Button_Update_States();
}

Button_State Get_Button_State(button_t button)
{
	Button_State state;
	switch(button)
	{
	case buttonOK:
		state = buttonOKState;
		break;
	case buttonUp:
		state = buttonUpState;
		break;
	case buttonDown:
		state = buttonDownState;
		break;
	case buttonSTM:
		state = buttonSTMState;
		break;
	default:
		state = UNKNOWN;
		break;
		// Error
	}
	return state;
}

void Button_Update_States(void)
{
	Button_State currentState = Button_Debounce((Button_State) HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9), buttonOK);
	if (buttonOKState != currentState)
		buttonOKState = currentState;
	else
		buttonOKState = OPEN;

	currentState = Button_Debounce((Button_State) HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8), buttonUp);
	if (buttonUpState != currentState)
		buttonUpState = currentState;
	else
		buttonUpState = OPEN;

	currentState = Button_Debounce((Button_State) HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6), buttonDown);
	if (buttonDownState != currentState)
		buttonDownState = currentState;
	else
		buttonDownState = OPEN;
}

Button_State Button_Debounce(Button_State buttonState, button_t button)
{
	static uint16_t buttonOKDebounceState = 0;
	static uint16_t buttonUpDebounceState = 0;
	static uint16_t buttonDownDebounceState = 0;
	static uint16_t buttonSTMDebounceState = 0;

	switch (button)
	{
	case buttonOK:
		buttonOKDebounceState = (buttonOKDebounceState << 1) | buttonState | 0xE000;
		if (buttonOKDebounceState == 0xF000)
			return PRESSED;
		return OPEN;

	case buttonUp:
		buttonUpDebounceState = (buttonUpDebounceState << 1) | buttonState | 0xE000;
		if (buttonUpDebounceState == 0xF000)
			return PRESSED;
		return OPEN;

	case buttonDown:
		buttonDownDebounceState = (buttonDownDebounceState << 1) | buttonState | 0xE000;
		if (buttonDownDebounceState == 0xF000)
			return PRESSED;
		return OPEN;

	case buttonSTM:
		buttonSTMDebounceState = (buttonSTMDebounceState << 1) | buttonState | 0xE000;
		if (buttonSTMDebounceState == 0xF000)
			return PRESSED;
		return OPEN;

	default:
		return UNKNOWN;
	}
}
