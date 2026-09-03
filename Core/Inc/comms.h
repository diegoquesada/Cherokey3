/*
 * comms.h
 *
 *  Created on: Sep 2, 2026
 *      Author: diegoq
 */

#ifndef INC_COMMS_H_
#define INC_COMMS_H_

#include <stdint.h>
#include "stm32f3xx_hal.h"

typedef enum {
    ESP_SUCCESS,
    ESP_ERROR,
    ESP_TIMEOUT,
    ESP_UNKNOWN
} esp_status_t;

void espInit();
void espStart(UART_HandleTypeDef *huart, UART_HandleTypeDef *huartEcho);
esp_status_t espSendSync(UART_HandleTypeDef *huart, UART_HandleTypeDef *huartEcho, const uint8_t *cmd);

#endif /* INC_COMMS_H_ */
