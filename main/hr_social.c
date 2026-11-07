#include "hr_social.h"
#include <string.h>
bool hr_social_peer_eligible(const hr_core_t *c, unsigned id, uint32_t now) {
  return id && id <= HR_PLAYERS && id != c->id && hr_online(c, id, now) &&
         !hr_conflict(c, id, now) && !hr_conflict(c, c->id, now) &&
         (c->social.manual ?
            (c->peers[id].social_flags & HR_SOCIAL_MANUAL_FLAG) &&
             c->peers[id].signal_q8 >= HR_SOCIAL_MANUAL_RSSI*256 :
            !(c->peers[id].social_flags & HR_SOCIAL_MANUAL_FLAG) &&
             c->peers[id].zone == 2) &&
         (uint32_t)(now - c->peers[id].hello_at) < 800 &&
         (c->peers[id].social_flags & 2) && c->peers[id].social_session;
}
uint8_t hr_nearest(const hr_core_t *c, uint32_t now, uint8_t previous) {
  unsigned best = 0;
  for (unsigned i = 1; i <= HR_PLAYERS; i++)
    if (i != c->id && hr_online(c, i, now) && !hr_conflict(c, i, now) &&
        c->peers[i].zone == 2 &&
        (uint32_t)(now - c->peers[i].hello_at) < 1500 &&
        (!best || c->peers[i].signal_q8 > c->peers[best].signal_q8))
      best = i;
  // Keep an existing nearest until another near peer is at least 4 dB stronger.
  if (best && previous && previous <= HR_PLAYERS && previous != c->id &&
      hr_online(c, previous, now) && !hr_conflict(c, previous, now) &&
      c->peers[previous].zone == 2 &&
      (uint32_t)(now - c->peers[previous].hello_at) < 1500 &&
      c->peers[best].signal_q8 < c->peers[previous].signal_q8 + 4 * 256)
    return previous;
  return best;
}
void hr_social_enter(hr_core_t *c) {
  uint32_t session = c->social.session + 1;
  uint8_t mask = c->social.friends;
  memset(&c->social, 0, sizeof c->social);
  c->social.session = session ? session : 1;
  c->social.friends = mask;
  c->social.active = true;
  c->next_hello = 0;
}
void hr_social_leave(hr_core_t *c) {
  c->social.active = false;
  c->social.manual = false;
  c->touch_target = 0;
  c->social.locked_candidate = 0;
  c->social.accepted = false;
  c->social.pending = false;
  c->social.complete = false;
  c->next_hello = 0;
}
void hr_social_select(hr_core_t *c, int direction, uint32_t now) {
  if (!c->social.active || c->social.locked_candidate ||
      (c->social.accepted && !c->social.complete))
    return;
  unsigned id = c->social.candidate;
  for (unsigned n = 0; n < HR_PLAYERS; n++) {
    id = direction > 0 ? id % HR_PLAYERS + 1 : (id <= 1 ? HR_PLAYERS : id - 1);
    if (hr_social_peer_eligible(c, id, now)) {
      if (c->social.complete && id != c->social.candidate)
        hr_social_enter(c); // New handshake; retain already saved friends.
      c->social.candidate = id;
      return;
    }
  }
}
void hr_social_tick(hr_core_t *c, uint32_t now) {
  hr_social_t *s = &c->social;
  if (!s->active)
    return;
  if (s->locked_candidate)
    s->candidate = s->locked_candidate;
  if (!hr_social_peer_eligible(c, s->candidate, now)) {
    s->candidate = 0;
    s->accepted = s->pending = s->saved = s->complete = s->failed = false;
    s->peer_boot = s->peer_session = 0;
    hr_social_select(c, 1, now);
    return;
  }
  hr_peer_t *p = &c->peers[s->candidate];
  if (s->accepted &&
      (s->peer_boot != p->boot || s->peer_session != p->social_session)) {
    s->accepted = s->pending = s->saved = s->complete = s->failed = false;
    s->peer_boot = s->peer_session = 0;
  }
  bool mutual = s->accepted && (p->social_flags & 4) &&
                p->social_target == c->id && p->social_boot == c->boot &&
                p->social_token == s->session;
  s->pending = mutual && !s->saved && !s->failed;
  s->complete = mutual && s->saved && (p->social_flags & 8);
}
void hr_social_confirm(hr_core_t *c, uint32_t now) {
  hr_social_tick(c, now);
  hr_social_t *s = &c->social;
  if (!s->active || !hr_social_peer_eligible(c, s->candidate, now))
    return;
  hr_peer_t *p = &c->peers[s->candidate];
  s->accepted = true;
  s->peer_boot = p->boot;
  s->peer_session = p->social_session;
  s->failed = false;
  c->next_hello = now;
}
void hr_social_saved(hr_core_t *c, bool success) {
  hr_social_t *s = &c->social;
  if (!s->pending)
    return;
  s->failed = !success;
  s->saved = success;
  if (success) {
    s->friends |= 1u << s->candidate;
    s->pending = false;
  }
}
void hr_friends_encode(uint8_t id, uint8_t mask, uint8_t out[12]) {
  memset(out, 0, 12);
  memcpy(out, "HRF1", 4);
  out[4] = id;
  out[5] = mask & 0x7e & ~(1u << id);
  uint32_t crc = hr_crc(out, 8);
  for (int i = 0; i < 4; i++)
    out[8 + i] = crc >> (8 * i);
}
bool hr_friends_decode(uint8_t id, const uint8_t in[12], uint8_t *mask) {
  uint32_t crc = 0;
  for (int i = 0; i < 4; i++)
    crc |= (uint32_t)in[8 + i] << (8 * i);
  if (id < 1 || id > HR_PLAYERS || memcmp(in, "HRF1", 4) || in[4] != id ||
      hr_crc(in, 8) != crc || (in[5] & ~0x7e) || (in[5] & (1u << id)))
    return false;
  *mask = in[5];
  return true;
}
