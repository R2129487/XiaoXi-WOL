/*
 * eth_manager.h — 以太网管理（LAN8720）
 */
#pragma once
#include <esp_err.h>
#include <stdbool.h>

esp_err_t eth_manager_init(void);
bool eth_manager_is_connected(void);
const char *eth_manager_get_ip(void);
