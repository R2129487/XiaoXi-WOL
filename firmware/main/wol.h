/*
 * wol.h — WOL 魔术包
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#define WOL_DEFAULT_PORT    9
#define WOL_MAX_DEVICES     8

typedef struct {
    char name[32];          // 设备名称，如"工坊电脑"
    uint8_t mac[6];         // 目标MAC地址
    bool mac_valid;         // MAC是否已配置
    char broadcast_ip[16];  // 广播地址（空=255.255.255.255）
    uint16_t port;          // WOL端口，默认9
} wol_device_t;

// 发送WOL魔术包
esp_err_t wol_send(const uint8_t *mac, const char *broadcast_ip, uint16_t port);

// 发送到已配置的设备
esp_err_t wol_send_to_device(const wol_device_t *dev);
