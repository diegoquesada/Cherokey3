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
			break; // timeout with no data received
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

	osDelay(200);

	// Examine RX buffer as data arrives, looking for "ready" word.
	const char readyString[] = "ready\r\n";
	esp_status_t readyStatus = espMatchWord(readyString);

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

	char errorBuffer[64];

	esp_status_t readyRes = espWaitReady();
	if (readyRes != ESP_SUCCESS)
	{
		uint16_t pos = ESP_RX_BUFFERSIZE - __HAL_DMA_GET_COUNTER(_huartESP->hdmarx);
		snprintf(errorBuffer, sizeof(errorBuffer), "UART error %u, pos=%hu\r\n",
				(unsigned int)readyRes, pos);
		HAL_UART_Transmit(_huartEcho, (const uint8_t *)errorBuffer, strlen(errorBuffer), 100);
		return readyRes;
	}

	//HAL_UART_Transmit(_huartEcho, (const uint8_t *)"Got ready\r\n", 11, 100);

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
	espSendSync(queryip_cmd);

	/*const uint8_t querysleep_cmd[] = "AT+SLEEP?\r\n";
	espSendSync(querysleep_cmd);

	const uint8_t querystore_cmd[] = "AT+SYSSTORE?\r\n";
	espSendSync(querystore_cmd);

	const uint8_t querycountry_cmd[] = "AT+CWCOUNTRY?\r\n";
	espSendSync(querycountry_cmd);*/

	const uint8_t queryver_cmd[] = "AT+GMR\r\n";
	espSendSync(queryver_cmd);

	return ESP_SUCCESS;
}

#define min(x, y) (((x) < (y)) ? (x) : (y))

uint8_t espEchoBuffer(uint16_t start, uint16_t end)
{
	uint8_t foundOK = 0;
	char responseBuffer[64];
	for (uint16_t currentPos = start; currentPos < end; )
	{
		uint16_t copyLen = min(end - currentPos, 64);
		memcpy(responseBuffer, (const char *)uartRxBuffer + currentPos, copyLen);
		HAL_UART_Transmit(_huartEcho, (const uint8_t *)responseBuffer, copyLen, 100);
		currentPos += copyLen;

		if (strstr(responseBuffer, "OK\r\n") != 0)
		{
			foundOK = 1;
		}
	}

	return foundOK;
}

esp_status_t espSendSync(const uint8_t *cmd)
{
	// Send AT command to the ESP8266
	HAL_UART_Transmit(_huartESP, cmd, strlen((const char *)cmd), 100);

	for (;;)
	{
		if (osSemaphoreAcquire(uart4RxSemHandle, 100) == osOK)
		{
			uint16_t pos = ESP_RX_BUFFERSIZE - __HAL_DMA_GET_COUNTER(_huartESP->hdmarx);

			if (pos > espDmaLastPos) // Buffer has not wrapped
			{
				if (espEchoBuffer(espDmaLastPos, pos))
				{
					return ESP_SUCCESS; // We found OK
				}
			}
			else if (pos < espDmaLastPos) // Buffer has wrapped
			{
				espEchoBuffer(espDmaLastPos, ESP_RX_BUFFERSIZE);
				if (espEchoBuffer(0, pos))
				{
					return ESP_SUCCESS; // We found OK
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
			return ESP_TIMEOUT;
		}
	}

	return ESP_ERROR;
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
