#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define HR_PLAYERS 6
#define HR_AUDIO 164
#define HR_SAMPLES 320
#define HR_WIRE_MAX 200
#define HR_OFFLINE 3000u
#define HR_LEASE 450u
#define HR_HOST_LEASE 700u
#define HR_MAX_TALK 30000u
enum { HR_SOCIAL_MANUAL_FLAG=128, HR_SOCIAL_MANUAL_RSSI=-65 };
enum {
  HR_HELLO = 1,
  HR_REQUEST,
  HR_GRANT,
  HR_RELEASE,
  HR_VOICE,
  HR_PROFILE,
  HR_CARD_META,
  HR_CARD_REQUEST,
  HR_CARD_DATA,
  HR_PREVIEW_REQUEST,
  HR_PREVIEW_DATA,
  HR_SCORE,
  HR_KIND_MAX = HR_SCORE
};
typedef struct {
  uint8_t kind, id, target, speaker;
  uint32_t boot, seq, request, epoch, token, flags;
  uint8_t audio[HR_AUDIO];
} hr_packet_t;
typedef struct {
  bool seen;
  uint8_t mac[6];
  char sn[25];
  uint32_t boot, retired_boot, seq, last, conflict_until;
  int signal_q8, zone, trend, anchor;
  int8_t signal_window[3];
  uint32_t trend_at, samples, hello_at;
  uint32_t social_session, social_boot, social_token;
  uint32_t room_session, best_score;
  bool score_valid;
  uint8_t social_target, social_flags, touch_target;
} hr_peer_t;
typedef struct {
  bool active, accepted, pending, saved, complete, failed, manual;
  uint8_t candidate, friends, locked_candidate;
  uint32_t session, peer_boot, peer_session;
} hr_social_t;
typedef struct {
  hr_social_t social;
  uint8_t id, target, speaker, floor_target, touch_target, host;
  uint32_t boot, seq, request;
  uint32_t epoch, token, floor_request, floor_at, floor_started, conflict_until;
  uint32_t floor_client_at, host_ready_at;
  uint32_t press_at, next_hello, next_control, release_request;
  unsigned releases;
  bool held, blocked, hardware_ok, room_enabled, room_joined, game_busy, best_valid;
  bool identity_mode, roster_dirty;
  uint32_t room_session, best_score;
  uint8_t mac[6];
  char sn[25];
  hr_peer_t peers[HR_PLAYERS + 1];
  uint32_t voice_seq;
  bool voice_seen;
} hr_core_t;
uint32_t hr_crc(const void *data, size_t len);
size_t hr_pack(const hr_packet_t *p, uint8_t *out);
bool hr_unpack(hr_packet_t *p, const uint8_t *data, size_t n);
void hr_init(hr_core_t *c, uint8_t id, const uint8_t mac[6], uint32_t boot,
             uint32_t now);
bool hr_online(const hr_core_t *c, uint8_t id, uint32_t now);
bool hr_conflict(const hr_core_t *c, uint8_t id, uint32_t now);
void hr_press(hr_core_t *c, uint8_t target, uint32_t now);
void hr_release(hr_core_t *c, uint32_t now);
void hr_tick(hr_core_t *c, uint32_t now);
bool hr_receive(hr_core_t *c, const hr_packet_t *p, const uint8_t mac[6],
                int rssi, uint32_t now);
bool hr_control(hr_core_t *c, hr_packet_t *p, uint32_t now);
bool hr_can_talk(const hr_core_t *c, uint32_t now);
bool hr_voice(hr_core_t *c, hr_packet_t *p, const uint8_t audio[HR_AUDIO],
              uint32_t now);
void hr_adpcm_encode(const int16_t pcm[HR_SAMPLES], uint8_t out[HR_AUDIO]);
bool hr_adpcm_decode(const uint8_t in[HR_AUDIO], int16_t pcm[HR_SAMPLES]);
void hr_config_encode(uint8_t id, uint8_t volume, uint8_t out[12]);
bool hr_config_decode(const uint8_t in[12], uint8_t *id, uint8_t *volume);

void hr_room_enable(hr_core_t *c, uint32_t now);
void hr_room_join(hr_core_t *c, bool joined, uint32_t now);
bool hr_room_member(const hr_core_t *c, uint8_t id, uint32_t now);

/* IMA primitive shared with the original continuous Aidol welcome stream. */
int hr_ima_sample(unsigned code, int *predictor, int *index);

bool hr_game_peer_busy(const hr_core_t *c, uint8_t id);
bool hr_score_packet(hr_core_t *c, hr_packet_t *p, uint32_t now);
unsigned hr_ranked(const hr_core_t *c, uint32_t now, uint8_t ids[HR_PLAYERS]);

void hr_coordinator_set(hr_core_t *c,uint8_t host,uint32_t now);
