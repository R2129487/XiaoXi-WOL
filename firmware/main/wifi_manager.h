/*
 * wifi_manager.h — WiFi 管理
 */
#pragma once
#include <stdbool.h>
#include <esp_err.h>

typedef enum {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_AP_MODE,
} wifi_status_t;

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_connect(const char *ssid, const char *password, bool keep_ap);
esp_err_t wifi_manager_start_ap(const char *ssid, const char *password);
wifi_status_t wifi_manager_get_status(void);
const char *wifi_manager_get_ip(void);
