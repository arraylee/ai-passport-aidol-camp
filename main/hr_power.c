#include "hr_power.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include <string.h>
static const char *TAG = "power";
#ifdef ESP_PLATFORM
extern void hr_power_record_sleep(void);
#endif
static struct {
  uint32_t idle_at, fade_at, neutral_at;
  uint32_t battery_at, battery_valid_at, battery_recover_at, now;
  int battery_percent;
  hr_power_stop_fn stop;
  hr_power_resume_fn resume;
  void *context;
  uint8_t light, fade_from;
  enum { DARK, WAIT_FRAME, RISING, ON, GOODBYE, FALLING } phase;
  bool input_ready, neutral, battery_sampled, battery_valid, battery_recovering;
  unsigned battery_errors;
} power;
static void sample_battery(uint32_t now) {
  int percent = bsp_battery_soc();
  if (percent >= 0 && percent <= 100) {
    power.battery_percent = percent;
    power.battery_valid_at = now;
    power.battery_valid = true;
    power.battery_errors = 0;
  } else {
    if (power.battery_errors < 3) power.battery_errors++;
    if (power.battery_errors >= 3 && (!power.battery_recovering ||
        (uint32_t)(now - power.battery_recover_at) >= 30000)) {
      bsp_battery_recover();
      power.battery_recover_at = now;
      power.battery_recovering = true;
    }
  }
  power.battery_sampled = true;
  power.battery_at = now;
}
int hr_power_battery(void) {
  return power.battery_valid &&
      (uint32_t)(power.now - power.battery_valid_at) < 30000
      ? power.battery_percent : -1;
}
static void light(unsigned value) {
  if (value != power.light) {
    power.light = value;
    bsp_display_backlight(value);
  }
}
static void rise(uint32_t now) {
  power.fade_from = power.light;
  power.fade_at = now;
  power.phase = RISING;
}
void hr_power_init(uint32_t now, hr_power_stop_fn stop,
                   hr_power_resume_fn resume, void *context) {
  memset(&power, 0, sizeof power);
  power.battery_percent = -1;
  power.now = now;
  power.idle_at = now;
  power.stop = stop;
  power.resume = resume;
  power.context = context;
  bsp_display_backlight(0);
  // Inspect/repair progressively on the shared I2C bus, without blocking boot.
  bsp_battery_recover();
  if (bsp_battery_service(now) == ESP_OK) sample_battery(now);
}
void hr_power_first_frame(uint32_t now) {
  if (power.phase != DARK)
    return;
  power.idle_at = now;
  bsp_lvgl_arm_first_frame();
  power.phase = WAIT_FRAME;
}
bool hr_power_filter_input(int mv, uint32_t now) {
  if (power.input_ready)
    return true;
  if (mv < 1900) {
    power.neutral = false;
    return false;
  }
  if (!power.neutral) {
    power.neutral = true;
    power.neutral_at = now;
  }
  if ((uint32_t)(now - power.neutral_at) < 30)
    return false;
  power.input_ready = true;
  return true;
}
static void sleep_attempt(void) {
  bool display_locked = false, platform_stopped = false,
       panel_attempted = false;
  if (!power.stop || !power.resume)
    return; // Never enter sleep without a platform quiescence/recovery
            // contract.
  ESP_LOGI(TAG, "sleep step: configure wake");
  if (esp_deep_sleep_enable_gpio_wakeup(1ULL, ESP_GPIO_WAKEUP_GPIO_LOW) !=
      ESP_OK)
    goto recover;
  ESP_LOGI(TAG, "sleep step: prepare button");
  if (!bsp_button_prepare_sleep())
    goto recover;
  ESP_LOGI(TAG, "sleep step: suspend display task");
  if (!bsp_lvgl_suspend(100))
    goto recover;
  display_locked = true;
  platform_stopped =
      true; // stop may fail after partially stopping peripherals.
  ESP_LOGI(TAG, "sleep step: stop peripherals");
  if (!power.stop(power.context))
    goto recover;
  ESP_LOGI(TAG, "sleep step: peripherals stopped; panel off");
  panel_attempted = true;
  if (bsp_display_sleep() != ESP_OK)
    goto recover;
  ESP_LOGI(TAG, "sleep step: panel off complete; battery sleep");
  // Missing/failed gauge does not make a safe MCU sleep impossible.
  if (bsp_battery_sleep() != ESP_OK)
    ESP_LOGW(TAG, "Battery gauge sleep unavailable");
  ESP_LOGI(TAG, "sleep step: hold display pins");
  bsp_display_sleep_hold();
  ESP_LOGI(TAG, "Idle timeout: deep sleep, GPIO0 low wake");
#ifdef ESP_PLATFORM
  hr_power_record_sleep();
#endif
  // Unlike unconditional start, a newly pressed key can reject sleep safely.
  if (esp_deep_sleep_try_to_start() != ESP_OK)
    ESP_LOGW(TAG, "Deep sleep entry rejected");
recover:
  bsp_display_sleep_unhold();
  if (panel_attempted) {
    (void)bsp_battery_wake();
    if (bsp_display_wake() != ESP_OK)
      ESP_LOGW(TAG, "Panel wake failed; display recovery needs retry");
  }
  if (platform_stopped)
    power.resume(power.context);
  if (display_locked)
    bsp_lvgl_resume();
  bsp_button_cancel_sleep();
  (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  ESP_LOGW(TAG, "Sleep cancelled; application resumed");
}
void hr_power_tick(uint32_t now, bool activity, bool busy) {
  power.now = now;
  // Includes intro/game/phone editing: these must not stall gauge recovery.
  // The driver advances at most one 5ms transaction per step, with no delays.
  if (power.phase != GOODBYE && power.phase != FALLING &&
      bsp_battery_service(now) == ESP_OK &&
      (!power.battery_sampled || (uint32_t)(now - power.battery_at) >=
          (power.battery_errors || !power.battery_valid ? 1000u : 5000u)))
    sample_battery(now);
  if (activity || busy) {
    power.idle_at = now;
    if (power.phase == FALLING)
      rise(now);
    else if (power.phase == GOODBYE)
      power.phase = ON;
  }
  if (power.phase == WAIT_FRAME && bsp_lvgl_first_frame_ready())
    rise(now);
  uint32_t elapsed = now - power.fade_at;
  if (power.phase == RISING) {
    if (elapsed >= 300) {
      light(80);
      power.phase = ON;
    } else
      light(power.fade_from + (80 - power.fade_from) * elapsed / 300);
  } else if (power.phase == ON && !busy &&
             (uint32_t)(now - power.idle_at) >= HR_POWER_IDLE_MS) {
    ESP_LOGI(TAG, "Idle timeout: start goodbye animation");
    power.phase = GOODBYE;
    power.fade_at = now;
  } else if (power.phase == GOODBYE && elapsed >= HR_POWER_GOODBYE_MS) {
    power.phase = FALLING;
    power.fade_at = now;
    power.fade_from = power.light;
  } else if (power.phase == FALLING) {
    if (elapsed < 300)
      light(power.fade_from * (300 - elapsed) / 300);
    else {
      light(0);
      sleep_attempt();
      // A failure gets a fresh idle interval, rather than retrying every 5ms.
      power.idle_at = now;
      rise(now);
    }
  }
}

bool hr_power_goodbye(uint32_t now, uint32_t *elapsed) {
  if (power.phase != GOODBYE && power.phase != FALLING) return false;
  *elapsed = power.phase == GOODBYE ? now-power.fade_at : HR_POWER_GOODBYE_MS;
  return true;
}
