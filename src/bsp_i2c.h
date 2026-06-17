#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

// BMA180 I2C Configuration
#define BMA180_I2C_ADDRESS    (0x40 << 1) // 0x40 shifted left for HAL
#define BMA180_REG_ACCEL_X_LSB 0x02
#define ACCEL_DATA_SIZE        6

extern I2C_HandleTypeDef hi2c3;
extern DMA_HandleTypeDef hdma_i2c3_rx;
extern uint8_t aRxBuffer[ACCEL_DATA_SIZE];
extern TaskHandle_t xPhysicsTaskHandle;

void BSP_I2C_Init(void);
void BSP_I2C_DMA_Init(void);
void BSP_I2C_Start_DMA_Read(void);

#endif
