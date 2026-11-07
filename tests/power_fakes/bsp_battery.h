#pragma once
#include "esp_err.h"
#include <stdint.h>
esp_err_t bsp_battery_service(uint32_t now);
void bsp_battery_recover(void);
esp_err_t bsp_battery_probe(void);
esp_err_t bsp_battery_sleep(void);
esp_err_t bsp_battery_wake(void);
int bsp_battery_soc(void);
