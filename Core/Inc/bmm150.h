/*
 * bmm150.h
 *
 *  Created on: Sep 26, 2026
 *      Author: diegoq
 */

#ifndef INC_BMM150_H_
#define INC_BMM150_H_

typedef enum {
    BMM150_SUCCESS,
	BMM150_ERROR,
	BMM150_TIMEOUT,
	BMM150_UNKNOWN
} bmm150_status_t;

#define BMM150_I2C_ADDRESS_CSB_HIGH_SDO_HIGH      UINT8_C(0x13)
#define BMM150_POWERMODE_NORMAL                   UINT8_C(0x00)

int8_t bmm150Init(I2C_HandleTypeDef *hi2c);
int8_t bmm150SetOpMode(uint8_t powerMode);

#endif /* INC_BMM150_H_ */
