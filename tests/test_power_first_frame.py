"""Exercise the actual BSP first-frame callbacks with a narrow display fake."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'components/bsp/src/bsp_display_lvgl.c').read_text()

def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
typedef int lv_event_t;
#define ESP_OK 0
#define LV_EVENT_REFR_READY 1
#define ESP_LOGW(tag, fmt, ...) ((void)0)
static int display;
static int *s_disp = &display;
static atomic_bool s_first_ready;
static bool s_first_armed, s_first_registered;
static unsigned s_first_failures;
static unsigned calls, failures, invalidations, registrations;
static int bsp_display_panel(void) { return 1; }
static int esp_lcd_panel_disp_on_off(int panel, bool on) {
    assert(panel == 1 && on); calls++; return calls <= failures ? -1 : 0;
}
static void *lv_screen_active(void) { return &display; }
static void lv_obj_invalidate(void *screen) { assert(screen == &display); invalidations++; }
static void lv_display_add_event_cb(int *d, void (*cb)(lv_event_t *), int event, void *data) {
    assert(d == &display && cb && event == LV_EVENT_REFR_READY && !data); registrations++;
}
'''
body = '\n'.join(function(s) for s in (
    'static void first_frame_ready(', 'void bsp_lvgl_arm_first_frame(',
    'bool bsp_lvgl_first_frame_ready('))
main = r'''
int main(void) {
    first_frame_ready(NULL); assert(!bsp_lvgl_first_frame_ready() && !calls);
    failures = 2;
    bsp_lvgl_arm_first_frame(); assert(!bsp_lvgl_first_frame_ready());
    first_frame_ready(NULL); assert(!bsp_lvgl_first_frame_ready());
    first_frame_ready(NULL); assert(!bsp_lvgl_first_frame_ready());
    first_frame_ready(NULL); assert(bsp_lvgl_first_frame_ready() && calls == 3);
    first_frame_ready(NULL); assert(calls == 3);
    calls = 0; failures = 100;
    bsp_lvgl_arm_first_frame();
    first_frame_ready(NULL); first_frame_ready(NULL); first_frame_ready(NULL);
    assert(bsp_lvgl_first_frame_ready() && calls == 3 && registrations == 1);
    first_frame_ready(NULL); assert(calls == 3);
    puts("PASS actual BSP complete-frame latch, bounded DISPON retry, single callback registration");
}
'''
with tempfile.TemporaryDirectory(prefix='hr-power-frame-') as d:
    path = Path(d)
    (path / 'test.c').write_text(prefix + body + main)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
