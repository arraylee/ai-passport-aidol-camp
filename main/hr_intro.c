#include "hr_intro.h"
void hr_intro_init(hr_intro_t *s, bool welcomed, uint32_t now) {
  *s = (hr_intro_t){.kind=HR_INTRO_BOOT,.welcomed=welcomed,.at=now};
}
unsigned hr_intro_tick(hr_intro_t *s, uint32_t now, bool audio_done) {
  uint32_t elapsed = now - s->at;
  if (s->kind == HR_INTRO_BOOT && elapsed >= HR_BOOT_MS) {
    s->at = now;
    s->kind = s->welcomed ? HR_INTRO_NONE : HR_INTRO_WELCOME;
    return s->welcomed ? 0 : HR_INTRO_START_AUDIO;
  }
  if (s->kind == HR_INTRO_WELCOME &&
      ((elapsed >= HR_WELCOME_MIN_MS && audio_done) || elapsed >= HR_WELCOME_MAX_MS)) {
    s->kind = HR_INTRO_NONE;
    s->welcomed = audio_done;
    return audio_done ? HR_INTRO_SAVE_WELCOME : 0;
  }
  return 0;
}
