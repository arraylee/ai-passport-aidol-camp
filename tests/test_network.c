#include "hr_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static hr_core_t nodes[7];
static uint8_t mac[7][6] = {{0},
                            {2, 0, 0, 0, 0, 1},
                            {2, 0, 0, 0, 0, 2},
                            {2, 0, 0, 0, 0, 3},
                            {2, 0, 0, 0, 0, 4},
                            {2, 0, 0, 0, 0, 5},
                            {2, 0, 0, 0, 0, 6}};
static uint32_t rng = 777;
static uint32_t random32(void) {
  rng = rng * 1664525 + 1013904223;
  return rng;
}
static void init_node(unsigned id, unsigned boot, unsigned now) {
  hr_init(&nodes[id], id, mac[id], boot, now);
#ifdef HR_TEST_ROOMS
  hr_room_enable(&nodes[id], now); hr_room_join(&nodes[id], true, now);
#endif
}
static void initialize(void) {
  for (int i = 1; i <= 6; i++)
    init_node(i, i + 100, 0);
}
static void step(uint32_t now, unsigned loss, bool host_offline) {
  for (int i = 1; i <= 6; i++)
    hr_tick(&nodes[i], now);
  for (int i = 1; i <= 6; i++) {
    if (host_offline && i == 1)
      continue;
    hr_packet_t p;
    if (!hr_control(&nodes[i], &p, now))
      continue;
    for (int j = 1; j <= 6; j++)
      if (j != i && !(host_offline && j == 1) && random32() % 100 >= loss)
        hr_receive(&nodes[j], &p, mac[i], -60, now);
  }
  unsigned speakers = 0;
  for (int i = 1; i <= 6; i++)
    if (hr_can_talk(&nodes[i], now))
      speakers++;
  assert(speakers <= 1);
}
static void asymmetric_loss(void) {
  initialize();
  for (unsigned t = 0; t < 1000; t += 10)
    step(t, 0, false);
  hr_press(&nodes[2], 0, 1000);
  for (unsigned t = 1000; t < 1400; t += 10)
    step(t, 0, false);
  for (unsigned t = 1400; t < 3500; t += 10) {
    for (int i = 1; i <= 6; i++)
      hr_tick(&nodes[i], t);
    if (t == 2150)
      hr_press(&nodes[3], 0, t);
    for (int i = 1; i <= 6; i++) {
      hr_packet_t p;
      if (!hr_control(&nodes[i], &p, t))
        continue;
      for (int j = 1; j <= 6; j++) {
        if (i == j || (i == 2 && j == 1))
          continue;
        if (i == 1 && j == 2 && p.kind == HR_GRANT && p.speaker != 2)
          continue;
        hr_receive(&nodes[j], &p, mac[i], -60, t);
      }
    }
    assert(!(hr_can_talk(&nodes[2], t) && hr_can_talk(&nodes[3], t)));
  }
  assert(!hr_can_talk(&nodes[2], 3500));
  assert(hr_can_talk(&nodes[3], 3500));
}
static void coordinator_restart(void) {
  initialize();
  for (unsigned t = 0; t < 1000; t += 10)
    step(t, 0, false);
  hr_press(&nodes[2], 0, 1000);
  for (unsigned t = 1000; t < 1400; t += 10)
    step(t, 0, false);
  assert(hr_can_talk(&nodes[2], 1400));
  init_node(1, 5678, 1400);
  for (unsigned t = 1400; t < 3000; t += 10) {
    for (int i = 1; i <= 6; i++)
      hr_tick(&nodes[i], t);
    if (t == 1500)
      hr_press(&nodes[3], 0, t);
    for (int i = 1; i <= 6; i++) {
      hr_packet_t p;
      if (!hr_control(&nodes[i], &p, t))
        continue;
      for (int j = 1; j <= 6; j++)
        if (i != j && !(i == 1 && j == 2))
          hr_receive(&nodes[j], &p, mac[i], -60, t);
    }
    assert(!(hr_can_talk(&nodes[2], t) && hr_can_talk(&nodes[3], t)));
  }
  assert(hr_can_talk(&nodes[3], 3000));
}
static void recipients(void) {
  for (unsigned target = 0; target <= 3; target += 3) {
    initialize();
    for (unsigned t = 0; t < 1000; t += 10) step(t, 0, false);
    hr_press(&nodes[2], target, 1000);
    for (unsigned t = 1000; t < 1400; t += 10) step(t, 0, false);
    hr_packet_t voice; uint8_t data[HR_AUDIO] = {0};
    assert(hr_voice(&nodes[2], &voice, data, 1400));
    for (unsigned id = 1; id <= 6; id++) if (id != 2)
      assert(hr_receive(&nodes[id], &voice, mac[2], -40, 1400) == (target == 0 || id == target));
#ifdef HR_TEST_ROOMS
    hr_room_join(&nodes[4], false, 1401);
    assert(hr_voice(&nodes[2], &voice, data, 1402));
    assert(!hr_receive(&nodes[4], &voice, mac[2], -40, 1402));
#endif
  }
}
static void gaming_coordinator(void) {
  initialize();
  for(unsigned t=0;t<1000;t+=10)step(t,0,false);
  nodes[1].game_busy=true;nodes[1].next_hello=1000;
  for(unsigned t=1000;t<1200;t+=10)step(t,0,false);
  hr_press(&nodes[2],0,1200);
  for(unsigned t=1200;t<1700;t+=10)step(t,0,false);
  assert(hr_can_talk(&nodes[2],1700)); // Host still arbitrates while gaming.
  uint8_t pcm[HR_AUDIO]={0};hr_packet_t voice;
  assert(hr_voice(&nodes[2],&voice,pcm,1700));
  assert(!hr_receive(&nodes[1],&voice,mac[2],-40,1700));
  assert(hr_receive(&nodes[3],&voice,mac[2],-40,1700));
  nodes[3].game_busy=true;
  assert(hr_voice(&nodes[2],&voice,pcm,1701));
  assert(!hr_receive(&nodes[3],&voice,mac[2],-40,1701));
  hr_press(&nodes[1],0,1702);assert(nodes[1].blocked&&!hr_can_talk(&nodes[1],1702));
  hr_release(&nodes[1],1703);
  hr_release(&nodes[2],1703);
  for(unsigned t=1710;t<2500;t+=10)step(t,0,false);
  hr_press(&nodes[2],3,2500);assert(nodes[2].blocked);
}
int main(void) {
  gaming_coordinator();
  recipients();
  asymmetric_loss();
  coordinator_restart();
  initialize();
  for (unsigned t = 0; t < 2000; t += 10)
    step(t, 20, false);
  unsigned talks = 0;
  // Deterministic 10-minute six-node contention trace, 20% independent control
  // loss.
  for (unsigned t = 2000; t < 602000; t += 10) {
    for (int i = 1; i <= 6; i++) {
      if (t % 1000 == 0 && random32() % 3 == 0)
        hr_press(&nodes[i], 0, t);
      if (nodes[i].held &&
          (uint32_t)(t - nodes[i].press_at) > 400 + random32() % 1300)
        hr_release(&nodes[i], t);
      if (hr_can_talk(&nodes[i], t))
        talks++;
    }
    step(t, 20, false);
  }
  assert(talks > 1000);
  for (int i = 1; i <= 6; i++)
    hr_release(&nodes[i], 602000);
  for (unsigned t = 602000; t < 604000; t += 10)
    step(t, 20, false);
  assert(nodes[1].speaker == 0);
  hr_press(&nodes[3], 0, 604000);
  for (unsigned t = 604000; t < 605000; t += 10)
    step(t, 0, false);
  assert(hr_can_talk(&nodes[3], 605000));
  for (unsigned t = 605000; t < 609000; t += 10)
    step(t, 0, true);
  assert(!hr_can_talk(&nodes[3], 609000));
  assert(!hr_online(&nodes[2], 1, 609000));
  assert(hr_online(&nodes[2], 5, 609000));
  init_node(1, 5555, 609000);
  hr_release(&nodes[3], 609000);
  for (unsigned t = 609000; t < 611000; t += 10)
    step(t, 0, false);
  hr_press(&nodes[3], 0, 611000);
  for (unsigned t = 611000; t < 612000; t += 10)
    step(t, 0, false);
  assert(hr_can_talk(&nodes[3], 612000));
  // Continuous press has a hard cap and cannot auto-restart while still held.
  for (unsigned t = 612000; t < 645000; t += 10)
    step(t, 0, false);
  assert(!hr_can_talk(&nodes[3], 645000) && nodes[3].blocked &&
         nodes[1].speaker == 0);
  puts("PASS 10-minute simulated contention with 20% loss; host outage/rejoin; "
       "30-second PTT cap");
}
