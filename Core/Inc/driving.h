/*
 * driving.h
 *
 *  Created on: Dec 19, 2025
 *      Author: diegoq
 */

#ifndef DRIVING_H_
#define DRIVING_H_

#include <stdint.h>

#define CAR_RIGHT_MOTOR 0
#define CAR_LEFT_MOTOR 1
#define CAR_HALF_SPEED 999
#define CAR_FULL_SPEED 1999
#define CAR_FORWARD 0
#define CAR_BACKWARD 1

void carInit();

/**
 * Ramps down duty cycle gradually from the current setting to a target.
 * This function does not adjust direction pins, it only sets duty cycle.
 *
 * @param Motor to adjust, CAR_RIGHT_MOTOR:M1, CAR_LEFT_MOTOR:M2
 * @param Target (desired) duty cycle
 * @param Timer channel to adjust
 */
void rampSingle(uint8_t motorIndex, uint32_t targetDuty);

/**
 * Move car forwards or backwards.
 * @param direction	0: forward, 1: backward
 * @param speed		0-1999, with 0: stop and 1999: full speed
 */
void carAdvance(uint8_t direction, uint32_t speed);

/**
 * Turn the car.
 * @param direction	0: forward-left, 1: backward-left, 2: forward-right, 3:backward-right
 */
void carTurn(uint8_t direction, uint32_t turnSpeed);

/**
 * Stop the car.
 */
void carStop();


#endif /* DRIVING_H_ */
