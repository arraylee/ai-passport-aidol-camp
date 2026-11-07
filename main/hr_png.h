#pragma once
#include <stdbool.h>
#include <stddef.h>

#include <stdint.h>
typedef int (*hr_png_row_fn)(void *, unsigned, const uint16_t[96]);
int hr_png_decode(const uint8_t *png, size_t size, hr_png_row_fn row,
                  void *user);
