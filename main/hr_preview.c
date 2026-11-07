#include "hr_preview.h"
#include <string.h>
enum { WIDTH = 32, HEIGHT = 52, BYTES = WIDTH * HEIGHT * 2 };
static struct {
  bool active, ready, response;
  uint8_t owner, peer;
  uint32_t boot, peer_boot, session, peer_session, crc, next, refresh;
  unsigned used, response_offset;
  uint8_t pixels[BYTES];
} preview;
static const lv_image_dsc_t image = {
  .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_RGB565,
             .w=WIDTH, .h=HEIGHT, .stride=WIDTH*2},
  .data_size=BYTES, .data=preview.pixels
};
static bool due(uint32_t now, uint32_t at) { return (int32_t)(now-at)>=0; }
void hr_preview_reset(void) {
  /* Pixel bytes can remain allocated: unavailable unless a full CRC passes. */
  preview.active=preview.ready=preview.response=false;
  preview.used=0;
}
static bool matches(const hr_core_t *c, uint32_t now) {
  unsigned id=preview.peer;
  if(!preview.active || !id || id>HR_PLAYERS) return false;
  const hr_peer_t *p=&c->peers[id];
  return c->id==preview.owner && c->boot==preview.boot &&
    c->hardware_ok && !c->held && !c->speaker && c->social.active &&
    c->social.locked_candidate==id && c->social.candidate==id &&
    c->social.session==preview.session && hr_online(c,id,now) &&
    !hr_conflict(c,id,now) && !hr_conflict(c,c->id,now) &&
    p->boot==preview.peer_boot && p->social_session==preview.peer_session &&
    (p->social_flags&2) && (uint32_t)(now-p->hello_at)<800;
}
void hr_preview_tick(const hr_core_t *c, bool enabled, uint32_t now) {
  unsigned id=c->social.locked_candidate;
  if(!enabled || !c->hardware_ok || !c->social.active || !id ||
     id>HR_PLAYERS || c->social.candidate!=id || id==c->id ||
     c->held || c->speaker || !hr_online(c,id,now) ||
     hr_conflict(c,id,now) || hr_conflict(c,c->id,now) ||
     !(c->peers[id].social_flags&2) || !c->peers[id].social_session ||
     (uint32_t)(now-c->peers[id].hello_at)>=800) {
    hr_preview_reset(); return;
  }
  if(matches(c,now)) return;
  hr_preview_reset();
  preview.active=true; preview.owner=c->id; preview.peer=id;
  preview.boot=c->boot; preview.peer_boot=c->peers[id].boot;
  preview.session=c->social.session;
  preview.peer_session=c->peers[id].social_session;
  preview.crc=0; preview.next=preview.refresh=now;
}
static const uint8_t *local_pixels(unsigned id) {
  const lv_image_dsc_t *p=hr_profile(id)->portrait;
  return p && p->data && p->header.cf==LV_COLOR_FORMAT_RGB565 &&
    p->header.w==96 && p->header.h==156 && p->header.stride==192 &&
    p->data_size>=29952 ? p->data : NULL;
}
static uint8_t sample(const uint8_t *pixels, unsigned at) {
  unsigned p=at/2;
  return pixels[((p/WIDTH)*3*96+(p%WIDTH)*3)*2+at%2];
}
static uint32_t local_crc(const uint8_t *pixels) {
  uint32_t crc=~0u;
  for(unsigned at=0;at<BYTES;at++) {
    crc^=sample(pixels,at);
    for(unsigned b=0;b<8;b++) crc=(crc>>1)^(0xedb88320u & -(crc&1));
  }
  return ~crc;
}
void hr_preview_receive(const hr_core_t *c, const hr_packet_t *p, uint32_t now) {
  if(!matches(c,now) || p->id!=preview.peer || p->target!=c->id ||
     p->boot!=preview.peer_boot || p->request!=preview.peer_session ||
     p->epoch!=preview.session) return;
  if(p->kind==HR_PREVIEW_REQUEST) {
    if(p->flags<BYTES && p->flags%HR_AUDIO==0) {
      preview.response=true; preview.response_offset=p->flags;
    }
    return;
  }
  if(p->kind!=HR_PREVIEW_DATA) return;
  if(p->flags==UINT32_MAX) {
    preview.ready=false;preview.used=0;preview.next=now+2000;return;
  }
  if(p->flags>=BYTES || p->flags%HR_AUDIO) return;
  if(preview.ready && p->token==preview.crc) {
    preview.refresh=now+2000;return;
  }
  if(p->token!=preview.crc) {
    preview.ready=false;preview.used=0;preview.crc=p->token;
  }
  if(p->flags!=preview.used) { preview.next=now;return; }
  preview.ready=false;
  unsigned n=BYTES-preview.used;if(n>HR_AUDIO)n=HR_AUDIO;
  memcpy(preview.pixels+preview.used,p->audio,n);preview.used+=n;
  preview.next=now;
  if(preview.used==BYTES) {
    preview.ready=hr_crc(preview.pixels,BYTES)==preview.crc;
    if(!preview.ready)preview.used=0;
    preview.refresh=now+2000;
  }
}
bool hr_preview_send(hr_core_t *c, hr_packet_t *p, uint32_t now) {
  if(!matches(c,now) || c->social.pending || c->social.saved) return false;
  memset(p,0,sizeof *p);
  if(preview.response) {
    const uint8_t *pixels=local_pixels(c->id);
    p->kind=HR_PREVIEW_DATA;p->flags=UINT32_MAX;
    if(pixels) {
      p->flags=preview.response_offset;p->token=local_crc(pixels);
      for(unsigned i=0;i<HR_AUDIO && p->flags+i<BYTES;i++)
        p->audio[i]=sample(pixels,p->flags+i);
    }
    preview.response=false;
  } else {
    if(!due(now,preview.ready ? preview.refresh : preview.next))return false;
    p->kind=HR_PREVIEW_REQUEST;p->flags=preview.ready ? 0 : preview.used;
    preview.next=now+120;preview.refresh=now+2000;
  }
  p->id=c->id;p->target=preview.peer;p->boot=c->boot;p->seq=++c->seq;
  p->request=preview.session;p->epoch=preview.peer_session;
  return true;
}
const lv_image_dsc_t *hr_preview_image(void) {
  return preview.active && preview.ready ? &image : NULL;
}
