/**
 * @file main.cpp
 * @brief System entry point and RTOS scheduler initialization.
 * @note Enforces strict initialization order: Clock -> Peripherals -> RTOS.
 */
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bsp_i2c.h"
#include "bsp_uart.h"
#include "physics_task.h"
#include "diag/trace.h"

extern "C" {
    void SystemClock_Config(void);
}

int main(void) {
    HAL_Init();
    SystemClock_Config();

    // 1. Initialize I2C and DMA
    BSP_I2C_DMA_Init();
    BSP_I2C_Init();

    trace_puts("Checking BMA180 connection...\n");
    if (HAL_I2C_IsDeviceReady(&hi2c3, BMA180_I2C_ADDRESS, 3, 1000) == HAL_OK) {
        trace_puts("BMA180 found!\n");
    } else {
        trace_puts("Error: BMA180 not found.\n");
    }

    // 2. Initialize UART IPC Semaphore
    xUartTxSemaphore = xSemaphoreCreateBinary();
    if (xUartTxSemaphore != NULL) xSemaphoreGive(xUartTxSemaphore);

    // 3. Initialize UART and DMA
    BSP_UART_DMA_Init();
    BSP_UART_Init();

    // 4. Create RTOS Tasks
    xTaskCreate(vPhysicsTask, "PhysicsTask", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 2, &xPhysicsTaskHandle);

    // 5. Start Scheduler
    vTaskStartScheduler();

    // Should never reach here
    while (1) {}
}
