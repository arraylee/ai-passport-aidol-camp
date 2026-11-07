#include "hr_cards.h"
#include "hr_identity.h"
#include "hr_social.h"
#include "hr_profile_store.h"
#include "esp_partition.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool b_hr_cards_init(unsigned);
void b_hr_friend_store_bind(const hr_core_t *);
bool b_hr_cards_send(hr_core_t *, hr_packet_t *, uint32_t);
void b_hr_cards_receive(hr_core_t *, const hr_packet_t *, uint32_t);
void b_hr_cards_reset(void);
void b_hr_cards_commit(hr_core_t *,uint32_t,hr_cards_save_fn);
void b_hr_cards_tick(hr_core_t *, uint32_t);
bool b_hr_cards_ready(const hr_core_t *, unsigned);
const hr_profile_t *b_hr_friend_profile(unsigned);

static uint8_t flash[2][0x30000], pixels[2][29952];
static esp_partition_t partitions[2] = {{0x3b0000, 0x30000}, {0x3b0000, 0x30000}};
static unsigned selecting;
static bool full_protocol;
static bool manual;
static unsigned save_attempts[2],fail_saves;
static bool save_a(const hr_core_t*c,uint8_t mask){assert(c->id==2&&(mask&2));return ++save_attempts[0]>fail_saves;}
static bool save_b(const hr_core_t*c,uint8_t mask){assert(c->id==2&&(mask&2));return ++save_attempts[1]>fail_saves;}
static lv_image_dsc_t portraits[2];
static hr_profile_t profiles[2];
static hr_edit_t edits[2];
const hr_profile_t *hr_profile(unsigned id) { (void)id; return &profiles[0]; }
const hr_profile_t *b_hr_profile(unsigned id) { (void)id; return &profiles[1]; }
const hr_edit_t *hr_profile_current(void) { return &edits[0]; }
const hr_edit_t *b_hr_profile_current(void) { return &edits[1]; }
const esp_partition_t *esp_partition_find_first(int type, int subtype,
                                                const char *label) {
  assert(type == ESP_PARTITION_TYPE_DATA && subtype == 0x42 &&
         !strcmp(label, "friend_store"));
  return &partitions[selecting];
}
int esp_partition_mmap(const esp_partition_t *p, unsigned at, unsigned n, int type,
                       const void **out, esp_partition_mmap_handle_t *handle) {
  (void)type; (void)handle;
  assert(!at && n == sizeof flash[0]);
  *out = flash[p - partitions];
  return ESP_OK;
}
int esp_partition_erase_range(const esp_partition_t *p, unsigned at, unsigned n) {
  assert(at + n <= sizeof flash[0]);
  memset(flash[p - partitions] + at, 0xff, n);
  return ESP_OK;
}
int esp_partition_write(const esp_partition_t *p, unsigned at,
                        const void *data, size_t n) {
  assert(at + n <= sizeof flash[0]);
  uint8_t *dest = flash[p - partitions] + at;
  const uint8_t *src = data;
  for (size_t i = 0; i < n; i++) {
    assert((dest[i] & src[i]) == src[i]);
    dest[i] &= src[i];
  }
  return ESP_OK;
}
static void receive(hr_core_t c[2], unsigned sender, hr_packet_t *p, uint32_t now) {
  uint8_t wire[HR_IDENTITY_WIRE_MAX];
  hr_identity_frame_t frame;
  size_t n=hr_identity_pack(&c[sender],p,wire);
  assert(n==(p->kind==HR_HELLO?72u:p->kind==8?48u:212u));
  assert(hr_identity_unpack(&frame,wire,n));
  assert(hr_identity_route(&c[!sender],&frame,c[sender].mac,now));
  p=&frame.p;
  hr_peer_t *peer=&c[!sender].peers[1];
  if(full_protocol) {
    (void)hr_receive(&c[!sender],p,c[sender].mac,manual?-62:-20,now);
    if(p->kind==HR_HELLO)return;
    if(p->boot!=peer->boot||p->seq!=peer->seq||peer->last!=now)return;
  } else {
    if(p->boot!=peer->boot||(int32_t)(p->seq-peer->seq)<=0)return;
    peer->seq=p->seq;
  }
  if (sender) hr_cards_receive(&c[0], p, now);
  else b_hr_cards_receive(&c[1], p, now);
}
int main(int argc, char **argv) {
  bool resume_test=argc>1&&!strcmp(argv[1],"resume-saved"), injected=false;
  full_protocol=argc>2;bool asymmetric=argc>3&&!strcmp(argv[3],"text-avatar");
  manual=argc>4&&!strcmp(argv[4],"manual");
  bool multiline=manual||(argc>4&&!strcmp(argv[4],"multiline"));
  unsigned rng=argc>5?(unsigned)strtoul(argv[5],NULL,10):0;bool fuzz=argc>5;
  unsigned loop_ms=fuzz?12+rng%14:asymmetric?30:12;
  bool speed=argc>4&&!strcmp(argv[4],"speed"),tail_loss=argc>4&&!strcmp(argv[4],"tail-loss");
  if(asymmetric)resume_test=false;
  if(argc>4&&!strcmp(argv[4],"roster-retry"))fail_saves=2;
  memset(flash, 0xff, sizeof flash);
  hr_core_t c[2] = {0};
  for (unsigned i = 0; i < 2; i++) {
    for (unsigned at = 0; at < sizeof pixels[i]; at++)
      pixels[i][at] = at * (i + 3) + at / 192;
    portraits[i] = (lv_image_dsc_t){
        .header = {.cf = LV_COLOR_FORMAT_RGB565, .w = 96, .h = 156, .stride = 192},
        .data_size = sizeof pixels[i], .data = pixels[i]};
    profiles[i] = (hr_profile_t){.name = i ? "Bravo" : "Alpha",
        .signature = i ? "B signature" : "A signature", .portrait = &portraits[i]};
    c[i].id = 2;
    uint8_t mac[6]={0x4c,0x11,0xae,0,0,(uint8_t)(10+i)};memcpy(c[i].mac,mac,6);
    hr_identity_enable(&c[i]);
    c[i].boot = 100 + i;
    c[i].social = (hr_social_t){.active = true, .accepted = true, .pending = true, .manual=manual,
        .candidate = 1, .session = 10 + i, .peer_session = 11 - i,
        .peer_boot = 101 - i};
    c[i].peers[1] = (hr_peer_t){.seen = true, .boot = 101 - i, .last = 1000, .hello_at = 1000,
        .zone = manual?1:2, .signal_q8=(manual?-62:-20)*256,
        .social_flags = 6|(manual?HR_SOCIAL_MANUAL_FLAG:0), .social_target = 2,
        .social_session = 11 - i, .social_boot = 100 + i, .social_token = 10 + i};
  }
  if(asymmetric)profiles[1].portrait=NULL;
  if(multiline){profiles[1].name="喵喵";profiles[1].signature="FoloToy\n简单开放\n人人可造";}
  for(unsigned i=0;i<2;i++)memcpy(c[i].peers[1].mac,c[!i].mac,6);
  hr_friend_store_bind(&c[0]);b_hr_friend_store_bind(&c[1]);
  selecting = 0;
  assert(hr_cards_init(2));
  selecting = 1;
  assert(b_hr_cards_init(2));
  hr_packet_t delayed[2];
  bool waiting[2] = {false};
  unsigned sent = 0, dropped = 0, reordered = 0;
  uint32_t now;
  for (now = 1000; now < 60000; now += loop_ms) {
    for (unsigned i = 0; i < 2; i++)
      if(!full_protocol)c[i].peers[1].last = c[i].peers[1].hello_at = now;
    if(full_protocol)for(unsigned i=0;i<2;i++){hr_tick(&c[i],now);hr_social_tick(&c[i],now);}
    hr_cards_tick(&c[0], now);
    b_hr_cards_tick(&c[1], now);
    for (unsigned i = 0; i < 2; i++) {
      hr_packet_t p;
      bool send = (full_protocol&&hr_control(&c[i],&p,now)) || (i ? b_hr_cards_send(&c[1], &p, now) : hr_cards_send(&c[0], &p, now));
      if (!send) continue;
      sent++;
      // Lose every image-completion ACK; durable-save HELLOs still arrive.
      if(tail_loss&&p.kind==HR_CARD_REQUEST&&p.flags==HR_FRIEND_IMAGE_BYTES)continue;
      // Withhold only A's final ACK, leaving A complete before B.
      if(resume_test&&!injected&&i==0&&p.kind==8&&p.flags==HR_FRIEND_IMAGE_BYTES)continue;
      rng=rng*1664525u+1013904223u;
      if (fuzz?rng%100<20:sent%7==0) { dropped++; continue; }
      if (sent % 11 == 0 && !waiting[i]) {
        delayed[i] = p; waiting[i] = true; continue;
      }
      receive(c, i, &p, now);
      receive(c, i, &p, now); /* Network duplicates. */
      if (waiting[i]) {
        receive(c, i, &delayed[i], now);
        waiting[i] = false;
        reordered++;
      }
    }
    if(resume_test&&!injected&&hr_cards_ready(&c[0],1)) {
      c[0].social.saved=true;c[1].peers[1].social_flags|=8;
      hr_cards_reset(); // E.g. a temporary link/profile pause after local commit.
      injected=true;
    }
    if(full_protocol) {
      hr_cards_commit(&c[0],now,save_a);b_hr_cards_commit(&c[1],now,save_b);
      if(c[0].social.complete&&c[1].social.complete)break;
    } else if (hr_cards_ready(&c[0], 1) && b_hr_cards_ready(&c[1], 1)) break;
  }
  if(now>=(asymmetric?21000u:15000u))for(unsigned i=0;i<2;i++)fprintf(stderr,"node%u now=%u active=%u accept=%u pending=%u saved=%u complete=%u failed=%u candidate=%u peerflags=%u peer_session=%u own_session=%u injected=%u\n",i,now,c[i].social.active,c[i].social.accepted,c[i].social.pending,c[i].social.saved,c[i].social.complete,c[i].social.failed,c[i].social.candidate,c[i].peers[1].social_flags,c[i].social.peer_session,c[i].social.session,injected);
  assert(now < (asymmetric?21000u:15000u) && dropped && reordered && (!resume_test||injected));
  if(speed){fprintf(stderr,"speed elapsed=%u frames=%u\n",now-1000,sent);assert(now-1000<5000);}
  assert(!strcmp(hr_friend_profile(1)->name, profiles[1].name));
  assert(!strcmp(b_hr_friend_profile(1)->name, profiles[0].name));
  for (unsigned y = 0; y < 104; y++)
    for (unsigned x = 0; x < 64; x++) {
      unsigned source = (y * 156 / 104) * 192 + (x * 96 / 64) * 2;
      if(!asymmetric)assert(!memcmp(hr_friend_profile(1)->portrait->data + (y * 64 + x) * 2,
                     pixels[1] + source, 2));
      assert(!memcmp(b_hr_friend_profile(1)->portrait->data + (y * 64 + x) * 2,
                     pixels[0] + source, 2));
    }
  assert(hr_cards_init(2) && b_hr_cards_init(2));
  assert(!strcmp(hr_friend_profile(1)->signature, profiles[1].signature));
  assert(!strcmp(b_hr_friend_profile(1)->signature, profiles[0].signature));
  printf("cards pair: both committed in %u ms; %u dropped, %u reordered\n",
         now - 1000, dropped, reordered);
}
