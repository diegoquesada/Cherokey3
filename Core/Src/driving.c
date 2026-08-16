/*
 * driving.c
 * Implementation of motor driving routines.
 *
 *  Created on: Dec 19, 2025
 *      Author: Diego Quesada 
 */

#include "stm32f3xx_hal.h"
#include "cmsis_os.h"
#include "main.h"
#include "driving.h"

extern TIM_HandleTypeDef htim4;

#define PWM_RAMP_STEP 100 // Single PWM ramp step increment in timer ticks
#define PWM_RAMP_DELAY 25 // Delay between ramp steps, in milliseconds
#define PWM_MAX_SPEED 2000 // Maximum configured PWM speed

//uint8_t pwmOn = 0;
uint8_t pwmDirection[2] = { 0, 0 }; // Motor direction, 0:forwards, 1:backwards
uint32_t pwmDuty[2]  = { 0, 0 }; // Motor duty cycle, 0:stopped, 1999:full tilt

void carInit()
{
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
}

/*
 * Logic for ramp up and ramp down in the two functions that follow.
 * At 4 KHz, each full PWM cycle is 250 microseconds. A full ramp from 0 to 2,000
 * should take us no longer than 5 seconds. We'd like to change in 5% increments,
 * which is 100 ticks of the timer or 25 milliseconds. This would require 20 iterations
 * of the ramp loop. Estimating that the ramp control code should take no more than
 * 3 microseconds to execute, that means we need the full delay of 25 milliseconds
 * in the ramp loop.
 */

/**
 * Ramps up duty cycle gradually from the current setting to a target.
 * This function does not adjust direction pins, it only sets duty cycle.
 *
 * @param Motor to adjust, 0:M1 (right), 1:M2 (left)
 * @param Target (desired) duty cycle
 * @param Timer channel to adjust
 */
void rampUpSingle(uint8_t motorIndex, uint32_t targetDuty)
{
	if (targetDuty > PWM_MAX_SPEED) // 2000 is valid as a maximum value
		return;

	TIM_OC_InitTypeDef sConfigOC = {0};
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	while (pwmDuty[motorIndex] < targetDuty)
	{
		if (targetDuty - pwmDuty[motorIndex] >= PWM_RAMP_STEP)
		{
			pwmDuty[motorIndex] += PWM_RAMP_STEP;
		}
		else
		{
			pwmDuty[motorIndex] = targetDuty;
		}

		sConfigOC.Pulse = pwmDuty[motorIndex];
		if (HAL_TIM_PWM_ConfigChannel(
				&htim4, &sConfigOC, (motorIndex == 0) ? TIM_CHANNEL_1 : TIM_CHANNEL_2) != HAL_OK)
		{
			Error_Handler();
			break;
		}

		osDelay(PWM_RAMP_DELAY);
	}
}

void rampUpSync(uint32_t targetDuty)
{
	if (targetDuty > PWM_MAX_SPEED) // 2000 is valid as a maximum value
		return;
	if (pwmDuty[0] != pwmDuty[1])
		return;

	while (pwmDuty[0] < targetDuty)
	{
		if (targetDuty - pwmDuty[0] >= PWM_RAMP_STEP)
		{
			pwmDuty[0] += PWM_RAMP_STEP;
			pwmDuty[1] += PWM_RAMP_STEP;
		}
		else
		{
			pwmDuty[0] = pwmDuty[1] = targetDuty;
		}

		__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pwmDuty[0]);
		__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, pwmDuty[1]);

		osDelay(PWM_RAMP_DELAY);
	}
}

/**
 * Ramps down duty cycle gradually from the current setting to a target.
 * This function does not adjust direction pins, it only sets duty cycle.
 *
 * @param Motor to adjust, 0:M1 (right), 1:M2 (left)
 * @param Target (desired) duty cycle
 * @param Timer channel to adjust
 */
void rampDownSingle(uint8_t motorIndex, uint32_t targetDuty)
{
	if (targetDuty > PWM_MAX_SPEED) // 2000 is valid as a maximum value
		return;

	TIM_OC_InitTypeDef sConfigOC = {0};
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	while (pwmDuty[motorIndex] > targetDuty)
	{
		if (pwmDuty[motorIndex] - targetDuty >= PWM_RAMP_STEP)
		{
			pwmDuty[motorIndex] -= PWM_RAMP_STEP;
		}
		else
		{
			pwmDuty[motorIndex] = targetDuty;
		}

		sConfigOC.Pulse = pwmDuty[motorIndex];
		if (HAL_TIM_PWM_ConfigChannel(
				&htim4, &sConfigOC, (motorIndex == 0) ? TIM_CHANNEL_1 : TIM_CHANNEL_2) != HAL_OK)
		{
			Error_Handler();
			break;
		}

		osDelay(PWM_RAMP_DELAY);
	}
}

void rampDownSync(uint32_t targetDuty)
{
	if (targetDuty > PWM_MAX_SPEED) // 2000 is valid as a maximum value
		return;
	if (pwmDuty[0] != pwmDuty[1]) // Can ony do syncd ramp if both motors are same speed
		return;

	while (pwmDuty[0] > targetDuty)
	{
		if (pwmDuty[0] - targetDuty >= PWM_RAMP_STEP)
		{
			pwmDuty[0] -= PWM_RAMP_STEP;
			pwmDuty[1] -= PWM_RAMP_STEP;
		}
		else
		{
			pwmDuty[0] = pwmDuty[1] = targetDuty;
		}

		__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, pwmDuty[0]);
		__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, pwmDuty[1]);

		osDelay(PWM_RAMP_DELAY);
	}
}

void rampTwo(uint32_t targetDuty)
{
	if (pwmDuty[0] == pwmDuty[1])
	{
		if (pwmDuty[0] < targetDuty)
			rampUpSync(targetDuty);
		else
			rampDownSync(targetDuty);
	}
	else
	{
		// Ramp up/down left motor
		if (pwmDuty[0] < targetDuty)
			rampUpSingle(0, targetDuty);
		else
			rampDownSingle(0, targetDuty);

		// Ramp up/down right motor
		if (pwmDuty[1] < targetDuty)
			rampUpSingle(1, targetDuty);
		else
			rampDownSingle(1, targetDuty);
	}
}

void rampSingle(uint8_t motorIndex, uint32_t targetDuty)
{
	if (motorIndex > 2)
		return;
	if (targetDuty > PWM_MAX_SPEED)
		return;

	if (pwmDuty[motorIndex] > targetDuty)
	{
		rampDownSingle(motorIndex, targetDuty);
	}
	else
	{
		rampUpSingle(motorIndex, targetDuty);
	}
}

/**
 * Ramps up duty cycle for both motors at the same time.
 * If changing direction, each motor will be ramped down to zero first.
 * Both motors will then be ramped up together to the target speed,
 * in steps of 100 units at PWM_RAMP_DELAY (default 25 ms) intervals.
 */
void carAdvance(uint8_t direction, uint32_t speed)
{
	if (direction > 1) // 0, 1 allowed
		return;
	if (speed > PWM_MAX_SPEED) // 2000 is valid as a maximum value
		return;

	// Changing direction - ramp down M1
	if (direction != pwmDirection[0] && pwmDuty[0] != 0)
	{
		rampDownSingle(0, 0);
	}

	// Changing direction - ramp down M2
	if (direction != pwmDirection[1] && pwmDuty[1] != 0)
	{
		rampDownSingle(1, 0);
	}

	// Set direction pins
	HAL_GPIO_WritePin(PIN_MOTOR1_GPIO_Port, PIN_MOTOR1_Pin, (direction == 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	HAL_GPIO_WritePin(PIN_MOTOR2_GPIO_Port, PIN_MOTOR2_Pin, (direction == 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	pwmDirection[0] = pwmDirection[1] = direction;

	rampTwo(speed);
}

void carTurn(uint8_t direction, uint32_t speed)
{
	if (direction > 3) // 0-3 allowed
		return;
	if (speed > PWM_MAX_SPEED) // 2000 is valid as a maximum value
		return;

	// Future improvement: allow ramp up or ramp down of motors here in any direction,
	// and not just the motor that will be on for the turn. This would allow calling
	// this function without having to stop the car.

	switch (direction)
	{
	case 0: // forward-left, M1 on
		if (pwmDirection[0] != 0 && pwmDuty[0] != 0) rampDownSingle(0, 0); // If changing direction, ramp down M2
		HAL_GPIO_WritePin(PIN_MOTOR1_GPIO_Port, PIN_MOTOR1_Pin, GPIO_PIN_SET);
		rampUpSingle(0, speed);
		break;
	case 1: // backward-left, M1 on
		if (pwmDirection[0] != 1 && pwmDuty[0] != 0) rampDownSingle(0, 0); // If changing direction, ramp down M1
		HAL_GPIO_WritePin(PIN_MOTOR1_GPIO_Port, PIN_MOTOR1_Pin, GPIO_PIN_RESET);
		rampUpSingle(0, speed);
		break;
	case 2: // forward-right, M2 on
		if (pwmDirection[1] != 0 && pwmDuty[1] != 0) rampDownSingle(1, 0); // If changing direction, ramp down M2
		HAL_GPIO_WritePin(PIN_MOTOR2_GPIO_Port, PIN_MOTOR2_Pin, GPIO_PIN_SET);
		rampUpSingle(1, speed);
		break;
	case 3: // backward-right, M2 on
		if (pwmDirection[1] != 1 && pwmDuty[1] != 0) rampDownSingle(1, 0); // If changing direction, ramp down M1
		HAL_GPIO_WritePin(PIN_MOTOR2_GPIO_Port, PIN_MOTOR2_Pin, GPIO_PIN_RESET);
		rampUpSingle(1, speed);
		break;
	}
}

void carStop()
{
	if (pwmDuty[0] == pwmDuty[1])
	{
		rampDownSync(0);
	}
	else
	{
		rampDownSingle(0, 0);
		rampDownSingle(1, 0);
	}

	// No changes to the pins.
}

