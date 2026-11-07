#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "hr_core.h"
typedef struct {
  const uint8_t *data;
  size_t position, samples;
  int predictor, index;
} hr_welcome_t;
extern const uint8_t hr_welcome_data[];
extern const size_t hr_welcome_size;
bool hr_welcome_begin(hr_welcome_t *s, const uint8_t *data, size_t bytes);
/* Returns 0 at EOF; writes at most HR_SAMPLES samples and zero pads the tail. */
size_t hr_welcome_next(hr_welcome_t *s, int16_t pcm[HR_SAMPLES]);
