#include "hr_social.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static hr_core_t a, b;
static uint32_t now = 1000;
static void hello(hr_core_t *s, hr_core_t *d) {
  hr_packet_t p;
  s->next_hello = now;
  assert(hr_control(s, &p, now));
  if(p.kind!=HR_HELLO)assert(hr_control(s,&p,now));
  assert(p.kind == HR_HELLO);
  hr_receive(d, &p, s->mac, -40, now);
}
static void exchange(void) {
  now += 140;
  hello(&a, &b);
  hello(&b, &a);
  hr_social_tick(&a, now);
  hr_social_tick(&b, now);
}
int main(void) {
  uint8_t ma[6] = {1}, mb[6] = {2};
  hr_init(&a, 1, ma, 11, 0);
  hr_init(&b, 2, mb, 22, 0);
  hr_social_enter(&a);
  hr_social_enter(&b);
  exchange();
  exchange();
  assert(a.social.candidate == 2 && b.social.candidate == 1);
  hr_social_confirm(&a, now);
  exchange();
  assert(!a.social.pending && !b.social.pending);
  hr_social_confirm(&b, now);
  exchange();
  exchange();
  assert(a.social.pending && b.social.pending);
  hr_social_saved(&a, false);
  assert(a.social.failed && !a.social.complete && a.social.friends == 0);
  hr_social_saved(&a, true);
  hr_social_saved(&b, true);
  exchange();
  exchange();
  assert(a.social.complete && b.social.complete);
  assert(a.social.friends == (1 << 2));
  uint8_t blob[12], mask = 0;
  hr_friends_encode(1, a.social.friends, blob);
  assert(hr_friends_decode(1, blob, &mask) && mask == (1 << 2));
  assert(!hr_friends_decode(2, blob, &mask));
  blob[5] ^= 4;
  assert(!hr_friends_decode(1, blob, &mask));
  hr_packet_t late;
  a.next_hello = now;
  assert(hr_control(&a, &late, now));
  hr_social_leave(&b);
  hr_social_enter(&b);
  assert(!b.social.accepted && !b.social.complete);
  hr_receive(&b, &late, a.mac, -40, now);
  hr_social_tick(&b, now);
  assert(!b.social.pending);
  exchange();
  hr_social_confirm(&b, now);
  exchange();
  assert(!b.social.pending); // old A confirmation binds old B session
  hr_social_leave(&a);
  exchange();
  assert(!b.social.accepted);
  hr_social_enter(&a);
  exchange();
  exchange();
  hr_social_confirm(&a, now);
  now += 4000;
  hr_social_tick(&a, now);
  assert(!a.social.accepted && !a.social.pending);
  hr_init(&a, 1, ma, 111, now);
  hr_init(&b, 2, mb, 222, now);
  hr_social_enter(&a);
  hr_social_enter(&b);
  for (unsigned k = 0; k < 12; k++) {
    now += 140;
    if (k % 3 != 0)
      hello(&a, &b);
    if (k % 3 != 1)
      hello(&b, &a);
    hr_social_tick(&a, now);
    hr_social_tick(&b, now);
  }
  hr_social_confirm(&a, now);
  hr_social_confirm(&b, now);
  for (unsigned k = 0; k < 20; k++) {
    now += 140;
    if (k % 3 != 0)
      hello(&a, &b);
    if (k % 3 != 1)
      hello(&b, &a);
    hr_social_tick(&a, now);
    hr_social_tick(&b, now);
    if (a.social.pending)
      hr_social_saved(&a, true);
    if (b.social.pending)
      hr_social_saved(&b, true);
  }
  assert(a.social.complete && b.social.complete);
  // A completed friendship must not lock the picker forever.
  a.peers[3] = a.peers[2];
  uint32_t prior_session = a.social.session;
  uint8_t saved_friends = a.social.friends;
  hr_social_select(&a, 1, now);
  assert(a.social.candidate == 3 && a.social.session != prior_session);
  assert(!a.social.accepted && !a.social.saved && !a.social.complete);
  assert(a.social.friends == saved_friends);
  puts(
      "PASS social mutual accept/save failure/late session/cancel/offline/CRC");
}
