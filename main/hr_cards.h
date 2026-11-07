#pragma once
#include "hr_core.h"
#include "hr_friend_store.h"
/* Kind values reserved by hr_core: META=7, REQUEST=8, DATA=9.
 * request/epoch carry sender/recipient social sessions; token is snapshot CRC.
 * META flags is image length, REQUEST/DATA flags is byte offset.
 * REQUEST offset==length acknowledges a verified Flash commit.
 * Core owns MAC, boot and seq validation. All calls are on the main owner task.
 * Reset at phone upload BEGIN and suspend all calls until upload ends.
 */
bool hr_cards_init(unsigned owner);
void hr_cards_reset(void);
void hr_cards_tick(hr_core_t *c, uint32_t now);
bool hr_cards_send(hr_core_t *c, hr_packet_t *p, uint32_t now);
void hr_cards_receive(hr_core_t *c, const hr_packet_t *p, uint32_t now);
bool hr_cards_ready(const hr_core_t *c, unsigned candidate);
unsigned hr_cards_progress(const hr_core_t *c);
/* A verified received snapshot may be committed before the reciprocal ACK;
 * social.complete still requires BOTH durable roster saves in this session. */
typedef bool (*hr_cards_save_fn)(const hr_core_t *c, uint8_t friends);
void hr_cards_commit(hr_core_t *c, uint32_t now, hr_cards_save_fn save);
enum { HR_CARDS_OK, HR_CARDS_TIMEOUT, HR_CARDS_BEGIN, HR_CARDS_WRITE,
       HR_CARDS_VERIFY, HR_CARDS_ROSTER };
typedef struct {
  unsigned reason, rx_bytes, rx_total, tx_confirmed, tx_total, commit_attempts;
  uint32_t elapsed;
  bool received, acknowledged;
} hr_cards_stats_t;
void hr_cards_stats(hr_cards_stats_t *out, uint32_t now);
