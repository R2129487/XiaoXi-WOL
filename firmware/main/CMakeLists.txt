idf_component_register(
    SRCS "main.c"
         "wol.c"
         "wifi_manager.c"
         "web_server.c"
         "eth_manager.c"
         "led_indicator.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_netif esp_http_server nvs_flash esp_event esp_eth driver esp_timer
)
