#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
void bsp_display_backlight(uint8_t value);
void bsp_lvgl_arm_first_frame(void);
bool bsp_lvgl_first_frame_ready(void);
bool bsp_lvgl_suspend(int timeout_ms);
void bsp_lvgl_resume(void);
esp_err_t bsp_display_sleep(void);
esp_err_t bsp_display_wake(void);
void bsp_display_sleep_hold(void);
void bsp_display_sleep_unhold(void);
