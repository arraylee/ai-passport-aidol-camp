// components/bsp/src/bsp_display_lvgl.c
// LVGL 接入单独成文件:不用 LVGL 的开发者删掉本文件 + idf_component.yml 里的两条依赖即可。
#include "bsp_display.h"
#include "bsp_pins.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include <stdatomic.h>

static const char *TAG = "bsp_lvgl";

static lv_display_t *s_disp;
static atomic_bool s_first_ready;
static bool s_first_armed, s_first_registered, s_suspended;
static unsigned s_first_failures;

lv_display_t *bsp_lvgl_init(void) {
    if (s_disp) return s_disp;
    if (!bsp_display_panel()) {
        ESP_LOGE(TAG, "请先成功调用 bsp_display_init()");
        return NULL;
    }

    const lvgl_port_cfg_t pc = ESP_LVGL_PORT_INIT_CONFIG();
    if (lvgl_port_init(&pc) != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init 失败");
        return NULL;
    }

    const lvgl_port_display_cfg_t dc = {
        .panel_handle = bsp_display_panel(),
        .io_handle    = bsp_display_io(),
        // ⚠ C3 无 PSRAM,DMA 只能用内部 RAM(总共约 150KB)。
        // 20 行单缓冲 ≈ 9.6KB;若改成 40 行双缓冲(≈37.5KB)会把 I2S 等外设的
        // DMA 描述符挤到 NO_MEM。刷新略慢但稳。
        .buffer_size   = (uint32_t)BSP_LCD_W * 20,
        .double_buffer = false,
        .hres = BSP_LCD_W, .vres = BSP_LCD_H,
        // 旋转/镜像必须在这里配:esp_lvgl_port 注册显示时会重新下发 MADCTL,
        // 覆盖 bsp_display.c 里 esp_lcd_panel_mirror() 的设置。
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        // swap_bytes:LVGL 输出小端 RGB565,ST7789 走 SPI 要大端 → 需交换高低字节。
        .flags = { .buff_dma = true, .swap_bytes = true },
    };
    s_disp = lvgl_port_add_disp(&dc);
    if (!s_disp) { ESP_LOGE(TAG, "lvgl_port_add_disp 失败"); return NULL; }

    ESP_LOGI(TAG, "LVGL 就绪");
    return s_disp;
}

bool bsp_lvgl_lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }
void bsp_lvgl_unlock(void)         { lvgl_port_unlock(); }

static void first_frame_ready(lv_event_t *event) {
    (void)event;
    if (!s_first_armed) return;
    // REFR_READY plus this parameter command drains the final SPI DMA.
    if (esp_lcd_panel_disp_on_off(bsp_display_panel(), true) != ESP_OK) {
        if (++s_first_failures < 3) {
            lv_obj_invalidate(lv_screen_active());
            return;
        }
        // init already issued DISPON. After three failed redundant commands
        // allow the completed frame's backlight instead of a permanent blackout.
        ESP_LOGW(TAG, "First frame ready; DISPON retry limit reached");
    }
    s_first_armed = false;
    atomic_store(&s_first_ready, true);
}
void bsp_lvgl_arm_first_frame(void) {
    if (!s_disp) return;
    atomic_store(&s_first_ready, false);
    s_first_armed = true;
    s_first_failures = 0;
    if (!s_first_registered) {
        lv_display_add_event_cb(s_disp, first_frame_ready, LV_EVENT_REFR_READY, NULL);
        s_first_registered = true;
    }
    lv_obj_invalidate(lv_screen_active());
}
bool bsp_lvgl_first_frame_ready(void) {
    return atomic_load(&s_first_ready);
}
bool bsp_lvgl_suspend(int timeout_ms) {
    if (!s_disp || s_suspended || timeout_ms <= 0 ||
        !lvgl_port_lock(timeout_ms)) return false;
    if (lvgl_port_stop() != ESP_OK) {
        (void)lvgl_port_resume();
        lvgl_port_unlock();
        return false;
    }
    s_suspended = true;
    return true;
}
void bsp_lvgl_resume(void) {
    if (!s_suspended) return;
    (void)lvgl_port_resume();
    s_suspended = false;
    lv_obj_invalidate(lv_screen_active());
    lvgl_port_unlock();
}
