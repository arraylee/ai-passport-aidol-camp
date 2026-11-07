#pragma once
#include "lvgl.h"
typedef struct {
  const char *name, *signature, *url;
  const lv_image_dsc_t *card, *gallery, *small, *portrait;
} hr_profile_t;
const hr_profile_t *hr_profile(unsigned id);
void hr_profile_override(unsigned id, const char *name, const char *intro,
                         const lv_image_dsc_t *card,
                         const lv_image_dsc_t *small);
void hr_profile_clear(unsigned id);
const char *hr_display_name(unsigned id);

void hr_profile_set_title(unsigned id, const char *title);
const char *hr_display_title(unsigned id);
bool hr_profile_remote_avatar(unsigned id);
void hr_profile_remote(unsigned id, const char *name, const char *intro,
                       bool avatar);

const hr_profile_t *hr_profile_base(unsigned id);

void hr_profile_set_portrait(unsigned id, const lv_image_dsc_t *portrait);

/* Receiver-local slots never inherit a chosen persona before PROFILE arrives. */
void hr_profile_remote_reset(unsigned self);
