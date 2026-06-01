#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include <cstring>
#include "diag/trace.h"

// BMA180 Register Map (Assumes I2C address 0x40 shifted left by 1)
#define BMA180_I2C_ADDRESS    (0x40 << 1)
#define BMA180_REG_ACCEL_X_LSB 0x02
#define ACCEL_DATA_SIZE        6

I2C_HandleTypeDef hi2c3;
DMA_HandleTypeDef hdma_i2c3_rx;

TaskHandle_t xPhysicsTaskHandle;
uint8_t aRxBuffer[ACCEL_DATA_SIZE];

// Function Prototypes
extern "C" {
    void SystemClock_Config(void);
}
static void I2C3_Init(void);
static void DMA_Init(void);
void vPhysicsTask(void *pvParameters);


int main(void) {
    HAL_Init();
    SystemClock_Config();

    // SystemView Configuration (Required by architecture)
    // SEGGER_SYSVIEW_Conf();

    DMA_Init();
    I2C3_Init();

    trace_puts("Checking BMA180 connection...\n");
    if (HAL_I2C_IsDeviceReady(&hi2c3, BMA180_I2C_ADDRESS, 3, 1000) == HAL_OK) {
        trace_puts("BMA180 found! I2C connection successful.\n");
    } else {
        trace_puts("Error: BMA180 not found. Check wiring.\n");
    }

    // Create the Physics Task (Sensor Task)
    xTaskCreate(vPhysicsTask, "PhysicsTask", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 2, &xPhysicsTaskHandle);

    vTaskStartScheduler();

    while (1) {
    }
}

static void DMA_Init(void) {
    // Enable DMA1 clock
    __HAL_RCC_DMA1_CLK_ENABLE();

    // Configure DMA for I2C3 RX
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

    // Link the DMA handle to the I2C handle
    __HAL_LINKDMA(&hi2c3, hdmarx, hdma_i2c3_rx);
}

static void I2C3_Init(void) {
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

// C-linkage required for HAL callbacks and IRQ handlers in a .cpp file
extern "C" {

void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c) {
    GPIO_InitTypeDef GPIO_InitStruct;
    memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));

    if(hi2c->Instance == I2C3) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_I2C3_CLK_ENABLE();

        // Configure SCL (PA8) and SDA (PC9)
        GPIO_InitStruct.Pin = GPIO_PIN_8;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FAST;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C3;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_9;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        // DMA Interrupt priority configuration
        HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
    }
}

// DMA RX Complete Callback (Hardware Interrupt)
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    if (hi2c->Instance == I2C3) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        // Unblock the Physics Task
        vTaskNotifyGiveFromISR(xPhysicsTaskHandle, &xHigherPriorityTaskWoken);

        // Yield if a higher priority task was woken
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// Ensure the actual hardware IRQ is exposed to C
void DMA1_Stream2_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_i2c3_rx);
}

void SystemClock_Config(void) {
  __PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;

  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2; /* 168 MHz for STM32F429xx */
  RCC_OscInitStruct.PLL.PLLQ = 7;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
      | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);
}

} // End extern "C"

// Physics Task
void vPhysicsTask(void *pvParameters) {
    uint32_t ulNotificationValue;
    int16_t accel_x, accel_y, accel_z;

    // Initial DMA Read Trigger
    HAL_I2C_Mem_Read_DMA(&hi2c3, BMA180_I2C_ADDRESS, BMA180_REG_ACCEL_X_LSB, I2C_MEMADD_SIZE_8BIT, aRxBuffer, ACCEL_DATA_SIZE);

    for (;;) {
        // Block indefinitely until DMA interrupt triggers notification
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (ulNotificationValue > 0) {
                    // Reconstruct 14-bit data from 8-bit registers
                    accel_x = (int16_t)((aRxBuffer[1] << 8) | (aRxBuffer[0])) >> 2;
                    accel_y = (int16_t)((aRxBuffer[3] << 8) | (aRxBuffer[2])) >> 2;
                    accel_z = (int16_t)((aRxBuffer[5] << 8) | (aRxBuffer[4])) >> 2;

                    // Output the raw data to the trace console
                    trace_printf("X: %d | Y: %d | Z: %d\n", accel_x, accel_y, accel_z);

                    // Re-trigger DMA for the next continuous read cycle
                    HAL_I2C_Mem_Read_DMA(&hi2c3, BMA180_I2C_ADDRESS, BMA180_REG_ACCEL_X_LSB, I2C_MEMADD_SIZE_8BIT, aRxBuffer, ACCEL_DATA_SIZE);
                }
    }
}
