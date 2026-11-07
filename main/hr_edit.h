#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define HR_EDIT_NAME 49
#define HR_EDIT_INTRO 85
#define HR_EDIT_HEADER 224
#define HR_EDIT_BANK 65536u
#define HR_EDIT_FULL_OFFSET 4096u
#define HR_EDIT_CARD_OFFSET 34048u
#define HR_EDIT_SMALL_OFFSET 59392u
#define HR_EDIT_IMAGE_END 61440u
typedef struct {
  uint32_t revision, image_crc;
  uint8_t id;
  bool avatar;
  char name[49], intro[85], title[49];
} hr_edit_t;
typedef struct {
  uint8_t bytes[260];
  size_t used, wanted;
  uint32_t at;
} hr_frame_t;
void hr_edit_encode(const hr_edit_t *p, uint8_t out[HR_EDIT_HEADER]);
bool hr_edit_decode(hr_edit_t *p, const uint8_t in[HR_EDIT_HEADER]);
// Positive status DONE=1. Output unchanged on invalid input.
unsigned hr_edit_config(hr_edit_t *p, const uint8_t *json, size_t n);
// 0 pending, 1 complete frame, 0x10 invalid, 0x11 expired, 0x15 oversized.
unsigned hr_frame_push(hr_frame_t *f, const uint8_t *data, size_t n,
                       uint32_t now);
bool hr_utf8_valid(const char *s, size_t length);
void hr_edit_wire_encode(const hr_edit_t *p, uint8_t out[164]);
bool hr_edit_wire_decode(hr_edit_t *p, const uint8_t in[164]);
