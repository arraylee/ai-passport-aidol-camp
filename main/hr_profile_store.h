#pragma once
#include "hr_edit.h"
#include "hr_profiles.h"
bool hr_profile_store_init(unsigned id);
void hr_profile_store_select(unsigned id);
const hr_edit_t *hr_profile_current(void);
unsigned hr_profile_config(const uint8_t *p, size_t n);
unsigned hr_profile_image(const uint8_t *p, size_t n, uint32_t now);
void hr_profile_abort(void);

void hr_profile_expire(uint32_t now);

bool hr_profile_upload_busy(void);

void hr_profile_store_detach(void);
