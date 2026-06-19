#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/uart.h"
#include "esp_netif.h"

#define WIFI_SSID "valmtv"
#define WIFI_PASS "12345678"

#define UART_NUM  UART_NUM_2
#define UART_RX   16
#define UART_TX   17
#define BUF_SIZE  256

static const char *TAG = "BMA180";

// Binary Struct Definition (Must match STM32 exactly)
typedef struct __attribute__((packed)) {
    uint16_t sync;
    int16_t ax, ay, az;
    int16_t tilt_x, tilt_y;
} TelemetryPacket_t;

// Global variables for web server
static int16_t ax = 0, ay = 0, az = 0;
static float tx = 0, ty = 0;

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) esp_wifi_connect();
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init(void) {
    wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_handler, NULL);
    wifi_config_t wifi_cfg = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS } };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

static esp_err_t root_handler(httpd_req_t *req) {
    const char *html =
        "<!DOCTYPE html><html><head><title>BMA180 Telemetry</title><meta charset='utf-8'>"
        "<style>body{font-family:sans-serif;text-align:center;margin-top:50px;background:#1e1e1e;color:#fff;}"
        "h1{color:#ccc;} .val{font-size:3em;color:#00ff00;} .accel{font-size:2em;color:#00ccff;}</style>"
        "<script>setInterval(()=>{fetch('/data').then(r=>r.json()).then(d=>{"
        "document.getElementById('tx').innerText=d.tx.toFixed(2);"
        "document.getElementById('ty').innerText=d.ty.toFixed(2);"
        "document.getElementById('ax').innerText=d.ax;"
        "document.getElementById('ay').innerText=d.ay;"
        "document.getElementById('az').innerText=d.az;});},100);"
        "</script></head><body>"
        "<h1>STM32F429 + BMA180 TELEMETRY</h1>"
        "<h2>Tilt X: <span class='val' id='tx'>--</span>&deg; | Tilt Y: <span class='val' id='ty'>--</span>&deg;</h2>"
        "<h3>Raw Accel -> X: <span class='accel' id='ax'>--</span> | Y: <span class='accel' id='ay'>--</span> | Z: <span class='accel' id='az'>--</span></h3>"
        "</body></html>";
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

static esp_err_t data_handler(httpd_req_t *req) {
    char buf[128];
    // ESP32 formats the JSON string here, not the STM32
    snprintf(buf, sizeof(buf), "{\"ax\":%d,\"ay\":%d,\"az\":%d,\"tx\":%.2f,\"ty\":%.2f}", ax, ay, az, tx, ty);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, strlen(buf));
    return ESP_OK;
}

static void uart_task(void *pvParameters) {
    TelemetryPacket_t pkt;
    while (1) {
        // Read exactly 12 bytes (the size of the struct)
        int len = uart_read_bytes(UART_NUM, (uint8_t*)&pkt, sizeof(TelemetryPacket_t), pdMS_TO_TICKS(100));
        
        // Validate sync word to ensure we haven't desynchronized
        if (len == sizeof(TelemetryPacket_t) && pkt.sync == 0xAA55) {
            ax = pkt.ax; 
            ay = pkt.ay; 
            az = pkt.az;
            tx = (float)pkt.tilt_x / 100.0f; // Convert back to float for web display
            ty = (float)pkt.tilt_y / 100.0f;
        }
    }
}

void app_main(void) {
    nvs_flash_init();
    uart_config_t uart_cfg = {
        .baud_rate = 115200, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_cfg);
    uart_set_pin(UART_NUM, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    wifi_init();

    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_start(&server, &config);
    httpd_uri_t root = { .uri="/", .method=HTTP_GET, .handler=root_handler };
    httpd_uri_t data = { .uri="/data", .method=HTTP_GET, .handler=data_handler };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &data);

    xTaskCreate(uart_task, "uart", 4096, NULL, 5, NULL);
}