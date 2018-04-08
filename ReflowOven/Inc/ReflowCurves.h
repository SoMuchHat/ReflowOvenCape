/*
 * ReflowCurves.h
 *
 *  Created on: May 3, 2017
 *      Author: Sean Harrington
 */

#ifndef REFLOWCURVES_H_
#define REFLOWCURVES_H_

/*
 * Given the current time in the reflow process in half seconds, this function
 * returns the current desired temperature for leaded solder paste
 *
 */
float ReflowCurves_GetDesiredTempLeaded(uint16_t halfSecond);

/*
 * Given the current time in the reflow process in half seconds, this function
 * returns the current desired temperature for lead-free solder paste
 */
float ReflowCurves_GetDesiredTempLeadFree(uint16_t halfSecond);

#endif /* REFLOWCURVES_H_ */
