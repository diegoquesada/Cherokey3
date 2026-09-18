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

UART_HandleTypeDef *_huartESP = 0;
UART_HandleTypeDef *_huartEcho = 0;

osSemaphoreId_t uart4RxSemHandle; // Used to signal that data has arrived from the ESP serial

#define ESP_TX_BUFFERSIZE  64
#define ESP_RX_BUFFERSIZE 512 	  // Size of our receive buffer for the ESP8266
static uint8_t uartTxBuffer[ESP_TX_BUFFERSIZE];
static uint8_t uartRxBuffer[ESP_RX_BUFFERSIZE];
uint16_t espDmaLastPos = 0; 	  // End of last data buffer processed
static volatile uint32_t espDmaWrapCount = 0; // Tracks how many times the buffer has filled up

void espInit(UART_HandleTypeDef *huartESP, UART_HandleTypeDef *huartEcho)
{
	_huartESP = huartESP;
	_huartEcho = huartEcho;

	uart4RxSemHandle = osSemaphoreNew(1, 0, NULL);

	memset(uartRxBuffer, 0, sizeof(uartRxBuffer));
}

/**
 * Looks for a word in the ESP8266 RX.
 * @return 0 if the word is found, 1 if timeout occurred.
 */
esp_status_t espMatchWord(const char *wordToMatch)
{
	uint8_t wordFound = 0;
	while (!wordFound)
	{
		if (osSemaphoreAcquire(uart4RxSemHandle, 100) == osOK)
		{
			uint16_t matchIndex = 0;
			uint16_t pos = ESP_RX_BUFFERSIZE - __HAL_DMA_GET_COUNTER(_huartESP->hdmarx);

			if (pos > espDmaLastPos)
			{
				// New data was received without a circular buffer wrap.
				for (uint16_t i = espDmaLastPos; i < pos && !wordFound; i++)
				{
					if (uartRxBuffer[i] == wordToMatch[matchIndex])
					{
						matchIndex++;
						if (wordToMatch[matchIndex] == '\0')
						{
							matchIndex = 0;
							wordFound = 1;
						}
					}
					else if (matchIndex != 0)
					{
						matchIndex = 0;
						if (uartRxBuffer[i] == wordToMatch[matchIndex])
						{
							matchIndex++;
						}
					}
				}
			}
			else if (pos < espDmaLastPos)
			{
				// New data caused a wrap in circular buffer. Process in two steps.
				for (uint16_t i = espDmaLastPos; i < ESP_RX_BUFFERSIZE && !wordFound; i++)
				{
					if (uartRxBuffer[i] == wordToMatch[matchIndex])
					{
						matchIndex++;
						if (wordToMatch[matchIndex] == '\0')
						{
							matchIndex = 0;
							wordFound = 1;
						}
					}
					else if (matchIndex != 0)
					{
						matchIndex = 0;
						if (uartRxBuffer[i] == wordToMatch[matchIndex])
						{
							matchIndex++;
						}
					}
				}
				for (uint16_t i = 0; i < pos && !wordFound; i++)
				{
					if (uartRxBuffer[i] == wordToMatch[matchIndex])
					{
						matchIndex++;
						if (wordToMatch[matchIndex] == '\0')
						{
							matchIndex = 0;
							wordFound = 1;
						}
					}
					else if (matchIndex != 0)
					{
						matchIndex = 0;
						if (uartRxBuffer[i] == wordToMatch[matchIndex])
						{
							matchIndex++;
						}
					}
				}
			}
			espDmaLastPos = pos;
		}
		else
		{
			break; // timeout without data received
		}
	}

	return wordFound ? ESP_SUCCESS : ESP_TIMEOUT;
}

esp_status_t espWaitReady()
{
	// Trigger data reception via DMA. We will be alerted via Idle interrupt.
	HAL_StatusTypeDef halStatus = HAL_UART_Receive_DMA(_huartESP, uartRxBuffer, sizeof(uartRxBuffer));
	if (halStatus != HAL_OK)
	{
		return ESP_ERROR;
	}
	__HAL_UART_ENABLE_IT(_huartESP, UART_IT_IDLE);

	// Raise enable pin to turn on ESP module.
	HAL_GPIO_WritePin(ESP_ENABLE_GPIO_Port, ESP_ENABLE_Pin, GPIO_PIN_SET);

	// Examine RX buffer as data arrives, looking for "ready" word.
	const char readyString[] = "ready\r\n";
	esp_status_t readyStatus = espMatchWord(readyString) == 0 ? ESP_SUCCESS : ESP_ERROR;

	// Process and clear any additional data we may have received after matching "ready"
	for (;;)
	{
		if (osSemaphoreAcquire(uart4RxSemHandle, 100) == osOK)
		{
			// Advance pointer so as to ignore additional data received.
			espDmaLastPos = ESP_RX_BUFFERSIZE - __HAL_DMA_GET_COUNTER(_huartESP->hdmarx);
		}
		else
		{
			break;
		}
	}

	return readyStatus;
}

esp_status_t espStart()
{
	if (_huartESP == 0)
	{
		return ESP_ERROR;
	}

	esp_status_t readyRes = espWaitReady();
	if (readyRes != ESP_SUCCESS)
	{
		HAL_UART_Transmit(_huartEcho, (const uint8_t *)"UART error\r\n", 12, 100);
		return readyRes;
	}

	HAL_UART_Transmit(_huartEcho, (const uint8_t *)"Got ready\r\n", 11, 100);

	const uint8_t at_cmd[] = "AT\r\n";
	if (espSendSync(at_cmd) != ESP_SUCCESS)
		return ESP_ERROR;

	const uint8_t mode_cmd[] = "AT+CWMODE=1\r\n";
	if (espSendSync(mode_cmd) != ESP_SUCCESS)
		return ESP_ERROR;

	const uint8_t querymac_cmd[] = "AT+CIPSTAMAC?\r\n";
	if (espSendSync(querymac_cmd) != ESP_SUCCESS)
		return ESP_ERROR;

	const char connect_cmd[] = "AT+CWJAP=\"Delphi\",\"%s\"\r\n";
	snprintf((char *)uartTxBuffer, sizeof(uartTxBuffer), connect_cmd, WIFI_PASSWORD);
	espSendSync(uartTxBuffer);

	const uint8_t queryip_cmd[] = "AT+CIPSTA?\r\n";
	espSendSync(queryip_cmd);

	return ESP_SUCCESS;
}

#define min(x, y) (((x) < (y)) ? (x) : (y))

esp_status_t espSendSync(const uint8_t *cmd)
{
	// Send AT command to the ESP8266
	HAL_UART_Transmit(_huartESP, cmd, strlen((const char *)cmd), 100);

	char responseBuffer[64];
	for (;;)
	{
		if (osSemaphoreAcquire(uart4RxSemHandle, 100) == osOK)
		{
			uint16_t pos = ESP_RX_BUFFERSIZE - __HAL_DMA_GET_COUNTER(_huartESP->hdmarx);
			uint16_t len = 0;

			if (pos > espDmaLastPos)
			{
				len = pos - espDmaLastPos;
				uint16_t copyLen = min(len, 63);
				memcpy(responseBuffer, (const char *)uartRxBuffer + espDmaLastPos, copyLen);
				responseBuffer[copyLen] = '\0';
				len = copyLen;
			}
			else if (pos < espDmaLastPos)
			{
				uint16_t firstPart = ESP_RX_BUFFERSIZE - espDmaLastPos;
				uint16_t secondPart = pos;
				uint16_t totalLen = firstPart + secondPart;
				uint16_t copyLen = min(totalLen, 63);

				uint16_t firstCopy = min(firstPart, copyLen);
				memcpy(responseBuffer, (const char *)uartRxBuffer + espDmaLastPos, firstCopy);

				if (copyLen > firstCopy)
				{
					memcpy(responseBuffer + firstCopy, uartRxBuffer, copyLen - firstCopy);
				}

				responseBuffer[copyLen] = '\0';
				len = copyLen;
			}
			else
			{
				// pos == espDmaLastPos, no new data.
				continue;
			}

			HAL_UART_Transmit(_huartEcho, (const uint8_t *)responseBuffer, len, 100);
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
}

void espRxCpltCallback(UART_HandleTypeDef *huart)
{
	espDmaWrapCount++;
}

static uint8_t errorCount = 0;

void espRecoverUart()
{
	if (_huartESP == 0)
	{
		return;
	}

	errorCount++;

    __HAL_UART_CLEAR_FLAG(_huartESP, UART_CLEAR_OREF);
    __HAL_UART_CLEAR_FLAG(_huartESP, UART_CLEAR_FEF);
    __HAL_UART_CLEAR_FLAG(_huartESP, UART_CLEAR_NEF);

	// Restart DMA reception
	HAL_UART_Receive_DMA(_huartESP, uartRxBuffer, ESP_RX_BUFFERSIZE);
	__HAL_UART_ENABLE_IT(_huartESP, UART_IT_IDLE);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart == _huartESP)
	{
		// Check if an Overrun or Framing error occurred using the HAL error code
		if ((huart->ErrorCode & HAL_UART_ERROR_ORE) || (huart->ErrorCode & HAL_UART_ERROR_FE))
		{
			espRecoverUart();
		}
	}
}
