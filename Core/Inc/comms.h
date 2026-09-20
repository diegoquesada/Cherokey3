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

/**
 * Initialize state of the ESP8266 library.
 */
void espInit(UART_HandleTypeDef *huartESP, UART_HandleTypeDef *huartEcho);

/**
 * Enable and initialize ESP8266.
 */
esp_status_t espStart();

esp_status_t espSendSync(const uint8_t *cmd, uint32_t timeout, uint8_t echo);
void espRxCpltCallback(UART_HandleTypeDef *huart);
void espRecoverUart();

esp_status_t espOpenSocket();
esp_status_t espSendSocket(const uint8_t *data, uint16_t len);
esp_status_t espCloseSocket();

#endif /* INC_COMMS_H_ */
