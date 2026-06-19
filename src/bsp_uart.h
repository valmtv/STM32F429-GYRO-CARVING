#ifndef BSP_UART_H
#define BSP_UART_H

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define UART_TX_BUFFER_SIZE 64 // 12 bytes is enough for our struct, 64 is safe

extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef hdma_usart6_tx;
extern SemaphoreHandle_t xUartTxSemaphore;
extern char uart_tx_buffer[UART_TX_BUFFER_SIZE];

void BSP_UART_Init(void);
void BSP_UART_DMA_Init(void);
void BSP_UART_SendData(size_t len);
#endif
