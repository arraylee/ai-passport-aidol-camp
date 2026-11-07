#pragma once
#include <stdint.h>
#define LV_IMAGE_HEADER_MAGIC 0x19
#define LV_COLOR_FORMAT_RGB565 0x12
typedef struct {
  struct { uint32_t magic, cf, w, h, stride; } header;
  uint32_t data_size;
  const uint8_t *data;
} lv_image_dsc_t;
