#include "hr_core.h"
#include <assert.h>
#include <stdio.h>
static hr_core_t a, b;
static void hello(hr_core_t *src, hr_core_t *dst, unsigned now) {
  hr_packet_t p;
  src->next_hello = now;
  assert(hr_control(src, &p, now));
 if(p.kind!=HR_HELLO)assert(hr_control(src, &p, now));
 assert(p.kind==HR_HELLO);
  hr_receive(dst, &p, src->mac, -40, now);
}
int main(void) {
  uint8_t ma[6] = {1}, mb[6] = {2};
  hr_init(&a, 1, ma, 11, 0);
  hr_init(&b, 2, mb, 22, 0);
  hr_room_enable(&a, 0);
  hr_room_enable(&b, 0);
  hello(&a, &b, 100);
  hello(&b, &a, 100);
  assert(!hr_room_member(&a, 2, 100));
  hr_press(&b, 0, 1000);
  assert(b.blocked);
  hr_release(&b, 1000);
  hr_room_join(&a, true, 1100);
  hr_room_join(&b, true, 1100);
  hello(&a, &b, 1200);
  hello(&b, &a, 1200);
  assert(hr_room_member(&a, 2, 1200));
  hr_press(&b, 0, 2000);
  assert(!b.blocked);
  hr_packet_t q = {.kind = HR_PROFILE, .id = 2, .boot = b.boot, .seq = ++b.seq};
  hr_receive(&a, &q, b.mac, -40, 4300);
  assert(hr_online(&a, 2, 4300));
  assert(!hr_room_member(&a, 2, 4300));
  hello(&b, &a, 4400);
  assert(hr_room_member(&a, 2, 4400));
  hr_room_join(&b, false, 4500);
  assert(!b.held && !b.speaker);
  hello(&b, &a, 4500);
  assert(!hr_room_member(&a, 2, 4500));
  hr_packet_t request = {.kind = HR_REQUEST,
                         .id = 2,
                         .boot = b.boot,
                         .seq = ++b.seq,
                         .request = 3};
  hr_receive(&a, &request, b.mac, -40, 4600);
  assert(!a.speaker);
  for (unsigned kind = HR_HELLO; kind <= HR_KIND_MAX; kind++) {
    uint8_t wire[200];
    hr_packet_t x = {.id = 1, .boot = 1, .kind = kind}, y;
    size_t n = hr_pack(&x, wire);
    assert(hr_unpack(&y, wire, n));
    assert(n == ((kind == HR_VOICE || kind == HR_PROFILE ||
                  kind == HR_CARD_META || kind == HR_CARD_DATA || kind == HR_PREVIEW_DATA)
                     ? 200
                     : 36));
  }
  puts("PASS explicit join/leave, coordinator membership, HELLO expiry and "
       "wire kinds");
}
