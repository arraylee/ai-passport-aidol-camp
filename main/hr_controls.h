#pragma once
#include "hr_input.h"
#include "hr_ui.h"
enum { HR_EFFECT_NONE, HR_EFFECT_SAVE_ID, HR_EFFECT_VOLUME_CHANGED, HR_EFFECT_ACTIVATE };
void hr_view_home(hr_view_t *v, uint32_t now);
int hr_handle_key(hr_view_t *v, int event, uint32_t now);

/* Same ordering for rendering and navigation: all/game/back/online peers. */
unsigned hr_room_choices(const hr_view_t *v, uint32_t now, uint8_t items[9]);
