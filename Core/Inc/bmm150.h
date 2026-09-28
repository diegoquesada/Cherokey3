/*
 * bmm150.h
 *
 *  Created on: Sep 26, 2026
 *      Author: diegoq
 */

#ifndef INC_BMM150_H_
#define INC_BMM150_H_

#include "bmm150_defs.h"

int8_t bmm150Init(I2C_HandleTypeDef *hi2c);
int8_t bmm150SetOpMode(const struct bmm150_settings *settings);
int8_t bmm150SetPresetMode(struct bmm150_settings *settings);
int8_t bmm150SetSensorSettings(uint16_t desiredSettings, const struct bmm150_settings *settings);

int8_t bmm150GetGeomagneticData();
int8_t bmm150GetCompassDegree(float *compassDegree);

#endif /* INC_BMM150_H_ */
