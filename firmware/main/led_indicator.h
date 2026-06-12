/*
 * led_indicator.h — LED 指示灯
 */
#pragma once
#include <esp_err.h>

typedef enum {
    LED_OFF,
    LED_ON,
    LED_BLINK_SLOW,    // 1s间隔 — WiFi连接中
    LED_BLINK_FAST,    // 200ms间隔 — 正在发送WOL
    LED_BLINK_DOUBLE,  // 100ms间隔 — AP配网模式
} led_state_t;

esp_err_t led_init(void);
void led_set_state(led_state_t state);
