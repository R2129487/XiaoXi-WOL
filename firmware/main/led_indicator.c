/*
 * led_indicator.c — LED 状态指示
 */
#include "led_indicator.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "LED";
static led_state_t s_state = LED_OFF;
static TaskHandle_t s_blink_task = NULL;
static bool s_running = false;

#ifndef CONFIG_LED_GPIO
#define CONFIG_LED_GPIO 2
#endif

static void blink_task(void *arg) {
    int interval_ms;
    while (s_running) {
        switch (s_state) {
        case LED_OFF:
            gpio_set_level(CONFIG_LED_GPIO, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        case LED_ON:
            gpio_set_level(CONFIG_LED_GPIO, 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        case LED_BLINK_SLOW:   interval_ms = 1000; break;
        case LED_BLINK_FAST:   interval_ms = 200;  break;
        case LED_BLINK_DOUBLE: interval_ms = 100;  break;
        default:               interval_ms = 500;  break;
        }
        gpio_set_level(CONFIG_LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
        gpio_set_level(CONFIG_LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }
    vTaskDelete(NULL);
}

esp_err_t led_init(void) {
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << CONFIG_LED_GPIO;
    io.mode = GPIO_MODE_OUTPUT;
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);
    gpio_set_level(CONFIG_LED_GPIO, 0);

    s_running = true;
    xTaskCreate(blink_task, "led_blink", 2048, NULL, 1, &s_blink_task);
    ESP_LOGI(TAG, "LED init (GPIO %d)", CONFIG_LED_GPIO);
    return ESP_OK;
}

void led_set_state(led_state_t state) {
    s_state = state;
}
