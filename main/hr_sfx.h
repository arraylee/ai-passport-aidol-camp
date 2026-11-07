#pragma once
#include "hr_game.h"
#include <stdatomic.h>
#include <stddef.h>
enum { HR_SFX_NONE, HR_SFX_THREE, HR_SFX_TWO, HR_SFX_ONE, HR_SFX_DEATH, HR_SFX_WIN };
typedef struct { uint16_t phase, position; uint8_t kind; } hr_sfx_player_t;
typedef struct { uint32_t start, collision; uint8_t count, rescues; bool active, won; } hr_sfx_watch_t;
typedef struct { atomic_uint pending, guard_at, guard_ms; atomic_bool enabled, playing; } hr_sfx_bus_t;
unsigned hr_sfx_event(hr_sfx_watch_t *w,const hr_game_t *g,bool enabled,uint32_t now);
unsigned hr_sfx_samples(unsigned kind);
void hr_sfx_begin(hr_sfx_player_t *p,unsigned kind);
bool hr_sfx_render(hr_sfx_player_t *p,int16_t *pcm,size_t samples);
void hr_sfx_enable(hr_sfx_bus_t *b,bool enabled,uint32_t now);
void hr_sfx_request(hr_sfx_bus_t *b,unsigned kind,uint32_t now);
bool hr_sfx_guarded(const hr_sfx_bus_t *b,uint32_t now);
bool hr_sfx_frame(hr_sfx_bus_t *b,hr_sfx_player_t *p,int16_t *pcm,size_t n,uint32_t now);
