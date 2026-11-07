#include "hr_input.h"
#include <assert.h>
#include <stdio.h>
static hr_input_t input;
static unsigned now;
static int sample(int mv, unsigned duration) {
  int event = 0;
  for (unsigned n = 0; n < duration; n += 10) {
    int e = hr_input_tick(&input, mv, now);
    if (e)
      event = e;
    now += 10;
  }
  return event;
}
int main(void) {
  hr_input_init(&input, 0);
  assert(sample(0, 1000) == HR_KEY_NONE);
  sample(3000, 60);
  assert(sample(0, 100) ==
         HR_KEY_NONE); // Holding UP must not navigate on initial press.
  assert(sample(3000, 60) == HR_KEY_UP);
  assert(sample(0, 100) == HR_KEY_NONE);
  assert(sample(3000, 60) == HR_KEY_UP);
  assert(sample(0, 500) == HR_KEY_NONE);
  assert(sample(3000, 60) == HR_KEY_UP);
  assert(sample(0, 700) == HR_KEY_UP_HOLD);
  assert(sample(3000, 60) == HR_KEY_END);
  assert(sample(600, 100) == HR_KEY_NONE);
  assert(sample(3000, 60) == HR_KEY_DOWN);
  assert(sample(600, 700) == HR_KEY_DOWN_HOLD);
  assert(sample(3000, 60) == HR_KEY_NONE);
  assert(sample(300, 100) == HR_KEY_NONE);
  assert(sample(3000, 60) == HR_KEY_CLICK);
  assert(sample(300, 900) == HR_KEY_CENTER_HOLD);
  assert(sample(3000, 60) == HR_KEY_NONE);
  assert(sample(0, 700) == HR_KEY_UP_HOLD);
  assert(sample(-1, 60) == HR_KEY_END);
  sample(3000, 60);
  assert(sample(0, 20) == HR_KEY_NONE);
  assert(sample(3000, 60) == HR_KEY_NONE);
  // ADC transitions between keys consume the entire chord until neutral.
  sample(600, 70);
  assert(sample(0, 500) == HR_KEY_NONE);
  assert(sample(3000, 60) == HR_KEY_NONE);
  puts("PASS release-based navigation, no switch before PTT, independent "
       "holds, bounce, ADC failure, chord suppression");
}
