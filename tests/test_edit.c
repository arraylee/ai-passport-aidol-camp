#include "hr_edit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned config(hr_edit_t *p, const char *s) {
  return hr_edit_config(p, (const uint8_t *)s, strlen(s));
}
int main(void) {
  hr_edit_t p = {.id = 1, .revision = 3};
  strcpy(p.name, "喵喵");
  strcpy(p.intro, "简单开发，人人可造");
  assert(config(&p, "{\"nickname\":\"直播小猫\",\"intro\":\"今天一起创造\","
                    "\"img_mode\":\"avatar\"}") == 1);
  assert(!strcmp(p.name, "直播小猫") && !strcmp(p.intro, "今天一起创造"));
  assert(config(&p, "{\"nickname\":\"直播小猫\",\"title\":\"开发者\",\"intro\":"
                    "\"今天一起创造\"}") == 1);
  assert(!strcmp(p.title, "开发者"));
  hr_edit_t old = p;
  assert(config(&p, "{\"nickname\":42}") != 1 && !memcmp(&p, &old, sizeof p));
  assert(config(&p, "{\"nickname\":\"坏\\u0000名\"}") != 1);
  assert(config(&p, "{\"nickname\":\"A\",\"nickname\":\"B\"}") != 1);
  assert(config(&p, "{\"img_mode\":\"fullscreen\"}") == 1);
  assert(!memcmp(&p, &old, sizeof p));
  assert(config(&p, "{\"time\":1234567890}") == 1);
  assert(config(&p, "{\"nickname\":\"\"}") != 1);
  assert(!hr_utf8_valid("\xf0\x80", 2));
  uint8_t b[HR_EDIT_HEADER];
  hr_edit_t q;
  hr_edit_encode(&p, b);
  assert(hr_edit_decode(&q, b));
  assert(q.id == 1 && !strcmp(q.name, p.name));
  b[99] ^= 1;
  assert(!hr_edit_decode(&q, b));
  hr_frame_t f = {0};
  uint8_t frame[] = {1, 1, 2, 0, '{', '}'};
  assert(hr_frame_push(&f, frame, 2, 0) == 0);
  assert(hr_frame_push(&f, frame + 2, 4, 10) == 1 && f.wanted == 6);
  f = (hr_frame_t){0};
  assert(hr_frame_push(&f, frame, 3, 0) == 0);
  assert(hr_frame_push(&f, frame + 3, 3, 2001) == 0x11 && f.used == 0);
  f = (hr_frame_t){0};
  uint8_t huge[] = {1, 2, 1, 1};
  assert(hr_frame_push(&f, huge, 4, 0) == 0x15);
  f = (hr_frame_t){0};
  assert(hr_frame_push(&f, frame, 2, 0xfffffffe) == 0);
  assert(hr_frame_push(&f, frame + 2, 4, 2) == 1);
  uint8_t wire[164];
  hr_edit_wire_encode(&p, wire);
  assert(hr_edit_wire_decode(&q, wire) && !strcmp(q.name, p.name));
  /* Shipped Miaomiao signature must survive radio, durable storage and miniapp. */
  strcpy(p.intro, "FoloToy\n简单开放\n人人可造");
  hr_edit_wire_encode(&p, wire);
  assert(hr_edit_wire_decode(&q, wire) && !strcmp(q.intro, p.intro));
  hr_edit_encode(&p, b);
  assert(hr_edit_decode(&q, b) && !strcmp(q.intro, p.intro));
  assert(config(&q, "{\"intro\":\"FoloToy\\n简单开放\\n人人可造\"}") == 1);
  assert(!strcmp(q.intro, p.intro));
  assert(config(&q, "{\"nickname\":\"坏\\n名字\"}") != 1);
  assert(config(&q, "{\"intro\":\"bad\\u0001text\"}") != 1);
  assert(config(&q, "{\"intro\":\"bad\\ttext\"}") != 1);
  memset(wire + 2, 65, 49);
  assert(!hr_edit_wire_decode(&q, wire));
  puts("PASS miniapp config UTF8/aliases/reject/atomic header/frame timeout "
       "and wrap");
}
