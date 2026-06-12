/*
 * web_server.h — Web 服务器
 */
#pragma once
#include <esp_err.h>

esp_err_t web_server_init(void);
esp_err_t web_server_start(void);
void web_server_start_dns(void);  // 启动 Captive Portal DNS
