#include "hr_welcome.h"
#include <string.h>
bool hr_welcome_begin(hr_welcome_t *s, const uint8_t *data, size_t bytes) {
  memset(s, 0, sizeof *s);
  if (!data || bytes < 12 || memcmp(data, "ADP1", 4) ||
      data[4] != 0x80 || data[5] != 0x3e || data[6] != 1 || data[7])
    return false;
  uint32_t samples = (uint32_t)data[8] | (uint32_t)data[9]<<8 |
                     (uint32_t)data[10]<<16 | (uint32_t)data[11]<<24;
  if (!samples || samples > 16000*10 || (samples+1)/2 != bytes-12)
    return false;
  s->data = data+12;
  s->samples = samples;
  return true;
}
size_t hr_welcome_next(hr_welcome_t *s, int16_t pcm[HR_SAMPLES]) {
  memset(pcm, 0, HR_SAMPLES*sizeof *pcm);
  size_t count = s->samples-s->position;
  if (count > HR_SAMPLES) count = HR_SAMPLES;
  for (size_t n=0; n<count; n++,s->position++) {
    unsigned code = (s->data[s->position/2] >> (4*(s->position%2))) & 15;
    pcm[n] = hr_ima_sample(code, &s->predictor, &s->index);
  }
  return count;
}
