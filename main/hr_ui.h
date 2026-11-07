#pragma once
#include "hr_core.h"
#include "hr_social.h"
#include "hr_intro.h"
#include "hr_game.h"
enum {
  HR_PAGE_HOME,
  HR_PAGE_RADIO,
  HR_PAGE_MENU,
  HR_PAGE_PAIR,
  HR_PAGE_FRIENDS,
  HR_PAGE_GALLERY,
  HR_PAGE_CARD,
  HR_PAGE_QR,
  HR_PAGE_LOBBY,
  HR_PAGE_GAME_MENU, HR_PAGE_GAME_PLAY, HR_PAGE_GAME_RESULT, HR_PAGE_RANK
};
enum { HR_ROOM_JOIN_MS = 1800, HR_ROOM_GAME=7, HR_ROOM_BACK=8 };
typedef struct hr_view {
  hr_core_t core;
  uint8_t selection, volume, choice, page, menu, gallery, friend_selection,
      nearest, card_id, home_selection;
  bool up_pressed, phone_connected, activation_required;
  bool setup, volume_mode, radio_ok, audio_ok, buttons_ok, save_failed,
      receiving;
  uint8_t game_selection, result_selection;
  bool gallery_return, best_dirty, new_best, game_scored;
  hr_game_t game;
  unsigned card_progress;
  const void *pair_preview;
  bool room_joining, room_empty, card_from_pair;
  unsigned intro_kind;
  int battery_percent; /* CW2017 SOC; -1 means unknown, 0 is valid empty. */
  uint32_t intro_elapsed;
  uint32_t now, room_join_started;
} hr_view_t;
void hr_ui_render(const hr_view_t *v);
