#include "hr_preview.h"
#include "hr_identity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void b_hr_preview_tick(const hr_core_t *,bool,uint32_t);
void b_hr_preview_reset(void);
bool b_hr_preview_send(hr_core_t *,hr_packet_t *,uint32_t);
void b_hr_preview_receive(const hr_core_t *,const hr_packet_t *,uint32_t);
const lv_image_dsc_t *b_hr_preview_image(void);
static uint8_t pixels[2][29952];
static lv_image_dsc_t portraits[2];
static hr_profile_t profiles[2];
const hr_profile_t *hr_profile(unsigned id) {(void)id;return &profiles[0];}
const hr_profile_t *b_hr_profile(unsigned id) {(void)id;return &profiles[1];}
static unsigned dropped, sent;
static void network(hr_core_t c[2],unsigned from,hr_packet_t *p,uint32_t now) {
  uint8_t wire[HR_IDENTITY_WIRE_MAX];hr_identity_frame_t f;
  size_t n=hr_identity_pack(&c[from],p,wire);assert(n==(p->kind==HR_PREVIEW_DATA?212u:48u));
  assert(hr_identity_unpack(&f,wire,n));assert(hr_identity_route(&c[!from],&f,c[from].mac,now));
  hr_packet_t decoded=f.p;
  if(++sent%7==0) {dropped++;return;}
  if(from)hr_preview_receive(&c[0],&decoded,now);
  else b_hr_preview_receive(&c[1],&decoded,now);
}
static void loop(hr_core_t c[2],unsigned start,unsigned end) {
  for(unsigned now=start;now<end;now+=12) {
    for(unsigned i=0;i<2;i++)c[i].peers[1].last=c[i].peers[1].hello_at=now;
    hr_preview_tick(&c[0],true,now);b_hr_preview_tick(&c[1],true,now);
    hr_packet_t p;
    if(hr_preview_send(&c[0],&p,now))network(c,0,&p,now);
    if(b_hr_preview_send(&c[1],&p,now))network(c,1,&p,now);
  }
}
int main(void) {
  hr_core_t c[2]={0};
  for(unsigned i=0;i<2;i++) {
    for(unsigned at=0;at<29952;at++)pixels[i][at]=(at*7+i*19)%251;
    portraits[i]=(lv_image_dsc_t){.header={.cf=LV_COLOR_FORMAT_RGB565,.w=96,.h=156,.stride=192},.data_size=29952,.data=pixels[i]};
    profiles[i].portrait=&portraits[i];
    c[i].id=2;uint8_t mac[6]={0x4c,0x11,0xae,0,0,(uint8_t)(10+i)};memcpy(c[i].mac,mac,6);hr_identity_enable(&c[i]);c[i].boot=100+i;c[i].hardware_ok=true;
    c[i].social=(hr_social_t){.active=true,.session=10+i,.candidate=1,.locked_candidate=1};
    c[i].peers[1]=(hr_peer_t){.seen=true,.boot=101-i,.social_session=11-i,.social_flags=2};
  }
  for(unsigned i=0;i<2;i++)memcpy(c[i].peers[1].mac,c[!i].mac,6);
  // Loss/retries before either party confirms; no friendship state changes.
  loop(c,1000,8000);
  assert(hr_preview_image() && b_hr_preview_image() && dropped);
  const uint8_t *got=hr_preview_image()->data;
  for(unsigned p=0;p<32*52;p++)
    assert(!memcmp(got+p*2,pixels[1]+((p/32)*3*96+(p%32)*3)*2,2));
  assert(!c[0].social.accepted && !c[0].social.friends && !c[0].social.saved);
  // Changed avatar refreshes without using old pixels; checksum protects completion.
  memset(pixels[1],0x66,sizeof pixels[1]);loop(c,8000,15000);
  assert(hr_preview_image());for(unsigned p=0;p<3328;p++)assert(hr_preview_image()->data[p]==0x66);
  // A missing avatar clears the earlier preview.
  profiles[1].portrait=NULL;loop(c,15000,20000);assert(!hr_preview_image());
  profiles[1].portrait=&portraits[1];loop(c,20000,26000);assert(hr_preview_image());
  // Leaving the popup cancels; stale sessions/boots cannot inject pixels.
  hr_preview_tick(&c[0],false,26000);assert(!hr_preview_image());
  hr_packet_t p={.kind=HR_PREVIEW_DATA,.id=1,.target=2,.boot=101,.request=11,.epoch=10};
  hr_preview_receive(&c[0],&p,26000);assert(!hr_preview_image());
  c[0].social.session++;hr_preview_tick(&c[0],true,26000);
  hr_preview_receive(&c[0],&p,26000);assert(!hr_preview_image());
  assert(hr_preview_send(&c[0],&p,26000));assert(p.flags==0);
  c[0].hardware_ok=false;hr_preview_tick(&c[0],true,26001);
  assert(!hr_preview_send(&c[0],&p,26001));assert(!hr_preview_image());
  c[0].hardware_ok=true;hr_preview_tick(&c[0],true,26002);
  p=(hr_packet_t){.kind=HR_PREVIEW_DATA,.id=1,.target=2,.boot=101,
                 .request=11,.epoch=c[0].social.session,.token=123};
  memset(p.audio,0x5a,sizeof p.audio);
  for(unsigned offset=0;offset<3328;offset+=HR_AUDIO) {
    p.flags=offset;hr_preview_receive(&c[0],&p,26002);
  }
  assert(!hr_preview_image()); // A complete but corrupt image must never display.
  assert(hr_preview_send(&c[0],&p,26002) && p.flags==0);
  hr_preview_reset();
  puts("PASS preview: both directions, loss retries, pixel equality, refresh, no-avatar, session cancel, inactive, no friendship writes");
}
