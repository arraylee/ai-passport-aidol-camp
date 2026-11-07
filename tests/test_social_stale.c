#include "hr_social.h"
#include <assert.h>
#include <stdio.h>
static void hello(hr_core_t *a, hr_core_t *b, unsigned t) {
  hr_packet_t p;
  a->next_hello = t;
  assert(hr_control(a, &p, t));
 if(p.kind!=HR_HELLO)assert(hr_control(a, &p, t));
 assert(p.kind==HR_HELLO);
  hr_receive(b, &p, a->mac, -40, t);
}
int main(void) {
  hr_core_t a, b;
  unsigned char ma[6] = {1}, mb[6] = {2};
  hr_init(&a, 1, ma, 11, 0);
  hr_init(&b, 2, mb, 22, 0);
  hr_social_enter(&a);
  hr_social_enter(&b);
  hello(&a, &b, 1000);
  hello(&b, &a, 1000);
  hr_social_tick(&a, 1000);
  hr_social_tick(&b, 1000);
  hello(&a, &b, 1140);
  hello(&b, &a, 1140);
  hr_social_confirm(&a, 1140);
  hello(&a, &b, 1150);
  assert(!b.social.accepted);
  hr_social_leave(&a);
  for (unsigned t = 1160; t < 7000; t += 10) {
    hr_packet_t p;
    if (hr_control(&a, &p, t) && p.kind != HR_HELLO)
      hr_receive(&b, &p, a.mac, -40, t);
    hr_social_tick(&b, t);
  }
  printf("At7000ms after A canceled at1150: B candidate=%u peer_online=%u "
         "stale_social_flags=%u\n",
         b.social.candidate, hr_online(&b, 1, 7000), b.peers[1].social_flags);
  hr_social_confirm(&b, 7000);
  hr_social_tick(&b, 7000);
  printf("B confirms stale A: pending_save=%u A.active=%u\n", b.social.pending,
         a.social.active);
  assert(!b.social.pending && !b.social.accepted && !a.social.active);
}
