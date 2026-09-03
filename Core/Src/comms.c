/*
 * comms.c
 *
 *  Created on: Sep 2, 2026
 *      Author: diegoq
 */

#include <string.h>
#include <stdio.h>
#include "cmsis_os.h"
#include "main.h"
#include "comms.h"
#include "secrets.h"

osSemaphoreId_t uart4RxSemHandle;
#define ESP_RX_BUFFERSIZE 64
uint8_t uartTxBuffer[64], uartRxBuffer[ESP_RX_BUFFERSIZE];
uint16_t espDmaLastPos = 0;

esp_status_t espWaitReady(UART_HandleTypeDef *huart, UART_HandleTypeDef *huartEcho);

void espInit()
{
	uart4RxSemHandle = osSemaphoreNew(1, 0, NULL);
}

void espStart(UART_HandleTypeDef *huart, UART_HandleTypeDef *huartEcho)
{
	espWaitReady(huart, huartEcho);

	const uint8_t at_cmd[] = "AT\r\n";
	if (espSendSync(huart, 0, at_cmd) != ESP_SUCCESS)
		return;

	const uint8_t mode_cmd[] = "AT+CWMODE=1\r\n";
	if (espSendSync(huart, 0, mode_cmd) != ESP_SUCCESS)
		return;

	const uint8_t querymac_cmd[] = "AT+CIPSTAMAC?\r\n";
	if (espSendSync(huart, huartEcho, querymac_cmd) != ESP_SUCCESS)
		return;

	const char connect_cmd[] = "AT+CWJAP=\"Delphi\",\"%s\"\r\n";
	snprintf((char *)uartTxBuffer, sizeof(uartTxBuffer), connect_cmd, WIFI_PASSWORD);
	espSendSync(huart, 0, uartTxBuffer);

	const uint8_t queryip_cmd[] = "AT+CIPSTA?\r\n";
	espSendSync(huart, huartEcho, queryip_cmd);
}

esp_status_t espWaitReady(UART_HandleTypeDef *huart, UART_HandleTypeDef *huartEcho)
{
	memset(uartRxBuffer, 0, sizeof(uartRxBuffer));

	// Trigger data reception via DMA. We will be alerted via Idle interrupt.
	HAL_UART_Receive_DMA(huart, uartRxBuffer, sizeof(uartRxBuffer));
	__HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);

	// Raise enable pin to turn on ESP module.
	HAL_GPIO_WritePin(ESP_ENABLE_GPIO_Port, ESP_ENABLE_Pin, GPIO_PIN_SET);

	for (;;)
	{
		if (osSemaphoreAcquire(uart4RxSemHandle, 100) == osOK)
		{
			// Match the string "ready" in the module's output.
			uint16_t pos = ESP_RX_BUFFERSIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);
			if (strnstr((const char *)uartRxBuffer + espDmaLastPos, "ready\r\n", pos - espDmaLastPos) != 0)
			{
				HAL_UART_Transmit(huartEcho, uartRxBuffer + espDmaLastPos, pos - espDmaLastPos, 100);

				espDmaLastPos = pos;
				return ESP_SUCCESS;
			}
		}
	}
}

#define min(x, y) (((x) < (y)) ? (x) : (y))

esp_status_t espSendSync(UART_HandleTypeDef *huart, UART_HandleTypeDef *huartEcho, const uint8_t *cmd)
{
	// Send AT command to the ESP8266
	HAL_UART_Transmit(huart, cmd, strlen((const char *)cmd), 100);

	// Start asynchronous DMA receive.
	HAL_UART_Receive_DMA(huart, uartRxBuffer, sizeof(uartRxBuffer));
	__HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);

	char responseBuffer[64];
	for (;;)
	{
		if (osSemaphoreAcquire(uart4RxSemHandle, 100) == osOK)
		{
			uint16_t pos = ESP_RX_BUFFERSIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);
			uint16_t len = 0;
			if (pos > espDmaLastPos)
			{
				len = pos - espDmaLastPos;
				memcpy(responseBuffer, (const char *)uartRxBuffer + espDmaLastPos, min(len, 63));
				responseBuffer[len] = '\0';
			}
			else
			{
				len = ESP_RX_BUFFERSIZE - espDmaLastPos;
				memcpy(responseBuffer, (const char *)uartRxBuffer + espDmaLastPos, min(len + pos, 63));
				memcpy(responseBuffer + len, uartRxBuffer, pos);
				responseBuffer[len + pos] = 0;
				len += pos;
			}
			HAL_UART_Transmit(huartEcho, (const uint8_t *)responseBuffer, len, 100);
			espDmaLastPos = pos;

			if (strstr(responseBuffer, "OK\r\n") != 0)
			{
				return ESP_SUCCESS;
			}
			else if (strstr(responseBuffer, "ERROR\r\n") != 0)
			{
				return ESP_ERROR;
			}
		}
	}

	return ESP_UNKNOWN;
}
