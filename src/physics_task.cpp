/**
 * @file physics_task.cpp
 * @brief FreeRTOS task for sensor fusion and binary telemetry packaging.
 * @note Transmits raw memory structs over UART DMA. Zero string formatting.
 */
#include "physics_task.h"
#include "bsp_i2c.h"
#include "bsp_uart.h"
#include "math_utils.h"
#include <string.h> // For memcpy

// Binary Struct Definition (Must match ESP32 exactly)
typedef struct __attribute__((packed)) {
    uint16_t sync_word; // 0xAA55 to prevent UART stream desynchronization
    int16_t ax, ay, az; // Raw acceleration
    int16_t tilt_x, tilt_y; // Angles stored as hundredths of a degree (e.g. 1234 = 12.34°)
} TelemetryPacket_t;

TelemetryPacket_t tx_packet;

void vPhysicsTask(void *pvParameters) {
    (void)pvParameters;
    uint32_t ulNotificationValue;
    int16_t accel_x, accel_y, accel_z;
    float tilt_x, tilt_y;

    tx_packet.sync_word = 0xAA55; // Set sync header once
    BSP_I2C_Start_DMA_Read();

    for (;;) {
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (ulNotificationValue > 0) {
            accel_x = (int16_t)((aRxBuffer[1] << 8) | (aRxBuffer[0])) >> 2;
            accel_y = (int16_t)((aRxBuffer[3] << 8) | (aRxBuffer[2])) >> 2;
            accel_z = (int16_t)((aRxBuffer[5] << 8) | (aRxBuffer[4])) >> 2;

            float fx = (float)-accel_x;
            float fy = (float)accel_y;
            float fz = (float)accel_z;

            float mag_yz = fast_mag(fy, fz);
            tilt_x = fast_atan2_deg(fx, mag_yz);
            tilt_y = fast_atan2_deg(fy, fz);

            // Pack raw data into binary struct
            tx_packet.ax = accel_x;
            tx_packet.ay = accel_y;
            tx_packet.az = accel_z;
            tx_packet.tilt_x = (int16_t)(tilt_x * 100.0f); // Convert to integer hundredths
            tx_packet.tilt_y = (int16_t)(tilt_y * 100.0f);

            // Copy struct memory directly to UART DMA buffer
            memcpy(uart_tx_buffer, &tx_packet, sizeof(TelemetryPacket_t));

            // Trigger DMA (Sends exactly 12 bytes)
            BSP_UART_SendData(sizeof(TelemetryPacket_t));

            BSP_I2C_Start_DMA_Read();
            vTaskDelay(pdMS_TO_TICKS(100)); // 10Hz
        }
    }
}
