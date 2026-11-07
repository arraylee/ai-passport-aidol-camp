#include "hr_core.h"
#include "hr_game.h"
#include <string.h>
static bool due(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }
static bool fresh(uint32_t now, uint32_t at, uint32_t age) {
  return (uint32_t)(now - at) < age;
}
static bool valid_id(unsigned id) { return id >= 1 && id <= HR_PLAYERS; }
static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++)
    p[i] = (uint8_t)(v >> (8 * i));
}
uint32_t hr_crc(const void *data, size_t len) {
  const uint8_t *p = data;
  uint32_t c = ~0u;
  while (len--) {
    c ^= *p++;
    for (int i = 0; i < 8; i++)
      c = (c >> 1) ^ (0xedb88320u & -(c & 1));
  }
  return ~c;
}
size_t hr_pack(const hr_packet_t *p, uint8_t *out) {
  size_t n = (p->kind == HR_VOICE || p->kind == HR_PROFILE ||
              p->kind == HR_CARD_META || p->kind == HR_CARD_DATA ||
              p->kind == HR_PREVIEW_DATA)
                 ? 200
                 : 36;
  memset(out, 0, n);
  memcpy(out, "HSW1", 4);
  out[4] = p->kind;
  out[5] = p->id;
  out[6] = p->target;
  out[7] = p->speaker;
  put32(out + 8, p->boot);
  put32(out + 12, p->seq);
  put32(out + 16, p->request);
  put32(out + 20, p->epoch);
  put32(out + 24, p->token);
  put32(out + 28, p->flags);
  if (n == 200)
    memcpy(out + 32, p->audio, 164);
  put32(out + n - 4, hr_crc(out, n - 4));
  return n;
}
bool hr_unpack(hr_packet_t *p, const uint8_t *d, size_t n) {
  if (!p || !d || (n != 36 && n != 200) || memcmp(d, "HSW1", 4) ||
      get32(d + n - 4) != hr_crc(d, n - 4))
    return false;
  if (d[4] < HR_HELLO || d[4] > HR_KIND_MAX || !valid_id(d[5]) ||
      d[6] > HR_PLAYERS || d[7] > HR_PLAYERS ||
      (((d[4] == HR_VOICE || d[4] == HR_PROFILE || d[4] == HR_CARD_META ||
         d[4] == HR_CARD_DATA || d[4] == HR_PREVIEW_DATA)) != (n == 200)) ||
      !get32(d + 8))
    return false;
  memset(p, 0, sizeof *p);
  p->kind = d[4];
  p->id = d[5];
  p->target = d[6];
  p->speaker = d[7];
  p->boot = get32(d + 8);
  p->seq = get32(d + 12);
  p->request = get32(d + 16);
  p->epoch = get32(d + 20);
  p->token = get32(d + 24);
  p->flags = get32(d + 28);
  if (n == 200)
    memcpy(p->audio, d + 32, 164);
  return true;
}
void hr_init(hr_core_t *c, uint8_t id, const uint8_t mac[6], uint32_t boot,
             uint32_t now) {
  memset(c, 0, sizeof *c);
  c->id = valid_id(id) ? id : 0;
  memcpy(c->mac, mac, 6);
  c->boot = boot ? boot : 1;
  c->hardware_ok = true;
  c->host = 1;
  c->next_hello = now;
  c->next_control = now;
  c->host_ready_at = now + 900;
  if (id == 1)
    c->epoch = c->boot;
}
bool hr_online(const hr_core_t *c, uint8_t id, uint32_t now) {
  return valid_id(id) &&
         (id == c->id ||
          (c->peers[id].seen && fresh(now, c->peers[id].last, HR_OFFLINE)));
}
bool hr_conflict(const hr_core_t *c, uint8_t id, uint32_t now) {
  if (!valid_id(id))
    return false;
  uint32_t until =
      id == c->id ? c->conflict_until : c->peers[id].conflict_until;
  return until && !due(now, until);
}
bool hr_room_member(const hr_core_t *c, uint8_t id, uint32_t now) {
  if (!valid_id(id) || !hr_online(c, id, now) || hr_conflict(c, id, now))
    return false;
  if (!c->room_enabled)
    return true;
  if (id == c->id)
    return c->room_joined && (!c->identity_mode || c->hardware_ok);
  return (c->peers[id].social_flags & 16) &&
         fresh(now, c->peers[id].hello_at, HR_OFFLINE);
}
static bool ready(const hr_core_t *c, uint32_t now) {
  return c->id && c->hardware_ok && !hr_conflict(c, c->id, now) &&
         !hr_conflict(c, c->host, now) && hr_room_member(c, c->id, now) &&
         hr_room_member(c, c->host, now);
}
static void clear_floor(hr_core_t *c, uint32_t now) {
  c->speaker = 0;
  c->floor_request = 0;
  c->voice_seen = false;
  if (c->id == c->host) {
    c->host_ready_at = now + 650;
    c->token++;
    c->next_control = now;
  }
}
void hr_press(hr_core_t *c, uint8_t target, uint32_t now) {
  if (c->held)
    return;
  c->held = true;
  c->press_at = now;
  c->target = target;
  c->request++;
  if (!c->request)
    c->request++;
  c->blocked = c->game_busy || !ready(c, now) || target > HR_PLAYERS || target == c->id ||
               (target && (hr_game_peer_busy(c,target) || !hr_room_member(c, target, now) ||
                           hr_conflict(c, target, now))) ||
               (c->speaker && c->speaker != c->id);
  c->next_control = now;
}
void hr_release(hr_core_t *c, uint32_t now) {
  if (!c->held)
    return;
  c->held = false;
  c->blocked = false;
  c->release_request = c->request;
  c->releases = 3;
  c->next_control = now;
  if (c->speaker == c->id) {
    if (c->id == c->host)
      clear_floor(c, now);
    else
      c->speaker = 0;
  }
}
void hr_tick(hr_core_t *c, uint32_t now) {
  if (c->speaker) {
    bool expired =
        (c->speaker == c->id && !fresh(now, c->floor_client_at, HR_LEASE)) ||
        !fresh(now, c->floor_at, c->id == c->host ? HR_HOST_LEASE : HR_LEASE);
    bool invalid = hr_game_peer_busy(c,c->speaker) ||
        (c->floor_target && hr_game_peer_busy(c,c->floor_target)) ||
        !hr_room_member(c, c->speaker, now) ||
        (c->floor_target && !hr_room_member(c, c->floor_target, now)) ||
        !ready(c, now) || hr_conflict(c, c->speaker, now) ||
        (c->id == c->host && !fresh(now, c->floor_started, HR_MAX_TALK));
    if (expired || invalid) {
      // Expired permission stops audio immediately, but a still-held sender
      // may renew this same request. A fresh matching grant is still required;
      // competing speakers, invalid membership and the press cap stay latched.
      if (c->held && (invalid || c->speaker != c->id ||
                      c->floor_request != c->request))
        c->blocked = true;
      clear_floor(c, now);
    }
  }
  if (c->held && (!fresh(now, c->press_at, HR_MAX_TALK) || !ready(c, now))) {
    c->blocked = true;
    if (c->speaker == c->id)
      clear_floor(c, now);
  }
}
static void signal_update(hr_peer_t *p, int rssi, uint32_t now) {
  if (rssi > 0 || rssi < -110)
    return;
  if (!p->samples || !fresh(now, p->hello_at, HR_OFFLINE)) {
    p->signal_q8 = rssi * 256;
    for (int i = 0; i < 3; i++)
      p->signal_window[i] = (int8_t)rssi;
    p->zone = rssi >= -57 ? 2 : rssi >= -72 ? 1 : 0;
    p->anchor = rssi;
    p->trend_at = now;
    p->trend = 0;
    p->samples = 1;
    return;
  }
  p->signal_window[p->samples % 3] = (int8_t)rssi;
  int a = p->signal_window[0], b = p->signal_window[1],
      d0 = p->signal_window[2];
  if (a > b) {
    int tmp = a;
    a = b;
    b = tmp;
  }
  if (b > d0) {
    int tmp = b;
    b = d0;
    d0 = tmp;
  }
  if (a > b)
    b = a;
  // A short median rejects one isolated spike; faster EMA follows real
  // movement.
  int d = b * 256 - p->signal_q8;
  if (d > 16 * 256)
    d = 16 * 256;
  if (d < -16 * 256)
    d = -16 * 256;
  p->signal_q8 += d / 2;
  p->samples++;
  int r = p->signal_q8 / 256;
  if (p->zone == 0 && r >= -68)
    p->zone = 1;
  else if (p->zone == 1 && r <= -75)
    p->zone = 0;
  else if (p->zone == 1 && r >= -53)
    p->zone = 2;
  else if (p->zone == 2 && r <= -61)
    p->zone = 1;
  if ((uint32_t)(now - p->trend_at) >= 480) {
    int delta = r - p->anchor;
    p->trend = delta >= 4 ? 1 : delta <= -4 ? -1 : 0;
    p->anchor = r;
    p->trend_at = now;
  }
}
static void request_floor(hr_core_t *c, const hr_packet_t *p, uint32_t now) {
  if (c->id != c->host || !due(now, c->host_ready_at) || !ready(c, now) ||
      hr_game_peer_busy(c,p->id) || !hr_room_member(c, p->id, now) || !p->request || p->target == p->id ||
      p->target > HR_PLAYERS)
    return;
  if (p->target &&
      (hr_game_peer_busy(c,p->target) || !hr_room_member(c, p->target, now) || hr_conflict(c, p->target, now)))
    return;
  if (!c->speaker) {
    c->speaker = p->id;
    c->floor_target = p->target;
    c->floor_request = p->request;
    c->floor_started = now;
    c->floor_at = now;
    c->floor_client_at = p->flags;
    c->token++;
    c->next_control = now;
    c->voice_seen = false;
  } else if (c->speaker == p->id && c->floor_request == p->request &&
             c->floor_target == p->target) {
    c->floor_at = now;
    c->floor_client_at = p->flags;
  }
}
bool hr_receive(hr_core_t *c, const hr_packet_t *p, const uint8_t mac[6],
                int rssi, uint32_t now) {
  if (!valid_id(p->id) || !p->boot || p->target > HR_PLAYERS ||
      p->speaker > HR_PLAYERS)
    return false;
  if (p->id == c->id) {
    if (memcmp(mac, c->mac, 6)) {
      c->conflict_until = now + HR_OFFLINE;
      if (c->held)
        c->blocked = true;
      clear_floor(c, now);
    }
    return false;
  }
  hr_peer_t *q = &c->peers[p->id];
  if (q->seen && memcmp(q->mac, mac, 6) && hr_online(c, p->id, now)) {
    q->conflict_until = now + HR_OFFLINE;
    if (c->speaker == p->id || p->id == c->host)
      clear_floor(c, now);
    return false;
  }
  bool changed = !q->seen || memcmp(q->mac, mac, 6) || q->boot != p->boot;
  if (changed) {
    if (p->kind != HR_HELLO || p->boot == q->retired_boot)
      return false;
    uint32_t old = q->boot;
    memset(q, 0, sizeof *q);
    q->retired_boot = old;
    q->boot = p->boot;
    memcpy(q->mac, mac, 6);
    if (c->speaker == p->id || p->id == c->host)
      clear_floor(c, now);
  } else if ((int32_t)(p->seq - q->seq) <= 0)
    return false;
  if (p->kind == HR_HELLO) {
    if (c->identity_mode) {
      memcpy(q->sn, p->audio, 24);
      q->sn[24] = 0;
    }
    signal_update(q, rssi, now);
    q->hello_at = now;
    q->social_session = p->request;
    q->social_boot = p->epoch;
    q->social_token = p->token;
    uint32_t room=(p->flags&16) ? p->request : 0;
    if(q->room_session!=room || !(p->flags&16)) {
      q->score_valid=false;q->best_score=0;
    }
    q->room_session=room;
    q->social_target = p->target;
    q->social_flags = (uint8_t)p->flags;
    q->touch_target = (p->flags & 32) && (p->flags & 2) && p->request &&
                      valid_id(p->speaker) && p->speaker != p->id ? p->speaker : 0;
    if (p->flags & 1)
      q->conflict_until = now + HR_OFFLINE;
  }
  q->seen = true;
  q->seq = p->seq;
  q->last = now;
  if (p->kind == HR_SCORE && c->room_joined && hr_room_member(c,p->id,now) &&
      p->target==0 && p->request==q->room_session && q->room_session &&
      p->epoch==HR_GAME_VERSION && p->flags<=1 && p->token<=HR_SCORE_MAX) {
    if(p->flags && (!q->score_valid || p->token>q->best_score)) {q->score_valid=true;q->best_score=p->token;}
  }
  if (p->kind == HR_REQUEST)
    request_floor(c, p, now);
  if (p->kind == HR_RELEASE && c->id == c->host && c->speaker == p->id &&
      c->floor_request == p->request)
    clear_floor(c, now);
  if (p->kind == HR_GRANT && p->id == c->host && c->id != c->host && ready(c, now) &&
      (!p->speaker || hr_room_member(c, p->speaker, now)) &&
      p->epoch == p->boot) {
    // A released button must never revive a late grant for the previous press.
    bool altered = c->epoch != p->epoch || c->token != p->token;
    c->epoch = p->epoch;
    c->token = p->token;
    c->speaker = p->speaker;
    c->floor_request = p->request;
    c->floor_at = now;
    c->floor_client_at = p->flags;
    if (altered)
      c->voice_seen = false;
    if (c->held && p->speaker && p->speaker != c->id)
      c->blocked = true;
    c->floor_target = p->target;
  }
  if (p->kind != HR_VOICE)
    return false;
  if (c->game_busy || !ready(c, now) || !c->speaker || c->speaker == c->id ||
      p->id != c->speaker || !hr_room_member(c, p->id, now) ||
      p->epoch != c->epoch || p->token != c->token ||
      p->request != c->floor_request ||
      !fresh(now, c->floor_at, c->id == c->host ? HR_HOST_LEASE : HR_LEASE) ||
      p->target != c->floor_target || (p->target != 0 && p->target != c->id))
    return false;
  if (c->voice_seen && (int32_t)(p->seq - c->voice_seq) <= 0)
    return false;
  c->voice_seen = true;
  c->voice_seq = p->seq;
  return true;
}
static void packet(hr_core_t *c, hr_packet_t *p, uint8_t kind) {
  memset(p, 0, sizeof *p);
  p->kind = kind;
  p->id = c->id;
  p->boot = c->boot;
  p->seq = ++c->seq;
}
bool hr_control(hr_core_t *c, hr_packet_t *p, uint32_t now) {
  if (!c->id)
    return false;
  // A stale coordinator must not receive repeated held-key claims after it
  // reboots or disappears. Resume only while its accepted packets are fresh.
  bool request_due = c->held && !c->blocked &&
      (c->id == c->host || (valid_id(c->host) &&
       fresh(now,c->peers[c->host].last,HR_LEASE)));
  bool control_due = due(now, c->next_control) &&
      (c->id == c->host ? ready(c, now) :
       c->releases || request_due);
  // If radio callbacks delay the next send past both deadlines, service the
  // older deadline. Always choosing HELLO can starve REQUEST/GRANT forever.
  if (due(now, c->next_hello) &&
      (!control_due || (int32_t)(c->next_hello-c->next_control) <= 0)) {
    packet(c, p, HR_HELLO);
    p->flags = (hr_conflict(c, c->id, now) ? 1 : 0) |
               (c->room_enabled && c->room_joined && (!c->identity_mode || c->hardware_ok) ? 16 : 0) |
               (c->game_busy ? 64 : 0);
    if(c->room_joined)p->request=c->room_session;
    if (c->social.active) {
      p->flags |= 2 | (c->social.accepted ? 4 : 0) | (c->social.saved ? 8 : 0);
      if(c->social.manual)p->flags |= HR_SOCIAL_MANUAL_FLAG;
      p->request = c->social.session;
      p->target = c->social.candidate;
      p->epoch = c->social.peer_boot;
      p->token = c->social.peer_session;
      if (c->touch_target) {
        p->flags |= 32;
        p->speaker = c->touch_target; // HELLO-only reciprocal close-range intent.
      }
    }
    // During a floor, 500 ms presence is sufficient for the 3 s member TTL.
    // Reserve the short send opportunities for renewals and voice frames.
    c->next_hello = now + ((c->held || c->speaker) ? 500 : 120) + c->id * 3;
    return true;
  }
  if (!due(now, c->next_control))
    return false;
  c->next_control = now + 120;
  if (c->id == c->host) {
    if (!ready(c, now))
      return false;
    if (c->held && !c->blocked) {
      hr_packet_t req = {
          .id = c->id, .request = c->request, .target = c->target, .flags = now};
      request_floor(c, &req, now);
    }
    packet(c, p, HR_GRANT);
    p->speaker = c->speaker;
    p->target = c->floor_target;
    p->request = c->floor_request;
    p->epoch = c->boot;
    p->token = c->token;
    p->flags = c->floor_client_at;
    return true;
  }
  if (c->releases) {
    packet(c, p, HR_RELEASE);
    p->request = c->release_request;
    c->releases--;
    return true;
  }
  if (request_due) {
    packet(c, p, HR_REQUEST);
    p->flags = now;
    p->request = c->request;
    p->target = c->target;
    return true;
  }
  return false;
}
bool hr_can_talk(const hr_core_t *c, uint32_t now) {
  return c->held && !c->game_busy && !c->blocked && ready(c, now) && c->speaker == c->id &&
         c->floor_request == c->request &&
         fresh(now, c->floor_client_at, HR_LEASE) &&
         fresh(now, c->floor_at, c->id == c->host ? HR_HOST_LEASE : HR_LEASE) &&
         fresh(now, c->press_at, HR_MAX_TALK);
}
bool hr_voice(hr_core_t *c, hr_packet_t *p, const uint8_t audio[HR_AUDIO],
              uint32_t now) {
  if (!hr_can_talk(c, now))
    return false;
  packet(c, p, HR_VOICE);
  p->speaker = c->id;
  p->target = c->target;
  p->epoch = c->epoch;
  p->token = c->token;
  p->request = c->request;
  memcpy(p->audio, audio, HR_AUDIO);
  return true;
}
void hr_config_encode(uint8_t id, uint8_t volume, uint8_t out[12]) {
  memset(out, 0, 12);
  memcpy(out, "HRC1", 4);
  out[4] = id;
  out[5] = volume;
  put32(out + 8, hr_crc(out, 8));
}
bool hr_config_decode(const uint8_t in[12], uint8_t *id, uint8_t *volume) {
  if (memcmp(in, "HRC1", 4) || !valid_id(in[4]) || in[5] > 100 ||
      get32(in + 8) != hr_crc(in, 8))
    return false;
  *id = in[4];
  *volume = in[5];
  return true;
}

void hr_room_join(hr_core_t *c, bool joined, uint32_t now) {
  hr_release(c, now);
  clear_floor(c, now);
  bool next=joined && c->id;
  if(next && !c->room_joined) {if(!++c->room_session)c->room_session=1;}
  c->room_joined = next;
  if(!next)c->game_busy=false;
  c->next_hello = now;
}
void hr_room_enable(hr_core_t *c, uint32_t now) {
  c->room_enabled = true;
  hr_room_join(c, false, now);
}

bool hr_game_peer_busy(const hr_core_t *c,uint8_t id) {
  return id==c->id ? c->game_busy : id>=1 && id<=HR_PLAYERS && (c->peers[id].social_flags&64);
}
bool hr_score_packet(hr_core_t *c,hr_packet_t *p,uint32_t now) {
  if(!c->room_joined || !hr_room_member(c,c->id,now) || c->held || c->speaker)return false;
  *p=(hr_packet_t){.kind=HR_SCORE,.id=c->id,.boot=c->boot,.seq=++c->seq,
    .request=c->room_session,.epoch=HR_GAME_VERSION,.flags=c->best_valid?1u:0u,.token=c->best_valid?c->best_score:0};
  return true;
}
unsigned hr_ranked(const hr_core_t *c,uint32_t now,uint8_t ids[HR_PLAYERS]) {
  unsigned n=0;
  if(!c->room_joined)return 0;
  for(unsigned id=1;id<=HR_PLAYERS;id++)if(hr_room_member(c,id,now) &&
      (id==c->id?c->best_valid:c->peers[id].score_valid))ids[n++]=id;
  for(unsigned i=1;i<n;i++) {
    uint8_t v=ids[i];unsigned value=v==c->id?c->best_score:c->peers[v].best_score,j=i;
    while(j) {
      unsigned previous=ids[j-1]==c->id?c->best_score:c->peers[ids[j-1]].best_score;
      if(previous>value || (previous==value && ids[j-1]<v))break;
      ids[j]=ids[j-1];j--;
    }
    ids[j]=v;
  }
  return n;
}

void hr_coordinator_set(hr_core_t *c,uint8_t host,uint32_t now) {
  if(c->host==host)return;
  c->host=host;clear_floor(c,now);c->epoch=host==c->id?c->boot:0;
  c->floor_at=0;c->floor_client_at=0;c->floor_target=0;c->voice_seen=false;
  c->host_ready_at=now+900;c->next_control=now;
  if(c->held)c->blocked=true;
}
