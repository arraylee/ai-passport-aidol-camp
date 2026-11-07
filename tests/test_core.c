#include "hr_core.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint8_t macs[7][6] = {{0},
                             {2, 0, 0, 0, 0, 1},
                             {2, 0, 0, 0, 0, 2},
                             {2, 0, 0, 0, 0, 3},
                             {2, 0, 0, 0, 0, 4},
                             {2, 0, 0, 0, 0, 5},
                             {2, 0, 0, 0, 0, 6}};
static hr_core_t c[7];
static void init(void) {
  for (int i = 1; i <= 6; i++)
    hr_init(&c[i], i, macs[i], 100 + i, 0);
}
static void pump(uint32_t now, unsigned drop) {
  for (int i = 1; i <= 6; i++)
    hr_tick(&c[i], now);
  for (int i = 1; i <= 6; i++) {
    hr_packet_t p;
    if (!hr_control(&c[i], &p, now))
      continue;
    uint8_t wire[HR_WIRE_MAX];
    size_t n = hr_pack(&p, wire);
    hr_packet_t q;
    assert(hr_unpack(&q, wire, n));
    for (int j = 1; j <= 6; j++)
      if (i != j && !(drop && (now / 10 + i * 3 + j) % drop == 0))
        hr_receive(&c[j], &q, macs[i], -65, now);
  }
}
static void run(uint32_t start, uint32_t end, unsigned drop) {
  for (uint32_t t = start; t < end; t += 10)
    pump(t, drop);
}
static void protocol(void) {
  hr_packet_t p = {.kind = HR_HELLO, .id = 3, .boot = 0x12345678, .seq = 123},
              q;
  uint8_t b[HR_WIRE_MAX];
  size_t n = hr_pack(&p, b);
  assert(n == 36);
  assert(hr_unpack(&q, b, n) && q.id == 3 && q.boot == 0x12345678);
  b[7] ^= 1;
  assert(!hr_unpack(&q, b, n));
  b[7] ^= 1;
  for (size_t i = 0; i < n; i++)
    assert(!hr_unpack(&q, b, i));
  p.kind = HR_VOICE;
  assert(hr_pack(&p, b) == 200);
  assert(hr_unpack(&q, b, 200));
  p.id = 7;
  hr_pack(&p, b);
  assert(!hr_unpack(&q, b, 200));
}
static void discovery(void) {
  init();
  run(0, 1000, 0);
  for (int i = 1; i <= 6; i++)
    for (int j = 1; j <= 6; j++)
      assert(hr_online(&c[i], j, 1000));
  assert(!hr_online(&c[2], 5, 4100));
  uint8_t other[6] = {2, 9, 9, 9, 9, 9};
  hr_packet_t p = {.kind = HR_HELLO, .id = 2, .boot = 999, .seq = 1};
  hr_receive(&c[2], &p, other, -60, 1000);
  assert(hr_conflict(&c[2], 2, 1000));
  hr_press(&c[2], 0, 1000);
  assert(!hr_can_talk(&c[2], 1000));
}
static void floor_and_audio(void) {
  init();
  run(0, 1000, 0);
  hr_press(&c[2], 3, 1000);
  hr_press(&c[4], 0, 1000);
  run(1000, 1600, 7);
  assert(hr_can_talk(&c[2], 1600));
  assert(!hr_can_talk(&c[4], 1600));
  assert(c[4].blocked);
  uint8_t audio[HR_AUDIO] = {0};
  hr_packet_t p;
  assert(hr_voice(&c[2], &p, audio, 1600));
  assert(hr_receive(&c[3], &p, macs[2], -60, 1600));
  assert(!hr_receive(&c[5], &p, macs[2], -60, 1600));
  assert(!hr_receive(&c[3], &p, macs[2], -60, 1601));
  p.token++;
  p.seq++;
  assert(!hr_receive(&c[3], &p, macs[2], -60, 1602));
  hr_release(&c[2], 1610);
  run(1610, 2200, 5);
  assert(!hr_can_talk(&c[2], 2200));
  assert(c[1].speaker == 0);
  assert(!hr_can_talk(&c[4], 2200));
  hr_release(&c[4], 2200);
  hr_press(&c[4], 0, 2210);
  run(2210, 2700, 0);
  assert(hr_can_talk(&c[4], 2700));
  assert(hr_voice(&c[4], &p, audio, 2700));
  for (int j = 1; j <= 6; j++)
    if (j != 4)
      assert(hr_receive(&c[j], &p, macs[4], -60, 2700));
  // Lost release must not lock the floor forever; no client transmission
  // without grants.
  hr_tick(&c[4], 3300);
  assert(!hr_can_talk(&c[4], 3300));
  hr_tick(&c[1], 3500);
  assert(c[1].speaker == 0);
}
static void delayed_and_restart(void) {
  init();
  run(0, 1000, 0);
  hr_press(&c[2], 0, 1000);
  run(1000, 1400, 0);
  hr_packet_t grant = {.kind = HR_GRANT,
                       .id = 1,
                       .boot = c[1].boot,
                       .seq = c[1].seq + 1,
                       .speaker = 2,
                       .target = 0,
                       .request = c[1].floor_request,
                       .epoch = c[1].epoch,
                       .token = c[1].token};
  hr_release(&c[2], 1400);
  hr_receive(&c[2], &grant, macs[1], -60, 1410);
  assert(!hr_can_talk(&c[2], 1410));
  hr_init(&c[1], 1, macs[1], 9876, 1500);
  run(1500, 2400, 0);
  assert(c[2].speaker == 0);
  hr_press(&c[2], 0, 2410);
  run(2410, 3000, 0);
  assert(hr_can_talk(&c[2], 3000));
  hr_receive(&c[2], &grant, macs[1], -60, 3001);
  assert(c[2].epoch == 9876);
}
static void signal(void) {
  init();
  hr_packet_t p = {.kind = HR_HELLO, .id = 2, .boot = 102};
  for (int t = 0; t < 900; t += 300) {
    p.seq++;
    hr_receive(&c[1], &p, macs[2], -80, t);
  }
  assert(c[1].peers[2].zone == 0);
  p.seq++;
  hr_receive(&c[1], &p, macs[2], -30, 900);
  assert(c[1].peers[2].zone == 0);
  for (int t = 1200; t < 6000; t += 300) {
    p.seq++;
    hr_receive(&c[1], &p, macs[2], -48, t);
  }
  assert(c[1].peers[2].zone == 2);
  assert(!hr_online(&c[1], 2, 9000));
}
static void codec_config(void) {
  int16_t pcm[320], decoded[320];
  uint8_t data[164];
  for (int i = 0; i < 320; i++)
    pcm[i] = (int16_t)(sin(i * 0.1) * 8000);
  hr_adpcm_encode(pcm, data);
  assert(hr_adpcm_decode(data, decoded));
  double err = 0;
  for (int i = 0; i < 320; i++)
    err += fabs((double)pcm[i] - decoded[i]);
  assert(err / 320 < 1200);
  data[2] = 255;
  assert(!hr_adpcm_decode(data, decoded));
  uint8_t cfg[12], id, v;
  hr_config_encode(6, 70, cfg);
  assert(hr_config_decode(cfg, &id, &v) && id == 6 && v == 70);
  cfg[7] ^= 1;
  assert(!hr_config_decode(cfg, &id, &v));
  hr_config_encode(0, 70, cfg);
  assert(!hr_config_decode(cfg, &id, &v));
}
int main(void) {
  protocol();
  discovery();
  floor_and_audio();
  delayed_and_restart();
  signal();
  codec_config();
  puts("PASS protocol, discovery, conflict, floor, targeted/all audio, "
       "timeout, restart, RSSI, ADPCM, config");
}
