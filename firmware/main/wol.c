/*
 * wol.c — WOL 魔术包发送
 * 小希-WOL 固件核心功能
 */
#include "wol.h"
#include <string.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <esp_log.h>

static const char *TAG = "WOL";

// 构建 WOL 魔术包：6字节0xFF + 目标MAC重复16次
static int build_magic_packet(const uint8_t *mac, uint8_t *packet) {
    // 6 bytes of 0xFF
    memset(packet, 0xFF, 6);
    // 16 repetitions of target MAC
    for (int i = 0; i < 16; i++) {
        memcpy(packet + 6 + i * 6, mac, 6);
    }
    return 6 + 16 * 6;  // = 102 bytes
}

esp_err_t wol_send(const uint8_t *mac, const char *broadcast_ip, uint16_t port) {
    if (!mac) {
        ESP_LOGE(TAG, "MAC address is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t packet[102];
    int packet_len = build_magic_packet(mac, packet);

    ESP_LOGI(TAG, "Sending WOL to %02X:%02X:%02X:%02X:%02X:%02X via %s:%u",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
             broadcast_ip ? broadcast_ip : "255.255.255.255", port);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket create failed: %d", errno);
        return ESP_FAIL;
    }

    // 允许广播
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    dest.sin_addr.s_addr = broadcast_ip
        ? inet_addr(broadcast_ip)
        : INADDR_BROADCAST;

    int ret = sendto(sock, packet, packet_len, 0,
                     (struct sockaddr *)&dest, sizeof(dest));
    close(sock);

    if (ret < 0) {
        ESP_LOGE(TAG, "sendto failed: %d", errno);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WOL magic packet sent (%d bytes)", ret);
    return ESP_OK;
}

esp_err_t wol_send_to_device(const wol_device_t *dev) {
    if (!dev || !dev->mac_valid) {
        ESP_LOGE(TAG, "Device not configured");
        return ESP_ERR_INVALID_STATE;
    }
    return wol_send(dev->mac, dev->broadcast_ip[0] ? dev->broadcast_ip : NULL, dev->port);
}
