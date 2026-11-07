#pragma once
#include <stdbool.h>
#include <stdint.h>
struct hr_view;
enum { HR_TOUCH_RSSI=-30, HR_TOUCH_MARGIN=6, HR_TOUCH_MS=1800, HR_TOUCH_SAMPLES=10, HR_TOUCH_REARM_MS=600, HR_TOUCH_MUTUAL_MS=300, HR_TOUCH_MUTUAL_SAMPLES=2 };
enum { HR_MANUAL_MARGIN=3, HR_MANUAL_MS=600, HR_MANUAL_SAMPLES=3 };
enum { HR_TOUCH_DISABLED,HR_TOUCH_WEAK,HR_TOUCH_AMBIGUOUS,HR_TOUCH_DWELL,HR_TOUCH_MUTUAL,HR_TOUCH_BLOCKED,HR_TOUCH_OPENED };
typedef struct {
  bool background, mutual_tracking;
  uint8_t candidate, blocked, away_tracking, nearest, runner, reason;
  uint32_t since, samples, peer_boot, away_since[7];
  uint32_t mutual_since, mutual_samples, mutual_session;
} hr_touch_t;
/* Returns true on HOME/explicit FRIENDS -> PAIR, never confirms friendship. */
bool hr_touch_tick(hr_touch_t *s, struct hr_view *v, uint32_t now,
                   bool keys_neutral);
