#pragma once
#include "lvgl.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Main-task owned. Abort on BLE disconnect/generation changes or image-mode
 * changes. Call while the queued BLE job's session is current. */
bool hr_gallery_init(void);
unsigned hr_gallery_image(const uint8_t *data, size_t length, uint32_t now);
void hr_gallery_abort(void);
void hr_gallery_expire(uint32_t now);
bool hr_gallery_busy(void);
/* Flash-backed, native-size RGB565. NULL until a complete photo is committed.
 */
const lv_image_dsc_t *hr_gallery_image_descriptor(void);

void hr_gallery_detach(void);
