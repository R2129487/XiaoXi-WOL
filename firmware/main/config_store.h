/*
 * config_store.h — NVS 配置存储
 */
#pragma once
#include "wol.h"
#include <esp_err.h>

#define WOL_WIFI_SSID_LEN   64
#define WOL_WIFI_PWD_LEN    64

typedef struct {
    char wifi_ssid[WOL_WIFI_SSID_LEN];
    char wifi_password[WOL_WIFI_PWD_LEN];
    bool keep_ap;                   // WiFi连上后保留AP
    int device_count;
    wol_device_t devices[WOL_MAX_DEVICES];
} wol_config_t;

esp_err_t config_init(void);
wol_config_t *config_get(void);
esp_err_t config_save(void);
