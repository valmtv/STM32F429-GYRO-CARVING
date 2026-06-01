/*
 * This file is part of the µOS++ distribution.
 * (https://github.com/micro-os-plus)
 * Copyright (c) 2014 Liviu Ionescu.
 */

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "diag/trace.h"
#include "SEGGER_SYSVIEW.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"



// If you followed step 2.10 to fix the hang, uncomment the line below:
// extern "C" void vSetVarulMaxPRIGROUPValue(void);

// ----------------------------------------------------------------------------

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

// TASK 2
// Global handle so Task 1 can find Task 2 to send the notification
TaskHandle_t xTaskLEDHandle = NULL;

// Task 1: Button Scanner
void vTaskButton(void *pvParameters) {
    uint8_t lastButtonState = 0; // Assume 0 is unpressed

    while(1) {
        // Read the blue User Button (PA0)
        uint8_t currentButtonState = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

        // Detect a "rising edge" (button was just pressed down)
        if(currentButtonState == 1 && lastButtonState == 0) {
            // Send the signal to Task 2!
            // eNoAction means we just want to wake it up, not send a specific number.
            if(xTaskLEDHandle != NULL) {
                xTaskNotify(xTaskLEDHandle, 0, eNoAction);
            }
        }

        lastButtonState = currentButtonState;

        // Sleep for 50ms. This prevents CPU hogging AND debounces the button!
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Task 2: LED Controller
void vTaskLED(void *pvParameters) {
    // 1. Initial Setup: Pick a random starting color
	if (xTaskGetTickCount() % 2 == 0) {
		HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);   // Green ON
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_14, GPIO_PIN_RESET); // Red OFF
    } else {
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET); // Green OFF
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_14, GPIO_PIN_SET);   // Red ON
    }

    // 2. The Main Loop
    while(1) {
        // Go completely to sleep and wait for the signal from Task 1.
        // portMAX_DELAY means it will wait forever without burning any CPU time.
        BaseType_t xResult = xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);

        // If we woke up because we got the signal...
        if(xResult == pdPASS) {
            // Toggle both LEDs to switch the colors
            HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_13);
            HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_14);
        }
    }
}
/* TASK 1
// Task 1: Active Waiting
void vTaskActiveWait(void *pvParameters) {
    while(1) {
        TickType_t startTick = xTaskGetTickCount();

        // ACTIVE WAIT: Spin in a useless loop for 350ms.
        // The CPU is completely trapped here.
        while((xTaskGetTickCount() - startTick) < pdMS_TO_TICKS(350)) {
            // Burning CPU cycles...
        }

        // Brief 10ms OS sleep just to prevent the system from totally locking up
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task 2: The "Good Citizen" (OS Sleep)
void vTaskDelaySleep(void *pvParameters) {
    while(1) {
        // Fake a tiny amount of "work" so SystemView draws a visible block
        for(volatile int i = 0; i < 50000; i++) {}

        // OS WAIT: Politely tell FreeRTOS to put this task to sleep for 350ms.
        // The CPU is immediately freed up for other tasks to use.
        vTaskDelay(pdMS_TO_TICKS(350));
    }
}
*/

int main(int argc, char* argv[]) {
    // 0. Initialize the STM32 Hardware Abstraction Layer
    HAL_Init();

    // --- START OF HARDWARE PIN SETUP ---
    // Enable Clocks for Port A (Button) and Port G (LEDs)
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Configure Button Pin (PA0) as Input
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Configure LED Pins (PG13, PG14) as Output
    GPIO_InitStruct.Pin = GPIO_PIN_13 | GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    // --- END OF HARDWARE PIN SETUP ---

    // If you followed step 2.10, uncomment the line below:
    vSetVarulMaxPRIGROUPValue();

    // 1. Activate hardware cycle counter (Step 2.7)
    DWT->CTRL |= 1;

    // 2. Configure and Start SystemView (Step 2.8)
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Start();

    // Create the tasks
    xTaskCreate(vTaskButton, "ButtonTask", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    // Notice the final parameter is NOT NULL here! We need to capture the handle.
    xTaskCreate(vTaskLED, "LEDTask", configMINIMAL_STACK_SIZE, NULL, 1, &xTaskLEDHandle);

    // 4. Start the FreeRTOS scheduler
    vTaskStartScheduler();

    // The program should never reach this loop if the scheduler starts successfully
    while (1) {
        trace_printf("failure!!!\n");
    }

    return 0;
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
