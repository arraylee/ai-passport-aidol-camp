#pragma once
#include <stdbool.h>
#include <stdint.h>
enum { HR_GAME_PREP_MS=3000, HR_GAME_CLEAR_MS=1800,
       HR_SCORE_MAX=99999, HR_GAME_COURSE=1000, HR_GAME_PLATFORMS=6,
       HR_GAME_COINS=12, HR_GAME_VERSION=6, HR_GAME_DEATH_PENALTY=50, HR_GAME_MAX_HEIGHT=124 };
enum { HR_GAME_READY, HR_GAME_RUNNING, HR_GAME_CLEAR, HR_GAME_DONE };
typedef struct { int16_t x, width; uint8_t height; } hr_game_platform_t;
typedef struct { int16_t x; uint8_t height; bool hit; } hr_obstacle_t;
typedef struct { int16_t x; uint8_t height; bool taken, missed, high; } hr_coin_t;
typedef struct {
 uint8_t phase, combo, coins_taken, rescues, rescue_kind;
 int16_t height, velocity_q8;
 int32_t height_q8;
 uint16_t base_hz, pitch_hz, progress, landings, voice_hz, reward_points, checkpoint;
 uint8_t checkpoint_height;
 uint8_t platform_mask;
 uint32_t start, step_at, score, collision_at, ground_at, rescue_until, voice_at, reward_at, clear_at;
 bool hit_flash, grounded;
 hr_obstacle_t obstacles[2]; hr_coin_t coins[HR_GAME_COINS];
} hr_game_t;
extern const hr_game_platform_t hr_game_platforms[HR_GAME_PLATFORMS];
void hr_game_start(hr_game_t *g, uint32_t now);
void hr_game_tick(hr_game_t *g, uint32_t now, unsigned hz);
unsigned hr_game_seconds(const hr_game_t *g, uint32_t now);
unsigned hr_game_player_x(const hr_game_t *g);
unsigned hr_pitch_hz(const int16_t pcm[640]);
typedef struct { uint16_t level,candidate_hz; uint8_t strong; bool open; } hr_voice_gate_t;
/* Game-only voice gate: two voiced 40ms windows, RMS 300 on / 180 off. */
unsigned hr_voice_hz(hr_voice_gate_t *gate,const int16_t pcm[640]);
void hr_best_encode(unsigned id,unsigned score,uint8_t out[16]);
bool hr_best_decode(unsigned id,const uint8_t in[16],unsigned *score);
