#include "hr_input.h"
#include <string.h>
void hr_input_init(hr_input_t *i, uint32_t now) {
  memset(i, 0, sizeof *i);
  i->stable = i->candidate = -2;
  i->changed = now;
}
int hr_input_tick(hr_input_t *i, int mv, uint32_t now) {
  int key = mv < 0 ? -2 : mv < 150 ? 0 : mv < 447 ? 2 : mv < 1900 ? 1 : -1;
  if (key != i->candidate) {
    i->candidate = key;
    i->changed = now;
  }
  if (key != i->stable && (uint32_t)(now - i->changed) >= 30) {
    int old = i->stable;
    bool end = old == 0 && i->long_sent;
    bool tap = old >= 0 && i->armed && !i->long_sent && key == -1;
    i->stable = key;
    if (key == -1)
      i->armed = true;
    else if (key == -2 || old >= 0)
      i->armed = false;
    i->pressed = now;
    i->long_sent = false;
    if (end)
      return HR_KEY_END;
    if (tap)
      return old == 0 ? HR_KEY_UP : old == 1 ? HR_KEY_DOWN : HR_KEY_CLICK;
  }
  if (!i->armed || i->stable != key || key < 0 || i->long_sent)
    return HR_KEY_NONE;
  unsigned threshold = key == 2 ? 800 : 600;
  if ((uint32_t)(now - i->pressed) >= threshold) {
    i->long_sent = true;
    return key == 0   ? HR_KEY_UP_HOLD
           : key == 1 ? HR_KEY_DOWN_HOLD
                      : HR_KEY_CENTER_HOLD;
  }
  return HR_KEY_NONE;
}
