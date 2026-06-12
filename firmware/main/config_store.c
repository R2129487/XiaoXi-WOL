/*
 * config_store.c — NVS 配置存储
 */
#include "config_store.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "Config";
static const char *NS = "wol";
static wol_config_t s_cfg;

esp_err_t config_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS erase + reinit");
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 默认值
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.keep_ap = true;

    // 从 NVS 加载
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(s_cfg);
        if (nvs_get_blob(h, "config", &s_cfg, &len) == ESP_OK) {
            ESP_LOGI(TAG, "Config loaded: %d devices", s_cfg.device_count);
        } else {
            ESP_LOGW(TAG, "NVS empty, using defaults");
        }
        nvs_close(h);
    }

    ESP_LOGI(TAG, "WiFi: %s", s_cfg.wifi_ssid[0] ? s_cfg.wifi_ssid : "(未配置)");
    return ESP_OK;
}

wol_config_t *config_get(void) {
    return &s_cfg;
}

esp_err_t config_save(void) {
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed");
        return ESP_FAIL;
    }
    nvs_set_blob(h, "config", &s_cfg, sizeof(s_cfg));
    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Config saved");
    return ESP_OK;
}
