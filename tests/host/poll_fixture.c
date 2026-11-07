#include "hr_core.h"
#include "hr_identity.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  hr_packet_t p;
  unsigned char target[6],speaker[6];
  unsigned char mac[6];
  signed char rssi;
  unsigned at;
} rx_t;
typedef struct {
  unsigned char data[HR_AUDIO];
  unsigned gate, at;
} audio_t;
static bool radio_ready = true, audio_ready;
static atomic_bool sending;
static atomic_uint drops, tx_gate, rx_gate;
static unsigned send_at, last_audio, send_count;
static int rx_queue = 1, tx_audio = 2, rx_audio = 3;
static rx_t queued_rx;
static audio_t queued_tx;
static bool has_rx, has_tx;
static unsigned fake_now = 1000, dequeue_now = 1002;
static unsigned ms(void) { return fake_now; }
static void profile_received(hr_core_t *c, const rx_t *r) {
  (void)c;
  (void)r;
}
static bool profile_packet(hr_core_t *c, hr_packet_t *p, uint32_t now) {
  (void)c;
  (void)p;
  (void)now;
  return false;
}
#define pdTRUE 1
static int xQueueReceive(int q, void *out, unsigned wait) {
  (void)wait;
  if (q == rx_queue && has_rx) {
    has_rx = false;
    fake_now = dequeue_now;
    memcpy(out, &queued_rx, sizeof queued_rx);
    return pdTRUE;
  }
  if (q == tx_audio && has_tx) {
    has_tx = false;
    fake_now = dequeue_now;
    memcpy(out, &queued_tx, sizeof queued_tx);
    return pdTRUE;
  }
  return 0;
}
static int xQueueSend(int q, const void *p, unsigned wait) {
  (void)q;
  (void)p;
  (void)wait;
  return pdTRUE;
}
static void hr_audio_gate(hr_core_t *c, unsigned now, bool enabled) {
  (void)c;
  (void)now;
  (void)enabled;
}
static void send_packet(const hr_core_t *c,const hr_packet_t *p) {
  (void)c;
  (void)p;
  send_count++;
}

static bool hr_ble_upload_busy(void) { return false; }
static bool hr_ble_connected(void) { return false; }
static void hr_cards_receive(hr_core_t *c, const hr_packet_t *p, uint32_t now) { (void)c; (void)p; (void)now; }
static bool hr_cards_send(hr_core_t *c, hr_packet_t *p, uint32_t now) { (void)c; (void)p; (void)now; return false; }
static void hr_preview_receive(const hr_core_t *c,const hr_packet_t *p,uint32_t now) {(void)c;(void)p;(void)now;}
static bool hr_preview_send(hr_core_t *c,hr_packet_t *p,uint32_t now) {(void)c;(void)p;(void)now;return false;}
static void hr_profile_clear(unsigned id){(void)id;}
static void hr_profile_remote(unsigned id,const char*n,const char*i,bool a){(void)id;(void)n;(void)i;(void)a;}
#define ESP_LOGI(...) ((void)0)
#include "production_poll.inc"
int main(void) {
  (void)ms();
  hr_core_t c;
  unsigned char mac[6] = {2, 0, 0, 0, 0, 1};
  hr_init(&c, 1, mac, 101, 0);
  c.identity_mode=true;
  c.next_hello = c.next_control = 2000;
  queued_rx = (rx_t){.p = {.kind = HR_HELLO, .id = 2, .boot = 102, .seq = 1},
                     .mac = {2, 0, 0, 0, 0, 2},
                     .rssi = -60,
                     .at = 1001};
  has_rx = true;
  hr_radio_poll(&c, 1000);
  printf("ACTUAL hr_radio_poll RX: cached now1000, callback at1001, "
         "peer_seen=%d drops=%u (expected peer_seen1/drops0)\n",
         c.peers[2].seen, atomic_load(&drops));
  assert(c.peers[2].seen && atomic_load(&drops) == 0);
  hr_init(&c, 1, mac, 101, 0);
  c.identity_mode=true;
  c.next_hello = c.next_control = 2000;
  c.held = true;
  c.speaker = 1;
  c.request = c.floor_request = 1;
  c.press_at = 900;
  c.floor_at = c.floor_client_at = 1000;
  assert(hr_can_talk(&c, 1000));
  atomic_store(&drops, 0);
  atomic_store(&tx_gate, 1);
  audio_ready = true;
  queued_tx = (audio_t){.gate = 1, .at = 1001};
  has_tx = true;
  send_count = 0;
  hr_radio_poll(&c, 1000);
  printf("ACTUAL hr_radio_poll TX: cached now1000, capture at1001, sends=%u "
         "drops=%u (expected sends1/drops0)\n",
         send_count, atomic_load(&drops));
  assert(send_count == 1 && atomic_load(&drops) == 0);
  atomic_store(&drops, 0);
  has_rx = true;
  queued_rx.at = 800;
  queued_rx.p.seq++;
  hr_radio_poll(&c, 1000);
  assert(atomic_load(&drops) == 1);
  atomic_store(&drops, 0);
  has_tx = true;
  queued_tx.at = 900;
  send_count = 0;
  hr_radio_poll(&c, 1000);
  assert(send_count == 0 && atomic_load(&drops) == 1);
  fake_now = 0xfffffffeu;
  dequeue_now = 2;
  audio_ready = false;
  hr_init(&c, 1, mac, 101, fake_now);
  c.identity_mode=true;
  c.next_hello = c.next_control = 20;
  atomic_store(&drops, 0);
  queued_rx = (rx_t){.p = {.kind = HR_HELLO, .id = 2, .boot = 102, .seq = 1},
                     .mac = {2, 0, 0, 0, 0, 2},
                     .rssi = -60,
                     .at = 1};
  has_rx = true;
  hr_radio_poll(&c, 0xfffffffeu);
  assert(c.peers[2].seen && atomic_load(&drops) == 0);
  puts("PASS asynchronous RX/TX clock race and stale-frame rejection against "
       "actual poll body");
}
