/*
 * MAX31855.c
 *
 *  Created on: May 3, 2017
 *      Author: Sean Harrington
 */
#include "gpio.h"
#include "spi.h"
#include "MAX31855.h"
#include <math.h>

static uint16_t tempRaw = 0;
static uint16_t tempInternalRaw = 0;

uint16_t MAX31855_ReadData(void)
{
  uint8_t pDataRx[4] = { };

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
  HAL_SPI_Receive(&hspi2, (uint8_t*) pDataRx, 4, 1000);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

  tempRaw = ((pDataRx[0] << 8) | pDataRx[1]);
  tempInternalRaw = ((pDataRx[2] << 8 | pDataRx[3]));
  return tempRaw;
}

float MAX31855_GetTemp(void)
{
	uint16_t temperatureData = tempRaw >> 2;
	float remainder = 0.25 * (temperatureData & (0x3));
	float temperature = (float)(temperatureData >> 2) + (float)(remainder);
	return temperature;
}

float MAX31855_GetInternalTemp(void)
{
	uint16_t temperatureData = tempInternalRaw >> 4;
	float remainder = 0.0625 * (temperatureData & (0xF));
	float temperature = (float)(temperatureData >> 4) + (float)(remainder);
	return temperature;
}

void MAX31855_GetErrors(uint16_t rawData)
{

}


float MAX31855_CorrectedTemp(float internalTemp, float rawTemp)
{

	float thermoVoltage = 0;
	float internalVoltage = 0;
	float correctedTemp = 0;

	thermoVoltage = (rawTemp - internalTemp) * 0.041276; // C * mv/C -> mv

	if (internalTemp >= 0)
	{
		float c[] = {-0.176004136860E-01,  0.389212049750E-01,  0.185587700320E-04, -0.994575928740E-07,  0.318409457190E-09, -0.560728448890E-12,  0.560750590590E-15, -0.320207200030E-18,  0.971511471520E-22, -0.121047212750E-25};

        // Exponential coefficients. Only used for positive temperatures.
        float a0 =  0.118597600000E+00;
        float a1 = -0.118343200000E-03;
        float a2 =  0.126968600000E+03;

        uint8_t cLength = 10;
        // From NIST: E = sum(i=0 to n) c_i t^i + a0 exp(a1 (t - a2)^2), where E is the thermocouple voltage in mV and t is the temperature in degrees C.
        // In this case, E is the cold junction equivalent thermocouple voltage.
		// Alternative form: C0 + C1*internalTemp + C2*internalTemp^2 + C3*internalTemp^3 + ... + C10*internaltemp^10 + A0*e^(A1*(internalTemp - A2)^2)
		// This loop sums up the c_i t^i components.
		for (uint8_t i = 0; i < cLength; i++) {
			internalVoltage += c[i] * powf(internalTemp, i);
		}
		// This section adds the a0 exp(a1 (t - a2)^2) components.
		internalVoltage += a0 * expf(a1 * powf((internalTemp - a2), 2));
	}

	else if (internalTemp < 0) { // for negative temperatures
		float c[] = {0.000000000000E+00,  0.394501280250E-01,  0.236223735980E-04 - 0.328589067840E-06, -0.499048287770E-08, -0.675090591730E-10, -0.574103274280E-12, -0.310888728940E-14, -0.104516093650E-16, -0.198892668780E-19, -0.163226974860E-22};
		uint8_t cLength = 11;

		// Below 0 degrees Celsius, the NIST formula is simpler and has no exponential components: E = sum(i=0 to n) c_i t^i
		for (uint8_t i = 0; i < cLength; i++) {
			internalVoltage += c[i] * powf(internalTemp, i) ;
		}
	}

	// Step 4. Add the cold junction equivalent thermocouple voltage calculated in step 3 to the thermocouple voltage calculated in step 2.
	float totalVoltage = thermoVoltage + internalVoltage;

	// Step 5. Use the result of step 4 and the NIST voltage-to-temperature (inverse) coefficients to calculate the cold junction compensated, linearized temperature value.
	// The equation is in the form correctedTemp = d_0 + d_1*E + d_2*E^2 + ... + d_n*E^n, where E is the totalVoltage in mV and correctedTemp is in degrees C.
	// NIST uses different coefficients for different temperature subranges: (-200 to 0C), (0 to 500C) and (500 to 1372C).
	if (totalVoltage < 0) { // Temperature is between -200 and 0C.
		float d[] = {0.0000000E+00, 2.5173462E+01, -1.1662878E+00, -1.0833638E+00, -8.9773540E-01, -3.7342377E-01, -8.6632643E-02, -1.0450598E-02, -5.1920577E-04, 0.0000000E+00};
		uint8_t dLength = 10;
		for (uint8_t i = 0; i < dLength; i++) {
		   correctedTemp += d[i] * powf(totalVoltage, i);
		}
	}
	else if (totalVoltage < 20.644) { // Temperature is between 0C and 500C.
		float d[] = {0.000000E+00, 2.508355E+01, 7.860106E-02, -2.503131E-01, 8.315270E-02, -1.228034E-02, 9.804036E-04, -4.413030E-05, 1.057734E-06, -1.052755E-08};
		uint8_t dLength = 10;
		for (uint8_t i = 0; i < dLength; i++) {
			correctedTemp += d[i] * powf(totalVoltage, i);
		}
	}
	else if (totalVoltage < 54.886 ) { // Temperature is between 500C and 1372C.
		float d[] = {-1.318058E+02, 4.830222E+01, -1.646031E+00, 5.464731E-02, -9.650715E-04, 8.802193E-06, -3.110810E-08, 0.000000E+00, 0.000000E+00, 0.000000E+00};
		uint8_t dLength = 10;

		for (uint8_t i = 0; i < dLength; i++) {
			correctedTemp += d[i] * powf(totalVoltage, i);
		}
	}
	else { // NIST only has data for K-type thermocouples from -200C to +1372C. If the temperature is not in that range, set temp to impossible value.
		correctedTemp = 10000;
	}


	return correctedTemp;

}

