/**
 * @file bsp_i2c.cpp
 * @brief Board Support Package for I2C3 and BMA180 DMA acquisition.
 * @note Handles hardware abstraction, GPIO Alternate Function mapping (AF4),
 *       and FreeRTOS ISR-safe task notifications.
 */
#include "bsp_i2c.h"
#include <cstring>

I2C_HandleTypeDef hi2c3;
DMA_HandleTypeDef hdma_i2c3_rx;
uint8_t aRxBuffer[ACCEL_DATA_SIZE];
TaskHandle_t xPhysicsTaskHandle;

/**
 * @brief Configures DMA1 Stream2 Channel 3 for I2C3 RX memory transfers.
 */
void BSP_I2C_DMA_Init(void) {
    __HAL_RCC_DMA1_CLK_ENABLE();
    hdma_i2c3_rx.Instance = DMA1_Stream2;
    hdma_i2c3_rx.Init.Channel = DMA_CHANNEL_3;
    hdma_i2c3_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_i2c3_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2c3_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2c3_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_i2c3_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_i2c3_rx.Init.Mode = DMA_NORMAL;
    hdma_i2c3_rx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_i2c3_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_i2c3_rx);
    __HAL_LINKDMA(&hi2c3, hdmarx, hdma_i2c3_rx);
}

/**
 * @brief Initializes I2C3 peripheral at 100kHz standard mode.
 */
void BSP_I2C_Init(void) {
    hi2c3.Instance = I2C3;
    hi2c3.Init.ClockSpeed = 100000;
    hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c3.Init.OwnAddress1 = 0;
    hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
    hi2c3.Init.OwnAddress2 = 0;
    hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
    hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;
    HAL_I2C_Init(&hi2c3);
}

/**
 * @brief Triggers a non-blocking DMA read of the 6-byte accelerometer block.
 */
void BSP_I2C_Start_DMA_Read(void) {
    HAL_I2C_Mem_Read_DMA(&hi2c3, BMA180_I2C_ADDRESS, BMA180_REG_ACCEL_X_LSB,
                         I2C_MEMADD_SIZE_8BIT, aRxBuffer, ACCEL_DATA_SIZE);
}

// C-linkage required for HAL callbacks and IRQ handlers in a .cpp file
extern "C" {
    /**
     * @brief MCU Specific Package (MSP) init: Maps PA8(SCL) and PC9(SDA) to AF4.
     */
    void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c) {
        GPIO_InitTypeDef GPIO_InitStruct;
        memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
        if(hi2c->Instance == I2C3) {
            __HAL_RCC_GPIOA_CLK_ENABLE();
            __HAL_RCC_GPIOC_CLK_ENABLE();
            __HAL_RCC_I2C3_CLK_ENABLE();

            GPIO_InitStruct.Pin = GPIO_PIN_8;
            GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
            GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

            GPIO_InitStruct.Pin = GPIO_PIN_9;
            HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

            // Priority 5 is safe for FreeRTOS API calls from ISR
            HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 5, 0);
            HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
        }
    }

    /**
     * @brief Hardware Interrupt Callback: Fires when DMA finishes I2C read.
     */
    void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
        if (hi2c->Instance == I2C3) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            // Unblock the Physics Task
            vTaskNotifyGiveFromISR(xPhysicsTaskHandle, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }

    void DMA1_Stream2_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_i2c3_rx); }
}
