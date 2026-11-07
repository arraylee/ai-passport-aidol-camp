#include "hr_touch.h"
#include "hr_controls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static hr_view_t view(unsigned id) {
 hr_view_t v={.page=HR_PAGE_HOME};uint8_t mac[6]={0,0,0,0,0,id};
 hr_init(&v.core,id,mac,10+id,0);v.core.hardware_ok=true;return v;
}
static void peer(hr_view_t *v,unsigned id,int rssi,uint32_t now) {
 hr_peer_t *p=&v->core.peers[id];p->seen=true;p->last=p->hello_at=now;
 p->boot=10+id;p->social_session=1;p->social_flags=2;
 p->touch_target=v->core.id;
 for(unsigned i=0;i<3;i++)p->signal_window[i]=rssi;
 p->signal_q8=rssi*256;p->zone=rssi>=-57?2:1;p->samples++;
}
static void tick(hr_touch_t*s,hr_view_t*v,uint32_t now,int signal) {
 peer(v,2,signal,now);hr_touch_tick(s,v,now,true);
}
static void transient_reciprocal(void) {
 hr_view_t v=view(1);hr_touch_t s={0};
 for(unsigned t=0;t<2000;t+=100) {
  peer(&v,2,-20,t);v.core.peers[2].touch_target=0;hr_touch_tick(&s,&v,t,true);
 }
 peer(&v,2,-20,2000);hr_touch_tick(&s,&v,2000,true);
 assert(v.page==HR_PAGE_HOME); // One reciprocal frame must not open a stale popup.
 for(unsigned t=2005;t<=2300;t+=5) {hr_touch_tick(&s,&v,t,true);assert(v.page==HR_PAGE_HOME);}
 peer(&v,2,-20,2310);hr_touch_tick(&s,&v,2310,true);
 assert(v.page==HR_PAGE_PAIR&&v.core.social.candidate==2);
}
int main(void) {
 transient_reciprocal();
 hr_touch_t s={0};hr_view_t v=view(1);
 tick(&s,&v,100,-25);assert(v.core.social.active&&v.page==HR_PAGE_HOME);
 // One strong spike, or holding a key, is not a touch.
 tick(&s,&v,200,-65);tick(&s,&v,300,-25);
 for(unsigned t=400;t<1600;t+=100){peer(&v,2,-25,t);hr_touch_tick(&s,&v,t,false);}
 assert(v.page==HR_PAGE_HOME);
 for(unsigned t=1600;t<=3500;t+=100)tick(&s,&v,t,-25);
 assert(v.page==HR_PAGE_PAIR&&v.core.social.candidate==2&&!v.core.social.accepted);
 hr_touch_tick(&s,&v,3501,true); // Observes pairing page and locks this candidate.
 for(unsigned i=0;i<3;i++) {
  int keys[]={HR_KEY_UP,HR_KEY_CLICK,HR_KEY_CENTER_HOLD};
  hr_handle_key(&v,keys[i],3502+i);
  assert(v.page==HR_PAGE_PAIR&&v.core.social.candidate==2&&!v.core.social.accepted);
 }
 // Losing the prompted peer must not substitute another nearby device.
 peer(&v,3,-25,3505);v.core.peers[2].social_flags=0;
 hr_social_tick(&v.core,3505);
 assert(v.core.social.candidate==0&&!v.core.social.accepted);
 hr_handle_key(&v,HR_KEY_DOWN,3506);
 assert(!v.core.social.accepted);
 peer(&v,2,-25,3507);hr_social_tick(&v.core,3507);
 assert(v.core.social.candidate==2&&!v.core.social.accepted);
 v.core.peers[3].seen=false;
 hr_handle_key(&v,HR_KEY_DOWN_HOLD,3600);
 for(unsigned t=3600;t<=6600;t+=100)tick(&s,&v,t,-25);
 assert(v.page==HR_PAGE_HOME); // Cancel must not immediately re-open.
 for(unsigned t=6700;t<=10000;t+=100)tick(&s,&v,t,-38);
 for(unsigned t=10100;t<=12100;t+=100)tick(&s,&v,t,-25);
 assert(v.page==HR_PAGE_PAIR);
 // Ambiguous strongest signal and known friends do not auto-open.
 s=(hr_touch_t){0};v=view(1);
 for(unsigned t=0;t<=3000;t+=100){peer(&v,2,-25,t);peer(&v,3,-28,t);hr_touch_tick(&s,&v,t,true);}
 assert(v.page==HR_PAGE_HOME);
 v.core.social.friends=1u<<2;
 for(unsigned t=3100;t<=5000;t+=100){peer(&v,2,-25,t);peer(&v,3,-60,t);hr_touch_tick(&s,&v,t,true);}
 assert(v.page==HR_PAGE_HOME);
 // Busy targets, phone, intro and non-home pages never steal the screen.
 for(unsigned mode=0;mode<6;mode++) {
  s=(hr_touch_t){0};v=view(1);
  if(mode==0)v.phone_connected=true;
  if(mode==1)v.intro_kind=HR_INTRO_BOOT;
  if(mode==2)v.setup=true;
  if(mode==3)v.page=HR_PAGE_RADIO;
  if(mode==4)v.page=HR_PAGE_GALLERY;
  unsigned original=v.page;
  for(unsigned t=0;t<=2000;t+=100){peer(&v,2,-25,t);if(mode==5){v.core.peers[2].social_flags|=4;v.core.peers[2].social_target=3;}hr_touch_tick(&s,&v,t,true);}
  assert(v.page==original);
 }
 // Leaving HOME withdraws automatic discovery; Friends starts explicit search.
 s=(hr_touch_t){0};v=view(1);tick(&s,&v,100,-25);
 v.home_selection=1;hr_handle_key(&v,HR_KEY_DOWN,200);
 hr_touch_tick(&s,&v,200,true);
 assert(v.page==HR_PAGE_LOBBY&&!v.core.social.active);
 v.page=HR_PAGE_HOME;tick(&s,&v,300,-25);v.home_selection=0;
 hr_handle_key(&v,HR_KEY_DOWN,400);hr_touch_tick(&s,&v,400,true);
 assert(v.page==HR_PAGE_FRIENDS&&v.core.social.active&&v.core.social.manual);
 // A phone connection withdraws HOME presence and requires a new dwell.
 s=(hr_touch_t){0};v=view(1);tick(&s,&v,100,-25);v.phone_connected=true;
 hr_touch_tick(&s,&v,200,true);assert(!v.core.social.active);
 v.phone_connected=false;tick(&s,&v,1500,-25);assert(v.page==HR_PAGE_HOME);
 // Stale samples cannot satisfy dwell; clock wrap still works with live frames.
 s=(hr_touch_t){0};v=view(1);peer(&v,2,-25,100);hr_touch_tick(&s,&v,100,true);
 hr_touch_tick(&s,&v,1400,true);assert(v.page==HR_PAGE_HOME);
 s=(hr_touch_t){0};v=view(1);
 for(unsigned dt=0;dt<=1900;dt+=100)tick(&s,&v,UINT32_MAX-500+dt,-25);
 assert(v.page==HR_PAGE_PAIR);
 // A moderate signal (-40 dBm), unilateral closeness, and inactive stack
 // never open either side even after a prolonged dwell.
 for(unsigned mode=0;mode<3;mode++) {
  s=(hr_touch_t){0};v=view(1);v.activation_required=mode==2;
  for(unsigned t=0;t<=5000;t+=100){
   peer(&v,2,mode==0?-40:-25,t);
   if(mode==1)v.core.peers[2].touch_target=0;
   hr_touch_tick(&s,&v,t,true);
  }
  assert(v.page==HR_PAGE_HOME);
  if(mode==2)assert(!v.core.social.active&&!v.core.touch_target);
 }
 // Six inactive badges may be stacked with extreme signal; none advertises
 // friendship availability or opens a prompt. One active badge cannot pull
 // an inactive badge into a session.
 hr_view_t stack[6];hr_touch_t states[6]={{0}};
 for(unsigned i=0;i<6;i++){stack[i]=view(i+1);stack[i].activation_required=true;}
 unsigned inactive_hellos=0;
 for(unsigned t=0;t<5000;t+=50) {
  for(unsigned i=0;i<6;i++) {
   hr_touch_tick(&states[i],&stack[i],t,true);hr_packet_t p;
   if(hr_control(&stack[i].core,&p,t)&&p.kind==HR_HELLO) {
    inactive_hellos++;
    assert(!(p.flags & (2|32)));
    for(unsigned j=0;j<6;j++)if(i!=j)hr_receive(&stack[j].core,&p,stack[i].core.mac,-10,t);
   }
   assert(stack[i].page==HR_PAGE_HOME);
  }
 }
 assert(inactive_hellos<300); // No per-tick beacon flooding while inactive.
 // Actual HELLO packets between two HOME screens; explicit confirmation retained.
 hr_view_t a=view(1),b=view(2);hr_touch_t sa={0},sb={0};
 for(unsigned t=0;t<3000;t+=10){
  hr_touch_tick(&sa,&a,t,true);hr_touch_tick(&sb,&b,t,true);
  hr_packet_t p;
  if(hr_control(&a.core,&p,t)&&p.kind==HR_HELLO){uint8_t wire[HR_WIRE_MAX];size_t n=hr_pack(&p,wire);assert(hr_unpack(&p,wire,n));hr_receive(&b.core,&p,a.core.mac,-25,t);}
  if(hr_control(&b.core,&p,t)&&p.kind==HR_HELLO){uint8_t wire[HR_WIRE_MAX];size_t n=hr_pack(&p,wire);assert(hr_unpack(&p,wire,n));hr_receive(&a.core,&p,b.core.mac,-25,t);}
  hr_social_tick(&a.core,t);hr_social_tick(&b.core,t);
 }
 assert(a.page==HR_PAGE_PAIR&&b.page==HR_PAGE_PAIR);
 assert(!a.core.social.accepted&&!b.core.social.accepted);
 hr_handle_key(&a,HR_KEY_DOWN,3000);hr_handle_key(&b,HR_KEY_DOWN,3000);
 for(unsigned t=3000;t<3600;t+=10){
  hr_packet_t p;
  if(hr_control(&a.core,&p,t)&&p.kind==HR_HELLO){uint8_t wire[HR_WIRE_MAX];size_t n=hr_pack(&p,wire);assert(hr_unpack(&p,wire,n));hr_receive(&b.core,&p,a.core.mac,-25,t);}
  if(hr_control(&b.core,&p,t)&&p.kind==HR_HELLO){uint8_t wire[HR_WIRE_MAX];size_t n=hr_pack(&p,wire);assert(hr_unpack(&p,wire,n));hr_receive(&a.core,&p,b.core.mac,-25,t);}
  hr_social_tick(&a.core,t);hr_social_tick(&b.core,t);
  if(a.core.social.pending)hr_social_saved(&a.core,true);
  if(b.core.social.pending)hr_social_saved(&b.core,true);
  hr_handle_key(&a,HR_KEY_NONE,t);hr_handle_key(&b,HR_KEY_NONE,t);
  hr_touch_tick(&sa,&a,t,true);hr_touch_tick(&sb,&b,t,true);
 }
 assert(a.core.social.complete&&b.core.social.complete);
 assert(a.page==HR_PAGE_CARD&&b.page==HR_PAGE_CARD);
 assert(a.card_id==2&&b.card_id==1&&a.card_from_pair&&b.card_from_pair);
 assert(a.core.social.active&&b.core.social.active);
 hr_handle_key(&a,HR_KEY_DOWN_HOLD,3601);
 assert(a.page==HR_PAGE_HOME&&!a.core.social.active);
 // One person cancels, separates slightly, and retries while the other
 // remains on their popup. A fresh pair of sessions must complete normally.
 a=view(1);b=view(2);sa=(hr_touch_t){0};sb=(hr_touch_t){0};
 for(unsigned t=0;t<7000;t+=10) {
  if(t==3000)hr_handle_key(&a,HR_KEY_DOWN_HOLD,t);
  hr_touch_tick(&sa,&a,t,true);hr_touch_tick(&sb,&b,t,true);
  hr_packet_t p;int signal=t>=3000&&t<4000?-38:-25;
  if(hr_control(&a.core,&p,t)&&p.kind==HR_HELLO)hr_receive(&b.core,&p,a.core.mac,signal,t);
  if(hr_control(&b.core,&p,t)&&p.kind==HR_HELLO)hr_receive(&a.core,&p,b.core.mac,signal,t);
  hr_social_tick(&a.core,t);hr_social_tick(&b.core,t);
 }
 assert(a.page==HR_PAGE_PAIR&&b.page==HR_PAGE_PAIR);
 assert(!a.core.social.friends&&!b.core.social.friends);
 hr_handle_key(&a,HR_KEY_DOWN,7000);hr_handle_key(&b,HR_KEY_DOWN,7000);
 for(unsigned t=7000;t<7600;t+=10) {
  hr_packet_t p;
  if(hr_control(&a.core,&p,t)&&p.kind==HR_HELLO)hr_receive(&b.core,&p,a.core.mac,-25,t);
  if(hr_control(&b.core,&p,t)&&p.kind==HR_HELLO)hr_receive(&a.core,&p,b.core.mac,-25,t);
  hr_social_tick(&a.core,t);hr_social_tick(&b.core,t);
  if(a.core.social.pending)hr_social_saved(&a.core,true);
  if(b.core.social.pending)hr_social_saved(&b.core,true);
  hr_handle_key(&a,HR_KEY_NONE,t);hr_handle_key(&b,HR_KEY_NONE,t);
 }
 assert(a.page==HR_PAGE_CARD&&b.page==HR_PAGE_CARD);
 puts("PASS home touch: dwell, ambiguity, known peers, cancel/rearm, stale/wrap, busy isolation, real HELLO mutual confirmation");
}
