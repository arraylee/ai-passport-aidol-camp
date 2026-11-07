#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { HR_INTRO_NONE, HR_INTRO_BOOT, HR_INTRO_WELCOME, HR_INTRO_SLEEP };
enum { HR_INTRO_START_AUDIO = 1, HR_INTRO_SAVE_WELCOME = 2 };
enum { HR_BOOT_MS = 1400, HR_WELCOME_MIN_MS = 5200, HR_WELCOME_MAX_MS = 7500 };
typedef struct { unsigned kind; bool welcomed; uint32_t at; } hr_intro_t;
void hr_intro_init(hr_intro_t *s, bool welcomed, uint32_t now);
unsigned hr_intro_tick(hr_intro_t *s, uint32_t now, bool audio_done);
