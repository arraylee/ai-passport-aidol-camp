#pragma once
#include <stdbool.h>
#include <stdint.h>
enum {
  HR_KEY_NONE,
  HR_KEY_UP,
  HR_KEY_DOWN,
  HR_KEY_CLICK,
  HR_KEY_UP_HOLD,
  HR_KEY_DOWN_HOLD,
  HR_KEY_CENTER_HOLD,
  HR_KEY_END
};
typedef struct {
  int stable, candidate;
  uint32_t changed, pressed;
  bool armed, long_sent;
} hr_input_t;
void hr_input_init(hr_input_t *i, uint32_t now);
int hr_input_tick(hr_input_t *i, int mv, uint32_t now);
