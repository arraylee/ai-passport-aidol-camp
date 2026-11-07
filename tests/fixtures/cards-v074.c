/* Frozen v0.7.4 production protocol. Original SHA256 454ab71a6173490f3024795b8fa6d39a4be46d4b1cd623acbc03fb7eac1b0c34; only progress signature and commit caller bridge adapted for host linking. */
#include "hr_cards.h"
#include "hr_profile_store.h"
#include <stdio.h>
#include <string.h>

enum { CARD_META = 7, CARD_REQUEST = 8, CARD_DATA = 9 };
static unsigned owner_id;
static bool available;
static struct {
  bool active, have_meta, received, acknowledged, response;
  unsigned peer, used, remote_bytes, local_bytes, response_offset, peer_offset;
  uint32_t boot, peer_boot, session, peer_session, revision, local_crc,
      remote_crc;
  uint32_t next_meta, next_request, next_send, started, progressed;
  const lv_image_dsc_t *portrait;
  const uint8_t *pixels;
  uint8_t peer_mac[6];
  uint8_t wire[164], remote_wire[164];
} sync_card;
static bool due(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }
void hr_cards_reset(void) {
  memset(&sync_card, 0, sizeof sync_card);
  hr_friend_store_abort();
}
bool hr_cards_init(unsigned owner) {
  hr_cards_reset();
  owner_id = owner;
  available = hr_friend_store_init(owner);
  return available;
}
static bool mutual(const hr_core_t *c, uint32_t now) {
  const hr_social_t *s = &c->social;
  if (!available || c->roster_dirty || c->id != owner_id || !s->active || !s->accepted || s->failed ||
      !s->candidate || s->candidate > HR_PLAYERS || s->candidate == c->id ||
      !hr_online(c, s->candidate, now) || hr_conflict(c, s->candidate, now) ||
      hr_conflict(c, c->id, now))
    return false;
  const hr_peer_t *p = &c->peers[s->candidate];
  return p->zone == 2 && (uint32_t)(now - p->hello_at) < 800 &&
         (p->social_flags & 6) == 6 && p->social_target == c->id &&
         p->social_boot == c->boot && p->social_token == s->session &&
         p->boot == s->peer_boot && p->social_session == s->peer_session;
}
static bool matching(const hr_core_t *c) {
  if (!sync_card.active || !sync_card.peer || sync_card.peer > HR_PLAYERS)
    return false;
  const hr_peer_t *p = &c->peers[sync_card.peer];
  return c->id == owner_id && c->boot == sync_card.boot && c->social.active &&
         c->social.accepted && c->social.candidate == sync_card.peer &&
         c->social.session == sync_card.session &&
         c->social.peer_boot == sync_card.peer_boot &&
         c->social.peer_session == sync_card.peer_session &&
         p->boot == sync_card.peer_boot &&
         p->social_session == sync_card.peer_session &&
         p->social_boot == c->boot && p->social_token == sync_card.session &&
         p->social_target == c->id && (p->social_flags & 6) == 6 &&
         !memcmp(p->mac, sync_card.peer_mac, sizeof p->mac);
}
static const lv_image_dsc_t *portrait(unsigned id) {
  const lv_image_dsc_t *p = hr_profile(id)->portrait;
  return p && p->data && p->header.cf == LV_COLOR_FORMAT_RGB565 &&
                 p->header.w == 96 && p->header.h == 156 &&
                 p->header.stride == 192 && p->data_size >= 29952
             ? p
             : NULL;
}
static void local_wire(uint8_t wire[164]) {
  const hr_profile_t *p = hr_profile(owner_id);
  hr_edit_t edit = {.avatar = portrait(owner_id) != NULL};
  snprintf(edit.name, sizeof edit.name, "%s", p->name ? p->name : "");
  snprintf(edit.intro, sizeof edit.intro, "%s",
           p->signature ? p->signature : "");
  hr_edit_wire_encode(&edit, wire);
}
static bool changed(void) {
  uint8_t wire[164];
  local_wire(wire);
  const hr_edit_t *edit = hr_profile_current();
  const lv_image_dsc_t *p = portrait(owner_id);
  return p != sync_card.portrait || (p && p->data != sync_card.pixels) ||
         (edit && edit->revision != sync_card.revision) ||
         memcmp(wire, sync_card.wire, sizeof wire);
}
static unsigned sample(unsigned offset, uint8_t out[164]) {
  unsigned count = sync_card.local_bytes - offset;
  if (count > 164)
    count = 164;
  for (unsigned i = 0; i < count; i++) {
    unsigned byte = offset + i, pixel = byte / 2;
    unsigned x = pixel % 64, y = pixel / 64;
    out[i] =
        sync_card.pixels[(y * 156 / 104) * 192 + (x * 96 / 64) * 2 + byte % 2];
  }
  return count;
}
static void failure(hr_core_t *c) {
  c->social.failed = true;
  c->social.pending = false;
  hr_cards_reset();
}
void hr_cards_tick(hr_core_t *c, uint32_t now) {
  if (!mutual(c, now)) {
    hr_cards_reset();
    return;
  }
  if (sync_card.active && (!matching(c) || changed()))
    hr_cards_reset();
  if (sync_card.active && !c->social.complete &&
      ((uint32_t)(now - sync_card.progressed) >= 6000 ||
       (uint32_t)(now - sync_card.started) >= 20000)) {
    failure(c);
    return;
  }
  if (sync_card.active || c->held || c->speaker)
    return;
  sync_card.active = true;
  sync_card.peer = c->social.candidate;
  sync_card.boot = c->boot;
  sync_card.peer_boot = c->social.peer_boot;
  sync_card.session = c->social.session;
  sync_card.peer_session = c->social.peer_session;
  memcpy(sync_card.peer_mac, c->peers[sync_card.peer].mac,
         sizeof sync_card.peer_mac);
  const hr_edit_t *edit = hr_profile_current();
  sync_card.revision = edit ? edit->revision : 0;
  sync_card.portrait = portrait(owner_id);
  sync_card.pixels = sync_card.portrait ? sync_card.portrait->data : NULL;
  sync_card.local_bytes = sync_card.portrait ? HR_FRIEND_IMAGE_BYTES : 0;
  local_wire(sync_card.wire);
  uint32_t crc = hr_friend_crc_update(~0u, sync_card.wire, 164);
  uint8_t chunk[164];
  for (unsigned offset = 0; offset < sync_card.local_bytes; offset += 164)
    crc = hr_friend_crc_update(crc, chunk, sample(offset, chunk));
  sync_card.local_crc = ~crc;
  sync_card.next_meta = sync_card.next_request = sync_card.next_send = now;
  sync_card.started = sync_card.progressed = now;
}
void hr_cards_receive(hr_core_t *c, const hr_packet_t *p, uint32_t now) {
  hr_cards_tick(c, now);
  if (!matching(c) || c->held || c->speaker || p->id != sync_card.peer ||
      p->target != c->id || p->boot != sync_card.peer_boot ||
      p->request != sync_card.peer_session || p->epoch != sync_card.session)
    return;
  if (p->kind == CARD_META) {
    hr_edit_t edit;
    if (!hr_edit_wire_decode(&edit, p->audio) ||
        p->flags != (edit.avatar ? HR_FRIEND_IMAGE_BYTES : 0))
      return;
    if (sync_card.have_meta && p->token == sync_card.remote_crc &&
        !memcmp(p->audio, sync_card.remote_wire, 164)) {
      sync_card.next_request = now;
      return;
    }
    // Resume from a verified Flash snapshot after a lost ACK or volatile reset.
    if (hr_friend_store_matches(sync_card.peer, p->audio, p->token)) {
      memcpy(sync_card.remote_wire, p->audio, 164);
      sync_card.have_meta = sync_card.received = true;
      sync_card.remote_crc = p->token;
      sync_card.used = sync_card.remote_bytes = p->flags;
      sync_card.progressed = sync_card.next_request = now;
      return;
    }
    /* A completed add captures a snapshot, not a live profile subscription. */
    if (c->social.saved)
      return;
    /* A peer's changed snapshot replaces only the inactive transaction. */
    sync_card.have_meta = sync_card.received = false;
    if (!hr_friend_store_begin(sync_card.peer, sync_card.session, p->audio,
                               p->token)) {
      failure(c);
      return;
    }
    memcpy(sync_card.remote_wire, p->audio, 164);
    sync_card.have_meta = true;
    sync_card.remote_crc = p->token;
    sync_card.remote_bytes = p->flags;
    sync_card.used = 0;
    sync_card.progressed = now;
    sync_card.next_request = now;
    if (!p->flags) {
      if (!hr_friend_store_finish()) {
        failure(c);
        return;
      }
      sync_card.received = true;
    }
  } else if (p->kind == CARD_REQUEST && p->token == sync_card.local_crc) {
    if (p->flags == sync_card.local_bytes) {
      if (!sync_card.acknowledged) sync_card.progressed = now;
      sync_card.acknowledged = true;
      sync_card.response = false;
    } else if (p->flags < sync_card.local_bytes && p->flags % 164 == 0) {
      // This offset acknowledges bytes already committed by the peer. Count
      // forward send-side progress too (e.g. our avatar vs their text-only card).
      // Repeated requests for the same lost chunk cannot extend the deadline.
      if (p->flags > sync_card.peer_offset) {
        sync_card.peer_offset = p->flags;
        sync_card.progressed = now;
      }
      sync_card.response_offset = p->flags;
      sync_card.response = true;
    }
  } else if (p->kind == CARD_DATA && sync_card.have_meta &&
             !sync_card.received && p->token == sync_card.remote_crc &&
             p->flags == sync_card.used) {
    unsigned n = sync_card.remote_bytes - sync_card.used;
    if (n > 164)
      n = 164;
    if (!hr_friend_store_append(sync_card.used, p->audio, n)) {
      failure(c);
      return;
    }
    sync_card.used += n;
    sync_card.progressed = now;
    sync_card.next_request = now;
    if (sync_card.used == sync_card.remote_bytes) {
      if (!hr_friend_store_finish()) {
        failure(c);
        return;
      }
      sync_card.received = true;
    }
  }
}
bool hr_cards_send(hr_core_t *c, hr_packet_t *p, uint32_t now) {
  hr_cards_tick(c, now);
  if (!matching(c) || c->held || c->speaker || !due(now, sync_card.next_send))
    return false;
  memset(p, 0, sizeof *p);
  if (sync_card.response) {
    p->kind = CARD_DATA;
    p->flags = sync_card.response_offset;
    p->token = sync_card.local_crc;
    sample(p->flags, p->audio);
    sync_card.response = false;
  } else if (due(now, sync_card.next_meta)) {
    p->kind = CARD_META;
    p->flags = sync_card.local_bytes;
    p->token = sync_card.local_crc;
    memcpy(p->audio, sync_card.wire, 164);
    sync_card.next_meta = now + 400;
  } else if (sync_card.have_meta && due(now, sync_card.next_request)) {
    p->kind = CARD_REQUEST;
    p->flags = sync_card.used;
    p->token = sync_card.remote_crc;
    sync_card.next_request = now + (sync_card.received ? 400 : 80);
  } else
    return false;
  p->id = c->id;
  p->target = sync_card.peer;
  p->boot = c->boot;
  p->seq = ++c->seq;
  p->request = sync_card.session;
  p->epoch = sync_card.peer_session;
  sync_card.next_send = now + 12;
  return true;
}
bool hr_cards_ready(const hr_core_t *c, unsigned candidate) {
  return matching(c) && candidate == sync_card.peer && sync_card.received &&
         sync_card.acknowledged;
}
unsigned hr_cards_progress(const hr_core_t *c) { (void)c;
  if (!sync_card.active)
    return 0;
  if (sync_card.received)
    return sync_card.acknowledged ? 100 : 99;
  return sync_card.remote_bytes ? sync_card.used * 99 / sync_card.remote_bytes
                                : 0;
}

/* Host API bridge only. This is the original main loop's legacy save step. */
#include "hr_social.h"
void hr_cards_commit(hr_core_t *c,uint32_t now,hr_cards_save_fn save) {
 (void)now;
 if(c->social.pending&&!c->social.failed&&hr_cards_ready(c,c->social.candidate))
  hr_social_saved(c,save(c,c->social.friends|(1u<<c->social.candidate)));
}
