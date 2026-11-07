#pragma once
#include "esp_err.h"
#include <stdint.h>
#define ESP_GPIO_WAKEUP_GPIO_LOW 0
#define ESP_SLEEP_WAKEUP_GPIO 7
esp_err_t esp_deep_sleep_enable_gpio_wakeup(uint64_t pins,int level);
esp_err_t esp_sleep_disable_wakeup_source(int source);
esp_err_t esp_deep_sleep_try_to_start(void);
