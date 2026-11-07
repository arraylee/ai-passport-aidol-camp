#pragma once
#include "hr_core.h"
void hr_social_enter(hr_core_t *c);
void hr_social_leave(hr_core_t *c);
bool hr_social_peer_eligible(const hr_core_t *c, unsigned id, uint32_t now);
void hr_social_tick(hr_core_t *c, uint32_t now);
void hr_social_confirm(hr_core_t *c, uint32_t now);
void hr_social_saved(hr_core_t *c, bool success);
void hr_social_select(hr_core_t *c, int direction, uint32_t now);
void hr_friends_encode(uint8_t id, uint8_t mask, uint8_t out[12]);
bool hr_friends_decode(uint8_t id, const uint8_t in[12], uint8_t *mask);
uint8_t hr_nearest(const hr_core_t *c, uint32_t now, uint8_t previous);
