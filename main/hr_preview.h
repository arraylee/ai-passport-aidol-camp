#pragma once
#include "hr_core.h"
#include "hr_profiles.h"
/* Volatile 32x52 portrait preview. Never creates a friend or writes Flash.
 * Only an activated popup's locked peer can request/respond. */
void hr_preview_tick(const hr_core_t *c, bool enabled, uint32_t now);
void hr_preview_reset(void);
bool hr_preview_send(hr_core_t *c, hr_packet_t *p, uint32_t now);
void hr_preview_receive(const hr_core_t *c, const hr_packet_t *p, uint32_t now);
const lv_image_dsc_t *hr_preview_image(void);
