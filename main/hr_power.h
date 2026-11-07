#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifndef HR_POWER_IDLE_MS
#define HR_POWER_IDLE_MS 1200000u
#endif
#define HR_POWER_GOODBYE_MS 1200u
/* Main-task only. stop must quiesce audio/radio/BLE with bounded waits.
 * resume must undo even a partial stop. Neither callback may use LVGL. */
typedef bool (*hr_power_stop_fn)(void *context);
typedef void (*hr_power_resume_fn)(void *context);
void hr_power_init(uint32_t now, hr_power_stop_fn stop,
                   hr_power_resume_fn resume, void *context);
/* Cached CW2017 SOC, 0..100 or -1 before a valid sample / after 30s stale.
 * Bounded foreground service retries discovery/profile repair; SOC is polled
 * every 5s when healthy, every 1s while unknown/failing, outside LVGL. */
int hr_power_battery(void);
/* Call once after rendering the restored home, while holding the LVGL lock.
 * Actual backlight fade waits for LV_EVENT_REFR_READY, not just object updates.
 */
void hr_power_first_frame(uint32_t now);
/* Busy refreshes idle time; discovery traffic must not count as activity.
 * Include physical held keys, genuine audio, uploads, card/NVS writes and
 * connected-phone editing in busy. Call after processing work each loop. */
void hr_power_tick(uint32_t now, bool activity, bool busy);
/* Feed raw ADC every loop. False consumes the boot/waking press until 30 ms
 * stable neutral. Keep hr_input unarmed/reset while false; do not synthesize
 * a release event. After true, feed that neutral sample to hr_input normally.
 */
bool hr_power_filter_input(int millivolts, uint32_t now);

/* Present JOY before fading. Any activity/busy state cancels it safely. */
bool hr_power_goodbye(uint32_t now, uint32_t *elapsed);
