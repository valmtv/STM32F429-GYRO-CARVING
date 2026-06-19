/**
 * @file bsp_uart.cpp
 * @brief Board Support Package for USART6 DMA transmission.
 * @note Offloads string transmission to DMA2 Stream6 to prevent CPU blocking.
 *       Uses a binary semaphore to prevent buffer tearing.
 */
#include "bsp_uart.h"
#include <cstring>

UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart6_tx;
SemaphoreHandle_t xUartTxSemaphore;
char uart_tx_buffer[UART_TX_BUFFER_SIZE];

/**
 * @brief Configures DMA2 Stream6 Channel 5 for USART6 TX memory transfers.
 */
void BSP_UART_DMA_Init(void) {
    __HAL_RCC_DMA2_CLK_ENABLE();
    hdma_usart6_tx.Instance = DMA2_Stream6;
    hdma_usart6_tx.Init.Channel = DMA_CHANNEL_5;
    hdma_usart6_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart6_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart6_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart6_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart6_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart6_tx.Init.Mode = DMA_NORMAL;
    hdma_usart6_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart6_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_usart6_tx);
    __HAL_LINKDMA(&huart6, hdmatx, hdma_usart6_tx);
}

/**
 * @brief Initializes USART6 at 115200 baud 8N1.
 */
void BSP_UART_Init(void) {
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 115200;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart6);
}

/**
 * @brief Safely triggers UART DMA if the buffer is not currently being read.
 */
void BSP_UART_SendData(size_t len) {
    // Wait for previous DMA to finish to prevent buffer tearing
    if (xSemaphoreTake(xUartTxSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
        HAL_UART_Transmit_DMA(&huart6, (uint8_t*)uart_tx_buffer, len);
    }
}

extern "C" {
    /**
     * @brief MCU Specific Package (MSP) init: Maps PC6(TX) and PC7(RX) to AF8.
     */
    void HAL_UART_MspInit(UART_HandleTypeDef* huart) {
        GPIO_InitTypeDef GPIO_InitStruct;
        memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
        if(huart->Instance == USART6) {
            __HAL_RCC_GPIOC_CLK_ENABLE();
            __HAL_RCC_USART6_CLK_ENABLE();

            GPIO_InitStruct.Pin = GPIO_PIN_6;
            GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
            GPIO_InitStruct.Pull = GPIO_PULLUP;
            GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
            GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
            HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

            GPIO_InitStruct.Pin = GPIO_PIN_7;
            HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

            HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 5, 0);
            HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);
            HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
            HAL_NVIC_EnableIRQ(USART6_IRQn);
        }
    }

    /**
     * @brief Hardware Interrupt Callback: Fires when DMA finishes UART TX.
     */
    void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
        if (huart->Instance == USART6) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            // Return ownership of the buffer to the Physics Task
            xSemaphoreGiveFromISR(xUartTxSemaphore, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }

    void DMA2_Stream6_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_usart6_tx); }
    void USART6_IRQHandler(void) { HAL_UART_IRQHandler(&huart6); }
}
