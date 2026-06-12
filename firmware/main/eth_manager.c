/*
 * eth_manager.c — 以太网管理（LAN8720 RMII）
 *
 * LAN8720 接线（ESP32 + LAN8720 模块常见接法）：
 *   ESP32 GPIO  →  LAN8720
 *   ─────────────────────
 *   GPIO21      →  MDIO
 *   GPIO22      →  MDC (部分模块用GPIO23)
 *   GPIO25      →  RXD0
 *   GPIO26      →  RXD1
 *   GPIO27      →  CRS_DV
 *   GPIO19      →  TXD0 (部分模块用GPIO17)
 *   GPIO22/18   →  TXD1
 *   GPIO17      →  CLK_OUT (50MHz from ESP32 to LAN8720)
 *   nRST        →  3.3V 或 GPIO（可选）
 *
 * 注：具体引脚可能因模块而异，需根据实际硬件调整
 */
#include "eth_manager.h"
#include <esp_eth.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "ETH";

static esp_eth_handle_t s_eth_handle = NULL;
static esp_netif_t *s_eth_netif = NULL;
static bool s_connected = false;
static char s_ip_str[16] = "0.0.0.0";

static void eth_event_handler(void *arg, esp_event_base_t base,
                              int32_t id, void *data) {
    switch (id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Up");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        s_connected = false;
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        s_connected = false;
        break;
    default:
        break;
    }
}

static void got_ip_handler(void *arg, esp_event_base_t base,
                           int32_t id, void *data) {
    ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
    snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&evt->ip_info.ip));
    s_connected = true;
    ESP_LOGI(TAG, "Ethernet Got IP: %s", s_ip_str);
}

esp_err_t eth_manager_init(void) {
    // 创建以太网 netif
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&netif_cfg);

    // 安装以太网驱动（LAN8720）
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;  // LAN8720 默认 PHY 地址
    phy_config.reset_gpio_num = -1;  // 不使用硬件复位

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_err_t err = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet driver install failed: %s", esp_err_to_name(err));
        return err;
    }

    // 注册事件
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                                &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                                &got_ip_handler, NULL));

    // 绑定 netif
    esp_netif_attach(s_eth_netif, s_eth_handle);
    esp_eth_start(s_eth_handle);

    ESP_LOGI(TAG, "Ethernet initialized (LAN8720)");
    return ESP_OK;
}

bool eth_manager_is_connected(void) { return s_connected; }
const char *eth_manager_get_ip(void) { return s_ip_str; }
