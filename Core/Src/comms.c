/*
 * comms.c
 *
 *  Created on: Sep 2, 2026
 *      Author: diegoq
 */

#include <string.h>
#include <stdio.h>
#include "cmsis_os.h"
#include "comms.h"
#include "secrets.h"

extern osSemaphoreId_t uart4RxSemHandle;
uint8_t uartTxBuffer[64], uartRxBuffer[64];

void espInit(UART_HandleTypeDef *huart, UART_HandleTypeDef *huartEcho)
{
	const uint8_t mode_cmd[] = "AT+CWMODE=1\r\n";
	espSendSync(huart, 0, mode_cmd);

	const uint8_t querymac_cmd[] = "AT+CIPSTAMAC?";
	espSendSync(huart, huartEcho, querymac_cmd);

	const char connect_cmd[] = "AT+CWJAP=\"Delphi\",\"%s\"\r\n";
	snprintf((char *)uartTxBuffer, sizeof(uartTxBuffer), connect_cmd, WIFI_PASSWORD);
	espSendSync(huart, 0, uartTxBuffer);

	const uint8_t queryip_cmd[] = "AT+CIPSTA?";
	espSendSync(huart, huartEcho, queryip_cmd);
}

uint16_t espSendSync(UART_HandleTypeDef *huart, UART_HandleTypeDef *huartEcho, const uint8_t *cmd)
{
	// Send AT command to the ESP8266
	HAL_UART_Transmit(huart, cmd, strlen((const char *)cmd), 100);

	// Start asynchronous DMA receive.
	HAL_UART_Receive_DMA(huart, uartRxBuffer, sizeof(uartRxBuffer));
	uint16_t received_len = 0;
	if (osSemaphoreAcquire(uart4RxSemHandle, 500) == osOK)
	{
		received_len = sizeof(uartRxBuffer);
	}
	else
	{
		// Timeout occurred. Calculate how many bytes actually arrived.
		received_len = sizeof(uartRxBuffer) - huart->RxXferCount;
	}

	if (huartEcho != 0 && received_len > 0)
	{
		HAL_UART_Transmit(huartEcho, uartRxBuffer, received_len, 100);
	}

	return received_len;
/*	else
	{
		const uint8_t error_str[] = "Nothing\r\n";
		HAL_UART_Transmit(&huart2, error_str, 9, 100);
	}*/
}
