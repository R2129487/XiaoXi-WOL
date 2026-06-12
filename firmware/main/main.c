/*
 * main.c — 小希-WOL 主程序
 *
 * 启动流程：
 * 1. 初始化 NVS + LED
 * 2. 检查 WiFi 配置
 *    - 有配置 → APSTA 双模式（连 WiFi + 保留 AP 热点）
 *    - 无配置 → 纯 AP 配网模式
 * 3. 初始化以太网（LAN8720）
 * 4. 启动 Web 服务器 + DNS（Captive Portal）
 * 5. LED 指示当前状态
 */
#include <nvs_flash.h>
#include <esp_log.h>
#include <esp_event.h>
#include "config_store.h"
#include "wifi_manager.h"
#include "eth_manager.h"
#include "web_server.h"
#include "led_indicator.h"

static const char *TAG = "WOL-Main";

void app_main(void) {
    ESP_LOGI(TAG, "=== 小希-WOL v0.1 ===");

    // 1. 配置 + LED
    config_init();
    led_init();
    led_set_state(LED_BLINK_SLOW);

    // 2. WiFi
    wifi_manager_init();
    wol_config_t *cfg = config_get();

    if (cfg->wifi_ssid[0]) {
        ESP_LOGI(TAG, "WiFi configured: %s", cfg->wifi_ssid);
        wifi_manager_connect(cfg->wifi_ssid, cfg->wifi_password, cfg->keep_ap);
    } else {
        ESP_LOGW(TAG, "No WiFi configured, starting AP mode");
        wifi_manager_start_ap("XiaoXi-WOL-Setup", NULL);
        led_set_state(LED_BLINK_DOUBLE);
    }

    // 3. 以太网（LAN8720）
    esp_err_t eth_err = eth_manager_init();
    if (eth_err != ESP_OK) {
        ESP_LOGW(TAG, "Ethernet init failed (no LAN8720 connected?) — continuing without");
    }

    // 4. Web 服务器
    web_server_init();
    web_server_start();
    web_server_start_dns();

    // 5. 状态灯
    if (cfg->wifi_ssid[0]) {
        // 等待 WiFi 连接（最多 15 秒）
        for (int i = 0; i < 15; i++) {
            if (wifi_manager_get_status() == WIFI_STATUS_CONNECTED) break;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        if (wifi_manager_get_status() == WIFI_STATUS_CONNECTED) {
            led_set_state(LED_ON);
            ESP_LOGI(TAG, "Ready! WiFi IP: %s", wifi_manager_get_ip());
        } else {
            led_set_state(LED_BLINK_SLOW);
            ESP_LOGW(TAG, "WiFi not connected, AP still active");
        }
    }

    if (eth_manager_is_connected()) {
        ESP_LOGI(TAG, "Ethernet IP: %s", eth_manager_get_ip());
    }

    ESP_LOGI(TAG, "=== 启动完成，等待指令 ===");

    // 主循环
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
