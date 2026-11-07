// components/bsp/include/bsp_battery.h
// CellWise CW2017 电量计:I2C 0x63,与 ES8311 共用总线。
// 仅为空参数/可识别的中断写入补齐 Aidol 4.2V 520mAh profile；正常校准保留。
#pragma once

#include "esp_err.h"
#include <stdint.h>

// 初始化。内部会调 bsp_i2c_init()(幂等)。
// 最多等待5秒；前台任务请使用下面的异步service，不要阻塞主循环。
esp_err_t bsp_battery_init(void);

// 剩余电量百分比 0..100;读失败返回 -1。
int bsp_battery_soc(void);

// 电池电压 mV;读失败返回 -1。
int bsp_battery_mv(void);

// Attach/read only; unlike init(), never writes profile/calibration data.
esp_err_t bsp_battery_probe(void);
esp_err_t bsp_battery_sleep(void);
esp_err_t bsp_battery_wake(void);

// Foreground-safe: at most one 5ms I2C transaction, no sleeping. ESP_OK when prepared.
// Empty or recognized interrupted profiles are repaired and verified; committed
// non-empty device calibrations are preserved. SOC may still be calculating.
esp_err_t bsp_battery_service(uint32_t now_ms);
// Retry configuration after persistent invalid SOC; restart a previously ready
// gauge calculation without rewriting its committed calibration. No Flash writes.
void bsp_battery_recover(void);
// Cached diagnostics only, no I2C.
void bsp_battery_report(void);
