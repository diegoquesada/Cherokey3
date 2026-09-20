/**
  * @file           : comms.c
  * @brief          : Communication via ESP8266 module
  *
  * Copyright (c) 2026 Diego Quesada
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
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
static uint8_t uartRxBuffer[ESP_RX_BUFFERSIZE];
uint16_t espDmaLastPos = 0; 	  // End of last data buffer processed
static volatile uint32_t espDmaWrapCount = 0; // Tracks how many times the buffer has filled up

uint8_t socketOpen = 0;

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
esp_status_t espMatchWord(const char *wordToMatch, uint32_t timeout)
{
	uint8_t wordFound = 0;
	while (!wordFound)
	{
		if (osSemaphoreAcquire(uart4RxSemHandle, timeout) == osOK)
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
			break; // timeout with no data received
		}
	}

	return wordFound ? ESP_SUCCESS : ESP_TIMEOUT;
}

esp_status_t espWaitReady()
{
	// Examine RX buffer as data arrives, looking for "ready" word.
	const char readyString[] = "ready\r\n";
	esp_status_t readyStatus = espMatchWord(readyString, 100);
	if (readyStatus != ESP_SUCCESS)
		return readyStatus;

	HAL_UART_Transmit(_huartEcho, (const uint8_t *)"ready\r\n", 7, 100);

	// Look for a message that indicates the ESP module has reconnected
	// to a previously stored SSID. Wait for up to 5 s.
	for (uint8_t retries = 0; retries < 5; retries++)
	{
		readyStatus = espMatchWord("WIFI GOT IP\r\n", 1000);
		if (readyStatus == ESP_SUCCESS)
		{
			HAL_UART_Transmit(_huartEcho, (const uint8_t *)"CONNECTED\r\n", 11, 100);
			break;
		}
		else if (readyStatus == ESP_TIMEOUT)
			continue;
		else
			break;
	}

	return readyStatus;
}

esp_status_t espStart()
{
	if (_huartESP == 0)
		return ESP_ERROR;

	char errorBuffer[64];

	// Trigger data reception via DMA. We will be alerted via Idle interrupt.
	HAL_StatusTypeDef halStatus = HAL_UART_Receive_DMA(_huartESP, uartRxBuffer, sizeof(uartRxBuffer));
	if (halStatus != HAL_OK)
	{
		return ESP_ERROR;
	}
	__HAL_UART_ENABLE_IT(_huartESP, UART_IT_IDLE);

	// Raise enable pin to turn on ESP module.
	HAL_GPIO_WritePin(ESP_ENABLE_GPIO_Port, ESP_ENABLE_Pin, GPIO_PIN_SET);

	// Give the ESP module some time to boot up.
	osDelay(500);

	esp_status_t readyRes = espWaitReady();
	if (readyRes != ESP_SUCCESS)
	{
		uint16_t pos = ESP_RX_BUFFERSIZE - __HAL_DMA_GET_COUNTER(_huartESP->hdmarx);
		snprintf(errorBuffer, sizeof(errorBuffer), "UART error %u, pos=%hu\r\n",
				(unsigned int)readyRes, pos);
		HAL_UART_Transmit(_huartEcho, (const uint8_t *)errorBuffer, strlen(errorBuffer), 100);
		return readyRes;
	}

	/*const uint8_t at_cmd[] = "AT\r\n";
	if (espSendSync(at_cmd) != ESP_SUCCESS)
		return ESP_ERROR;*/

	/*const uint8_t mode_cmd[] = "AT+CWMODE=1\r\n";
	if (espSendSync(mode_cmd) != ESP_SUCCESS)
		return ESP_ERROR;

	const uint8_t querymac_cmd[] = "AT+CIPSTAMAC?\r\n";
	if (espSendSync(querymac_cmd) != ESP_SUCCESS)
		return ESP_ERROR;

	const char connect_cmd[] = "AT+CWJAP=\"Delphi\",\"%s\"\r\n";
	snprintf((char *)uartTxBuffer, sizeof(uartTxBuffer), connect_cmd, WIFI_PASSWORD);
	espSendSync(uartTxBuffer);*/

	const uint8_t queryip_cmd[] = "AT+CIPSTA?\r\n";
	espSendSync(queryip_cmd, 100, 1);

	/*const uint8_t queryver_cmd[] = "AT+GMR\r\n";
	espSendSync(queryver_cmd);*/

	return ESP_SUCCESS;
}

#define min(x, y) (((x) < (y)) ? (x) : (y))

uint8_t espCheckOK(uint16_t start, uint16_t end, uint8_t echo)
{
	uint8_t foundOK = 0;
	char responseBuffer[64];
	for (uint16_t currentPos = start; currentPos < end; )
	{
		uint16_t copyLen = min(end - currentPos, 64);
		memcpy(responseBuffer, (const char *)uartRxBuffer + currentPos, copyLen);

		if (echo)
			HAL_UART_Transmit(_huartEcho, (const uint8_t *)responseBuffer, copyLen, 100);
		currentPos += copyLen;

		if (strstr(responseBuffer, "OK\r\n") != 0)
		{
			foundOK = 1;
		}
	}

	return foundOK;
}

esp_status_t espSendSync(const uint8_t *cmd, uint32_t timeout, uint8_t echo)
{
	if (_huartESP == 0)
		return ESP_ERROR;

	// Send AT command to the ESP8266
	HAL_UART_Transmit(_huartESP, cmd, strlen((const char *)cmd), 100);

	esp_status_t retStatus = ESP_ERROR;
	for (uint8_t done = 0; !done;)
	{
		if (osSemaphoreAcquire(uart4RxSemHandle, timeout) == osOK)
		{
			uint16_t pos = ESP_RX_BUFFERSIZE - __HAL_DMA_GET_COUNTER(_huartESP->hdmarx);

			if (pos > espDmaLastPos) // Buffer has not wrapped
			{
				if (espCheckOK(espDmaLastPos, pos, echo))
				{
					retStatus = ESP_SUCCESS;
					done = 1; // We found OK.
				}
			}
			else if (pos < espDmaLastPos) // Buffer has wrapped
			{
				espCheckOK(espDmaLastPos, ESP_RX_BUFFERSIZE, echo);
				if (espCheckOK(0, pos, echo))
				{
					// Dangerous assumption: OK is in the second buffer. This needs fixing.
					retStatus = ESP_SUCCESS;
					done = 1;
				}
			}
			else
			{
				// pos == espDmaLastPos, no new data.
				continue;
			}

			espDmaLastPos = pos;
		}
		else
		{
			retStatus = ESP_TIMEOUT;
			done = 1;
		}
	}

	return retStatus;
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

esp_status_t espOpenSocket()
{
	const uint8_t tcp_cmd[] = "AT+CIPSTART=\"TCP\",\"192.168.1.232\",9576\r\n";
	esp_status_t retStatus = espSendSync(tcp_cmd, 200, 1);
	if (retStatus == ESP_SUCCESS)
	{
		socketOpen = 1;
	}

	return retStatus;
}

esp_status_t espSendSocket(const uint8_t *data, uint16_t len)
{
	if (_huartESP == 0)
		return ESP_ERROR;
	if (data == 0 || len > 64)
		return ESP_ERROR;
	if (!socketOpen)
		return ESP_ERROR;

	uint8_t dataBuffer[64];
	snprintf((char *)dataBuffer, 64, "AT+CIPSEND=%hu\r\n", len);
	if (espSendSync(dataBuffer, 100, 0) != ESP_SUCCESS)
	{
		espCloseSocket();
		socketOpen = 0;
	}

	if (espSendSync(data, 200, 0) != ESP_SUCCESS)
	{
		espCloseSocket();
		socketOpen = 0;
	}

	return ESP_SUCCESS;
}

esp_status_t espCloseSocket()
{
	if (!socketOpen)
		return ESP_ERROR;

	const uint8_t close_cmd[] = "AT+CIPCLOSE\r\n";
	return espSendSync(close_cmd, 100, 1);
}

