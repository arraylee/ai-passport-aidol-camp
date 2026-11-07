#include "hr_cards.h"
#include "hr_social.h"
#include "hr_profile_store.h"
#include "esp_partition.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t flash[0x30000], avatar[HR_FRIEND_IMAGE_BYTES], own_pixels[29952];
static int fail_write = -1, writes, erases;
static bool fail_erase;
static const esp_partition_t partition = {0x3b0000, sizeof flash};
static hr_edit_t local_edit;
static lv_image_dsc_t own_image = {
    .header = {.cf = LV_COLOR_FORMAT_RGB565, .w = 96, .h = 156, .stride = 192},
    .data_size = sizeof own_pixels, .data = own_pixels};
static hr_profile_t local = {.name = "Local", .signature = "Own intro", .url = ""};
const hr_profile_t *hr_profile(unsigned id) { (void)id; return &local; }
const hr_edit_t *hr_profile_current(void) { return &local_edit; }
const esp_partition_t *esp_partition_find_first(int type, int subtype,
                                                const char *label) {
  assert(type == ESP_PARTITION_TYPE_DATA && subtype == 0x42);
  assert(!strcmp(label, "friend_store"));
  return &partition;
}
int esp_partition_mmap(const esp_partition_t *p, unsigned offset, unsigned size,
                       int type, const void **out, esp_partition_mmap_handle_t *h) {
  (void)type; (void)h;
  assert(p == &partition && !offset && size == sizeof flash);
  *out = flash;
  return ESP_OK;
}
int esp_partition_erase_range(const esp_partition_t *p, unsigned at, unsigned n) {
  assert(p == &partition && at + n <= sizeof flash && !(at % 4096) && !(n % 4096));
  if (fail_erase) return -1;
  memset(flash + at, 0xff, n);
  erases++;
  return ESP_OK;
}
int esp_partition_write(const esp_partition_t *p, unsigned at,
                        const void *data, size_t n) {
  assert(p == &partition && at + n <= sizeof flash);
  if (fail_write >= 0 && writes++ == fail_write) return -1;
  const uint8_t *in = data;
  for (size_t i = 0; i < n; i++) {
    assert((flash[at + i] & in[i]) == in[i]);
    flash[at + i] &= in[i];
  }
  return ESP_OK;
}
static uint32_t make_wire(uint8_t wire[164], const char *name, bool image) {
  hr_edit_t edit = {.avatar = image};
  snprintf(edit.name, sizeof edit.name, "%s", name);
  snprintf(edit.intro, sizeof edit.intro, "Signature %s", name);
  hr_edit_wire_encode(&edit, wire);
  uint32_t crc = hr_friend_crc_update(~0u, wire, 164);
  if (image) crc = hr_friend_crc_update(crc, avatar, sizeof avatar);
  return ~crc;
}
static void append_all(void) {
  for (unsigned at = 0; at < sizeof avatar; at += 164) {
    unsigned n = sizeof avatar - at;
    if (n > 164) n = 164;
    assert(hr_friend_store_append(at, avatar + at, n));
  }
}
static void bind_owner(unsigned owner) {
  hr_core_t c={.id=(uint8_t)owner,.mac={0x4c,0x11,0xae,0,0,1}};
  for(unsigned i=1;i<=6;i++)if(i!=owner){uint8_t m[6]={0x4c,0x11,0xae,0,0,(uint8_t)i};memcpy(c.peers[i].mac,m,6);}
  hr_friend_store_bind(&c);
}
static bool store_init(unsigned owner){bind_owner(owner);return hr_friend_store_init(owner);}
static void store_tests(void) {
  uint8_t wire[164];
  uint32_t crc = make_wire(wire, "Old", true);
  assert(store_init(1));
  assert(hr_friend_store_begin(2, 11, wire, crc));
  assert(!hr_friend_store_append(164, avatar, 164));
  append_all();
  assert(hr_friend_store_finish());
  assert(!memcmp(flash+2*HR_FRIEND_BANK,"HFC2",4));
  uint8_t *header=flash+2*HR_FRIEND_BANK;
  memcpy(header,"HFC1",4);header[6]=1;
  uint32_t old_crc=hr_crc(header,248);for(unsigned j=0;j<4;j++)header[248+j]=old_crc>>(8*j);
  assert(store_init(1)&&!hr_friend_profile(2));
  memcpy(header,"HFC2",4);header[6]=2;
  old_crc=hr_crc(header,248);for(unsigned j=0;j<4;j++)header[248+j]=old_crc>>(8*j);
  assert(store_init(1)&&hr_friend_profile(2));
  assert(!strcmp(hr_friend_profile(2)->name, "Old"));
  assert(!memcmp(hr_friend_profile(2)->portrait->data, avatar, sizeof avatar));
  hr_core_t changed= { .id=1,.mac={0x4c,0x11,0xae,0,0,1} };
  uint8_t uid[6]={0x4c,0x11,0xae,0,0,99};memcpy(changed.peers[2].mac,uid,6);
  hr_friend_store_bind(&changed);assert(hr_friend_store_init(1));assert(!hr_friend_profile(2));
  bind_owner(1);assert(hr_friend_store_init(1));assert(hr_friend_profile(2));
  changed.mac[5]=99;hr_friend_store_bind(&changed);assert(hr_friend_store_init(1));assert(!hr_friend_profile(2));
  bind_owner(1);assert(hr_friend_store_init(1));
  assert(store_init(1));
  assert(!strcmp(hr_friend_profile(2)->name, "Old"));
  const uint8_t *thumb = hr_friend_profile(2)->small->data;
  for (unsigned y = 0; y < 32; y++)
    for (unsigned x = 0; x < 32; x++)
      assert(!memcmp(thumb + (y * 32 + x) * 2,
          avatar + ((y * 104 / 32) * 64 + x * 2) * 2, 2));
  crc = make_wire(wire, "New", true);
  fail_erase = true;
  assert(!hr_friend_store_begin(2, 12, wire, crc));
  fail_erase = false;
  assert(!strcmp(hr_friend_profile(2)->name, "Old"));
  wire[163] = 1; /* Reject noncanonical reserved metadata. */
  assert(!hr_friend_store_begin(2, 12, wire, crc));
  wire[163] = 0;
  assert(hr_friend_store_begin(2, 12, wire, crc ^ 1));
  append_all();
  assert(!hr_friend_store_finish());
  assert(store_init(1));
  assert(!strcmp(hr_friend_profile(2)->name, "Old"));
  /* Power loss after each possible image/thumbnail/header/commit write. */
  for (int cut = 0; cut < 116; cut++) {
    assert(hr_friend_store_begin(2, 13, wire, crc));
    fail_write = cut; writes = 0;
    bool ok = true;
    for (unsigned at = 0; at < sizeof avatar && ok; at += 164) {
      unsigned n = sizeof avatar - at;
      if (n > 164) n = 164;
      ok = hr_friend_store_append(at, avatar + at, n);
    }
    if (ok) ok = hr_friend_store_finish();
    assert(!ok);
    fail_write = -1;
    assert(store_init(1));
    assert(!strcmp(hr_friend_profile(2)->name, "Old"));
  }
  assert(hr_friend_store_begin(2, 14, wire, crc));
  append_all();
  assert(hr_friend_store_finish());
  assert(store_init(1));
  assert(!strcmp(hr_friend_profile(2)->name, "New"));
  /* Corruption in newest committed image falls back to old bank. */
  unsigned newest = 3 * HR_FRIEND_BANK + HR_FRIEND_IMAGE_OFFSET;
  flash[newest] ^= 1;
  assert(store_init(1));
  assert(!strcmp(hr_friend_profile(2)->name, "Old"));
  assert(store_init(3));
  assert(!hr_friend_profile(2));
  assert(store_init(1));
  crc = make_wire(wire, "No avatar", false);
  assert(hr_friend_store_begin(2, 15, wire, crc));
  assert(hr_friend_store_finish());
  assert(!hr_friend_profile(2)->portrait && !hr_friend_profile(2)->small);
}
static hr_core_t core(uint32_t now) {
  hr_core_t c = {0};
  uint8_t mac[6] = {0x4c,0x11,0xae,0,0,1};
  hr_init(&c, 1, mac, 100, now);
  c.social = (hr_social_t){.active = true, .accepted = true, .pending = true,
      .candidate = 2, .session = 20, .peer_boot = 200, .peer_session = 30};
  c.peers[2] = (hr_peer_t){.seen = true, .boot = 200, .last = now,
      .hello_at = now, .zone = 2, .social_flags = 6, .social_target = 1,
      .social_boot = 100, .social_token = 20, .social_session = 30};
  const uint8_t peer_uid[6]={0x4c,0x11,0xae,0,0,2};memcpy(c.peers[2].mac,peer_uid,6);
  return c;
}
static hr_packet_t packet(unsigned kind, uint32_t crc, unsigned offset) {
  return (hr_packet_t){.kind = kind, .id = 2, .target = 1, .boot = 200,
      .request = 30, .epoch = 20, .token = crc, .flags = offset};
}
static unsigned save_attempts;
static bool retry_save(const hr_core_t *c,uint8_t friends) {
  assert(c->id==1&&(friends&(1u<<2)));return ++save_attempts>=3;
}
static bool never_save(const hr_core_t*c,uint8_t mask){(void)c;(void)mask;return false;}
static void protocol_tests(void) {
  bind_owner(1);
  assert(hr_cards_init(1));
  uint32_t now = 1000;
  hr_core_t c = core(now);
  hr_packet_t out;
  local.portrait = &own_image;
  hr_cards_tick(&c, now);
  assert(hr_cards_send(&c, &out, now));
  assert(out.kind == 7 && out.flags == sizeof avatar);
  uint32_t own_crc = out.token;
  hr_packet_t meta = packet(7, 0, sizeof avatar);
  meta.token = make_wire(meta.audio, "Remote", true);
  hr_packet_t bad = meta;
  bad.epoch--;
  int erase_before = erases;
  hr_cards_receive(&c, &bad, now);
  assert(erases == erase_before);
  bad = meta; bad.boot++;
  hr_cards_receive(&c, &bad, now);
  assert(erases == erase_before);
  hr_cards_receive(&c, &meta, now);
  assert(erases == erase_before + 1);
  hr_cards_receive(&c, &meta, now);
  assert(erases == erase_before + 1);
  /* Lost data retries exactly the missing offset. */
  assert(hr_cards_send(&c, &out, now + 12));
  assert(out.kind == 8 && out.flags == 0);
  assert(hr_cards_send(&c, &out, now + 132));
  assert(out.kind == 8 && out.flags == 0);
  hr_packet_t data = packet(9, meta.token, 164);
  memcpy(data.audio, avatar + 164, 164);
  hr_cards_receive(&c, &data, now + 132);
  assert(hr_cards_progress(&c) == 0);
  // Retain the other three chunks of a window when its first chunk is lost.
  for(unsigned at=328;at<656;at+=164) {
    data=packet(9,meta.token,at);memcpy(data.audio,avatar+at,164);
    hr_cards_receive(&c,&data,now+132);
  }
  data=packet(9,meta.token,0);memcpy(data.audio,avatar,164);
  hr_cards_receive(&c,&data,now+132);
  hr_cards_stats_t buffered;hr_cards_stats(&buffered,now+132);assert(buffered.rx_bytes==656);now+=132;
  for (unsigned at = 0; at < sizeof avatar; at += 164) {
    now += 20;
    c.peers[2].last = c.peers[2].hello_at = now;
    data = packet(9, meta.token, at);
    unsigned n = sizeof avatar - at;
    if (n > 164) n = 164;
    memcpy(data.audio, avatar + at, n);
    hr_cards_receive(&c, &data, now);
    unsigned progress = hr_cards_progress(&c);
    hr_cards_receive(&c, &data, now); /* Duplicate has no second write. */
    assert(progress == hr_cards_progress(&c));
  }
  assert(hr_cards_ready(&c, 2)); /* Local verified snapshot is ready to commit. */
  assert(hr_cards_progress(&c)<100);
  assert(!strcmp(hr_friend_profile(2)->name, "Remote"));
  hr_packet_t ack = packet(8, own_crc, sizeof avatar);
  hr_cards_receive(&c, &ack, now);
  assert(hr_cards_ready(&c, 2) && hr_cards_progress(&c) < 100);
  assert(!hr_cards_ready(&c, 3));
  save_attempts=0;
  hr_cards_commit(&c,now,retry_save);assert(!c.social.saved&&!c.social.failed&&save_attempts==1);
  hr_cards_commit(&c,now+100,retry_save);assert(save_attempts==1);
  c.peers[2].last=c.peers[2].hello_at=now+500;
  hr_cards_commit(&c,now+500,retry_save);assert(!c.social.saved&&!c.social.failed&&save_attempts==2);
  c.peers[2].last=c.peers[2].hello_at=now+1000;
  hr_cards_commit(&c,now+1000,retry_save);assert(c.social.saved&&!c.social.failed&&save_attempts==3);
  assert(hr_cards_progress(&c)<100);
  c.social.complete=true;assert(hr_cards_progress(&c)==100);
  c.social.complete=c.social.saved=false;now+=1000;
  c.peers[2].mac[0]++;
  assert(!hr_cards_ready(&c, 2));
  c.peers[2].mac[0]--;
  /* Own canonical image is actually sent using the requested offset. */
  hr_packet_t request = packet(8, own_crc, 164);
  hr_cards_receive(&c, &request, now);
  bool got_data=false;
  for(unsigned wait=5;wait<=100&&!got_data;wait+=5)
    got_data=hr_cards_send(&c,&out,now+wait)&&out.kind==9&&out.flags==164;
  assert(got_data);
  for (unsigned i = 0; i < 164; i++) {
    unsigned pos = i + 164, px = pos / 2;
    assert(out.audio[i] == own_pixels[(px / 64 * 156 / 104) * 192 +
                                      (px % 64 * 96 / 64) * 2 + pos % 2]);
  }
  c.held = true;
  assert(!hr_cards_send(&c, &out, now + 24));
  c.held = false;
  c.speaker = 2;
  assert(!hr_cards_send(&c, &out, now + 24));
  c.speaker = 0;
  local_edit.revision++;
  hr_cards_tick(&c, now + 24);
  assert(!hr_cards_ready(&c, 2) && hr_cards_progress(&c) == 0);
  c.social.peer_session++;
  hr_cards_tick(&c, now + 25);
  assert(hr_cards_progress(&c) == 0 && !hr_cards_ready(&c, 2));
  c = core(now);
  local.portrait = NULL;
  hr_cards_reset();
  hr_cards_tick(&c, now);
  assert(hr_cards_send(&c, &out, now) && out.kind == 7 && !out.flags);
  own_crc = out.token;
  meta = packet(7, 0, 0);
  meta.token = make_wire(meta.audio, "Text only", false);
  hr_cards_receive(&c, &meta, now);
  ack = packet(8, own_crc, 0);
  hr_cards_receive(&c, &ack, now);
  assert(hr_cards_ready(&c, 2));
  c.social.saved = true;
  meta.token = make_wire(meta.audio, "Unrequested update", false);
  hr_cards_receive(&c, &meta, now);
  assert(!strcmp(hr_friend_profile(2)->name, "Text only"));
  hr_cards_reset();
  hr_cards_tick(&c, now);
  assert(hr_cards_send(&c, &out, now) && out.kind==7);
  // Same verified snapshot may be acknowledged without a fresh Flash erase.
  erase_before=erases;meta.token=make_wire(meta.audio,"Text only",false);
  hr_cards_receive(&c,&meta,now);assert(erases==erase_before);
  assert(hr_cards_send(&c,&out,now+12)&&out.kind==8&&out.flags==0);
  assert(hr_cards_init(1));
  assert(!strcmp(hr_friend_profile(2)->name, "Text only"));
  assert(!hr_friend_profile(2)->portrait);
  // A permanent roster error is bounded and preserves the verified card.
  now+=2000;c=core(now);hr_cards_reset();hr_cards_tick(&c,now);
  meta=packet(7,0,0);meta.token=make_wire(meta.audio,"Text only",false);
  hr_cards_receive(&c,&meta,now);
  for(unsigned elapsed=0;elapsed<=6000;elapsed+=500) {
    c.peers[2].last=c.peers[2].hello_at=now+elapsed;
    hr_social_tick(&c,now+elapsed);hr_cards_commit(&c,now+elapsed,never_save);
  }
  hr_cards_stats_t failed_save;hr_cards_stats(&failed_save,now+6000);
  assert(c.social.failed&&!c.social.saved&&failed_save.reason==HR_CARDS_ROSTER);
  assert(!strcmp(hr_friend_profile(2)->name,"Text only"));
  now+=6100;
  // A stalled exchange fails visibly and does not erase/restart forever.
  c = core(now);hr_cards_reset();hr_cards_tick(&c,now);
  for(unsigned elapsed=120;elapsed<=6500;elapsed+=120) {
    c.peers[2].last=c.peers[2].hello_at=now+elapsed;
    hr_cards_tick(&c,now+elapsed);
  }
  assert(c.social.failed);
  erase_before=erases;
  for(unsigned elapsed=6600;elapsed<30000;elapsed+=120) {
    c.peers[2].last=c.peers[2].hello_at=now+elapsed;
    hr_social_tick(&c,now+elapsed);hr_cards_tick(&c,now+elapsed);
    assert(c.social.failed&&!c.social.pending&&!hr_cards_send(&c,&out,now+elapsed));
  }
  assert(erases==erase_before);
  hr_social_confirm(&c,now+29900);assert(!c.social.failed);
  // Duplicate requests cannot keep a dead transfer alive; real forward
  // requests may refresh the short timer, but never the 20s total deadline.
  local.portrait=&own_image;
  for(unsigned advancing=0;advancing<2;advancing++) {
    now=1000;c=core(now);hr_cards_reset();hr_cards_tick(&c,now);
    assert(hr_cards_send(&c,&out,now)&&out.kind==7);
    uint32_t token=out.token,limit=advancing?20000:6000;
    for(uint32_t elapsed=1000;elapsed<=limit;elapsed+=1000) {
      c.peers[2].last=c.peers[2].hello_at=now+elapsed;
      request=packet(8,token,advancing?(elapsed/1000)*164:0);
      hr_cards_receive(&c,&request,now+elapsed);
      if(elapsed<limit)assert(!c.social.failed);
    }
    assert(c.social.failed);
  }
  local.portrait=NULL;
  // Deadline arithmetic also works across the 32-bit millisecond rollover.
  now=0xffffff00u;c=core(now);hr_cards_reset();hr_cards_tick(&c,now);
  c.peers[2].last=c.peers[2].hello_at=now+5999u;
  hr_cards_tick(&c,now+5999u);assert(!c.social.failed);
  c.peers[2].last=c.peers[2].hello_at=now+6000u;
  hr_cards_tick(&c,now+6000u);assert(c.social.failed);
  now=1000;
  c = core(now);
  hr_cards_tick(&c, now);
  hr_cards_tick(&c, now + 801);
  assert(!hr_cards_ready(&c, 2) && !hr_cards_progress(&c));
}
int main(void) {
  memset(flash, 0xff, sizeof flash);
  for (unsigned i = 0; i < sizeof avatar; i++) avatar[i] = i * 13 + i / 64;
  for (unsigned i = 0; i < sizeof own_pixels; i++) own_pixels[i] = i * 7 + i / 192;
  store_tests();
  protocol_tests();
  puts("cards: persistence, fault injection, retry and session tests passed");
}
