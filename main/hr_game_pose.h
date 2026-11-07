#pragma once
#include "hr_game.h"
/* Presentation only: never writes game physics or clocks. */
enum { HR_JOY_IDLE, HR_JOY_PUSH, HR_JOY_TUCK, HR_JOY_LAND };
typedef struct {
  uint32_t start, jump_at;
  int16_t height;
  bool ready, grounded, descending;
} hr_jump_animation_t;
static inline unsigned hr_jump_pose(hr_jump_animation_t *a,
                                    const hr_game_t *g, uint32_t now) {
  bool fresh = !a->ready || a->start != g->start || g->phase == HR_GAME_READY;
  unsigned pose = HR_JOY_IDLE;
  if (g->phase == HR_GAME_RUNNING) {
    if (g->grounded) {
      if (g->landings && (uint32_t)(now - g->ground_at) < 140)
        pose = HR_JOY_LAND;
    } else {
      if (fresh || a->grounded) {
        a->jump_at = now;
        a->descending = false;
      }
      if (!fresh && g->height < a->height)
        a->descending = true;
      else if (g->height > a->height)
        a->descending = false;
      if (g->velocity_q8 < 0)
        a->descending = true;
      pose = a->descending ? HR_JOY_LAND :
             (uint32_t)(now - a->jump_at) < 100 ? HR_JOY_PUSH : HR_JOY_TUCK;
    }
  }
  a->ready = true;
  a->start = g->start;
  a->height = g->height;
  a->grounded = g->grounded;
  return pose;
}
