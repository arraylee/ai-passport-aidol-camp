// components/bsp/src/bsp_battery.c
// CW2017 电量计驱动。
#include "bsp_battery.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <string.h>

static const char *TAG = "bsp_batt";

#define CW_REG_VERSION   0x00   // 版本号,上电应答即代表芯片在位
#define CW_REG_VCELL_H   0x02   // 14bit 电压,V(uV) = raw * 312.5
#define CW_REG_SOC_H     0x04   // 高字节 = 整数百分比;低字节(0x05)= 1/256 %
#define CW_REG_CONFIG    0x08   // 0xF0=睡眠 / 0x30=复位态 / 0x00=正常
#define CW_REG_SOC_ALERT 0x0B   // bit7=profile UPDATE_FLAG;bit6:0=SOC 告警阈值
#define CW_REG_PROFILE   0x10   // 80 字节电池 profile 起始地址

#define CW_CONFIG_ACTIVE  0x00
#define CW_CONFIG_RESTART 0x30
#define CW_CONFIG_SLEEP   0xF0
#define CW_UPDATE_FLAG    0x80
#define CW_PROFILE_SIZE   80

static const uint8_t s_battery_profile[CW_PROFILE_SIZE] = {
    0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAD, 0xC7, 0xC8, 0xCA, 0xBD, 0xB1, 0xC1, 0x94,
    0x88, 0xD1, 0xBD, 0x97, 0x88, 0x66, 0x56, 0x4A,
    0x3F, 0x33, 0x26, 0x5C, 0x37, 0xD1, 0x27, 0xD8,
    0xCC, 0xB7, 0xCF, 0xB3, 0xB2, 0xAE, 0xA6, 0x9E,
    0x99, 0x97, 0x9B, 0x86, 0x47, 0x1E, 0x17, 0x26,
    0x49, 0x96, 0xD9, 0xE1, 0xDD, 0xDC, 0xD4, 0x59,
    0x00, 0x00, 0x90, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5C,
};

_Static_assert(sizeof(s_battery_profile) == CW_PROFILE_SIZE,
               "CW2017 battery profile must contain exactly 80 bytes");

static i2c_master_dev_handle_t s_dev;
enum { ATTACH, READ_CONFIG, READ_ALERT, READ_PROFILE, CHECK_PROFILE,
       CLEAR_FLAG, VERIFY_CLEAR_FLAG, SLEEP_RESTART, SLEEP_CONFIG, WRITE_PROFILE, VERIFY_PROFILE,
       WRITE_FLAG, VERIFY_FLAG, WAKE_RESTART, WAKE_ACTIVE, SETTLE, READY, RETRY };
static struct {
    uint8_t phase, index, config, alert, version, failures, profile_kind;
    uint8_t profile[CW_PROFILE_SIZE];
    uint32_t due;
    bool waiting, prepared, own_repair, write_profile, restart_requested;
    int raw_soc, last_error;
} gauge = {.raw_soc = -1};
static int cw_read(uint8_t reg, uint8_t *buf, size_t n) {
    if (!s_dev) return -1;
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, n, 5) == ESP_OK ? 0 : -1;
}
static int cw_write(uint8_t reg, uint8_t value) {
    if (!s_dev) return -1;
    uint8_t b[2] = {reg, value};
    return i2c_master_transmit(s_dev, b, 2, 5) == ESP_OK ? 0 : -1;
}
static void wait_for(uint8_t phase, uint32_t now, uint32_t delay) {
    gauge.phase = phase; gauge.waiting = true; gauge.due = now + delay;
}
static esp_err_t failed(uint32_t now, esp_err_t error) {
    if (gauge.failures < 6) gauge.failures++;
    uint32_t delay = gauge.failures < 5 ? 1000u << (gauge.failures - 1) : 30000u;
    gauge.prepared = false; gauge.last_error = error;
    wait_for(RETRY, now, delay);
    ESP_LOGW(TAG, "gauge unavailable error=%d; retry in %lu ms", error, (unsigned long)delay);
    return error;
}
void bsp_battery_recover(void) {
    /* Preserve own_repair until its verified transaction finishes. */
    if (gauge.prepared) gauge.restart_requested = true;
    gauge.phase = ATTACH; gauge.waiting = false; gauge.prepared = false;
}
esp_err_t bsp_battery_probe(void) {
    if (s_dev) return ESP_OK;
    esp_err_t e = bsp_i2c_init();
    if (e != ESP_OK) return e;
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BSP_I2C_CW2017_ADDR, .scl_speed_hz = 100000,
    };
    e = i2c_master_bus_add_device(bsp_i2c_bus(), &cfg, &s_dev);
    if (e != ESP_OK) { s_dev = NULL; return e; }
    uint8_t version = 0;
    if (cw_read(CW_REG_VERSION, &version, 1) == 0 && version != 0 && version != 0xff) {
        gauge.version = version;
        return ESP_OK;
    }
    i2c_master_bus_rm_device(s_dev); s_dev = NULL;
    return ESP_ERR_NOT_FOUND;
}
static bool all_bytes(const uint8_t *bytes, size_t count, uint8_t value) {
    for (size_t i = 0; i < count; i++) if (bytes[i] != value) return false;
    return true;
}
static bool interrupted_profile(void) {
    /* Recognize only our exact sequential prefix and an untouched blank tail. */
    for (unsigned cut = 1; cut < CW_PROFILE_SIZE; cut++) {
        if (memcmp(gauge.profile, s_battery_profile, cut) != 0) continue;
        if (all_bytes(gauge.profile + cut, CW_PROFILE_SIZE - cut, 0) ||
            all_bytes(gauge.profile + cut, CW_PROFILE_SIZE - cut, 0xff)) return true;
    }
    return false;
}
static esp_err_t prepared(void) {
    gauge.phase = READY; gauge.prepared = true; gauge.waiting = false;
    gauge.own_repair = false; gauge.failures = 0; gauge.last_error = 0;
    gauge.restart_requested = false;
    return ESP_OK;
}
esp_err_t bsp_battery_service(uint32_t now) {
    uint8_t value = 0;
    if (gauge.waiting) {
        if ((int32_t)(now - gauge.due) < 0) return ESP_ERR_NOT_FINISHED;
        gauge.waiting = false;
    }
    /* One bounded I2C transaction at most. Never delay, spin or allocate a task. */
    switch (gauge.phase) {
    case ATTACH:
        if (bsp_battery_probe() != ESP_OK) return failed(now, ESP_ERR_NOT_FOUND);
        gauge.phase = READ_CONFIG; break;
    case READ_CONFIG:
        if (cw_read(CW_REG_CONFIG, &gauge.config, 1)) return failed(now, ESP_FAIL);
        gauge.phase = READ_ALERT; break;
    case READ_ALERT:
        if (cw_read(CW_REG_SOC_ALERT, &gauge.alert, 1)) return failed(now, ESP_FAIL);
        gauge.index = 0; gauge.phase = READ_PROFILE; break;
    case READ_PROFILE:
        if (cw_read(CW_REG_PROFILE + gauge.index, &gauge.profile[gauge.index], 1))
            return failed(now, ESP_FAIL);
        if (++gauge.index == CW_PROFILE_SIZE) gauge.phase = CHECK_PROFILE;
        break;
    case CHECK_PROFILE: {
        bool blank = all_bytes(gauge.profile, CW_PROFILE_SIZE, 0) ||
                     all_bytes(gauge.profile, CW_PROFILE_SIZE, 0xff);
        bool same = memcmp(gauge.profile, s_battery_profile, CW_PROFILE_SIZE) == 0;
        bool committed = (gauge.alert & CW_UPDATE_FLAG) != 0;
        if (!blank && committed && (!gauge.own_repair || same)) {
            /* A different, committed calibration belongs to this device. */
            gauge.profile_kind = 1;
            if (gauge.config == CW_CONFIG_ACTIVE && !gauge.restart_requested) return prepared();
            gauge.phase = WAKE_RESTART;
        } else if (same || blank || gauge.own_repair || interrupted_profile()) {
            gauge.profile_kind = 2; gauge.own_repair = true;
            gauge.write_profile = !same;
            gauge.phase = committed ? CLEAR_FLAG : SLEEP_RESTART;
            ESP_LOGI(TAG, "repair empty/interrupted gauge profile; preserve device Flash/SN");
        } else {
            gauge.profile_kind = 3;
            ESP_LOGW(TAG, "unrecognized uncommitted profile retained; no blind overwrite");
            return failed(now, ESP_ERR_INVALID_STATE);
        }
        break;
    }
    case CLEAR_FLAG:
        if (cw_write(CW_REG_SOC_ALERT, gauge.alert & ~CW_UPDATE_FLAG)) return failed(now, ESP_FAIL);
        gauge.phase = VERIFY_CLEAR_FLAG; break;
    case VERIFY_CLEAR_FLAG:
        if (cw_read(CW_REG_SOC_ALERT, &value, 1) || (value & CW_UPDATE_FLAG))
            return failed(now, ESP_FAIL);
        gauge.alert = value; gauge.phase = SLEEP_RESTART; break;
    case SLEEP_RESTART:
        if (cw_write(CW_REG_CONFIG, CW_CONFIG_RESTART)) return failed(now, ESP_FAIL);
        wait_for(SLEEP_CONFIG, now, 20); break;
    case SLEEP_CONFIG:
        if (cw_write(CW_REG_CONFIG, CW_CONFIG_SLEEP)) return failed(now, ESP_FAIL);
        gauge.index = 0;
        wait_for(gauge.write_profile ? WRITE_PROFILE : WRITE_FLAG, now, 10); break;
    case WRITE_PROFILE:
        if (cw_write(CW_REG_PROFILE + gauge.index, s_battery_profile[gauge.index]))
            return failed(now, ESP_FAIL);
        if (++gauge.index == CW_PROFILE_SIZE) {
            gauge.index = 0; gauge.phase = VERIFY_PROFILE;
        }
        break;
    case VERIFY_PROFILE:
        if (cw_read(CW_REG_PROFILE + gauge.index, &value, 1) ||
            value != s_battery_profile[gauge.index]) return failed(now, ESP_FAIL);
        if (++gauge.index == CW_PROFILE_SIZE) gauge.phase = WRITE_FLAG;
        break;
    case WRITE_FLAG:
        if (cw_write(CW_REG_SOC_ALERT, gauge.alert | CW_UPDATE_FLAG)) return failed(now, ESP_FAIL);
        gauge.phase = VERIFY_FLAG; break;
    case VERIFY_FLAG:
        if (cw_read(CW_REG_SOC_ALERT, &value, 1) || !(value & CW_UPDATE_FLAG))
            return failed(now, ESP_FAIL);
        gauge.phase = WAKE_RESTART; break;
    case WAKE_RESTART:
        if (cw_write(CW_REG_CONFIG, CW_CONFIG_RESTART)) return failed(now, ESP_FAIL);
        wait_for(WAKE_ACTIVE, now, 20); break;
    case WAKE_ACTIVE:
        if (cw_write(CW_REG_CONFIG, CW_CONFIG_ACTIVE)) return failed(now, ESP_FAIL);
        gauge.config = CW_CONFIG_ACTIVE;
        wait_for(SETTLE, now, 10); break;
    case SETTLE: return prepared();
    case READY: return ESP_OK;
    case RETRY: gauge.phase = ATTACH; break;
    default: bsp_battery_recover(); break;
    }
    return ESP_ERR_NOT_FINISHED;
}
esp_err_t bsp_battery_init(void) {
    /* Legacy blocking entry. Foreground firmware uses service() instead. */
    bsp_battery_recover();
    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    do {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (bsp_battery_service(now) == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(5));
    } while ((uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS - start) < 5000);
    return ESP_ERR_TIMEOUT;
}
int bsp_battery_soc(void) {
    if (!gauge.prepared) return -1;
    uint8_t bytes[2];
    if (cw_read(CW_REG_SOC_H, bytes, 2)) { gauge.raw_soc = -1; return -1; }
    gauge.raw_soc = bytes[0];
    return bytes[0] <= 100 ? bytes[0] : -1;
}
int bsp_battery_mv(void) {
    uint8_t bytes[2];
    if (cw_read(CW_REG_VCELL_H, bytes, 2)) return -1;
    return (((bytes[0] & 0x3f) << 8) | bytes[1]) * 5 / 16;
}
static int cw_enter_sleep(void) {
    if (cw_write(CW_REG_CONFIG, CW_CONFIG_RESTART)) return -1;
    vTaskDelay(pdMS_TO_TICKS(20));
    if (cw_write(CW_REG_CONFIG, CW_CONFIG_SLEEP)) return -1;
    vTaskDelay(pdMS_TO_TICKS(10));
    return 0;
}
static int cw_enter_active(void) {
    if (cw_write(CW_REG_CONFIG, CW_CONFIG_RESTART)) return -1;
    vTaskDelay(pdMS_TO_TICKS(20));
    if (cw_write(CW_REG_CONFIG, CW_CONFIG_ACTIVE)) return -1;
    vTaskDelay(pdMS_TO_TICKS(10));
    return 0;
}
esp_err_t bsp_battery_sleep(void) {
    bsp_battery_recover();
    return s_dev && !cw_enter_sleep() ? ESP_OK : ESP_FAIL;
}
esp_err_t bsp_battery_wake(void) {
    bsp_battery_recover();
    uint8_t config;
    if (cw_read(CW_REG_CONFIG, &config, 1)) return ESP_FAIL;
    if (config != CW_CONFIG_ACTIVE && cw_enter_active()) return ESP_FAIL;
    gauge.restart_requested = false;
    return ESP_OK;
}
void bsp_battery_report(void) {
    ESP_LOGI(TAG, "gauge ver=0x%02x phase=%u prepared=%u profile=%u raw_soc=%d err=%d retries=%u",
             gauge.version, gauge.phase, gauge.prepared, gauge.profile_kind,
             gauge.raw_soc, gauge.last_error, gauge.failures);
}
