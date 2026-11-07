#include "hr_controls.h"
#include "hr_touch.h"
#include "hr_identity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static hr_view_t v[6];static hr_touch_t state[6];
static int signal[6][6];static bool receive_enabled[6];
static unsigned slot(unsigned from,unsigned peer) {
 for(unsigned i=1;i<=6;i++)if(i!=v[from].core.id&&!memcmp(v[from].core.peers[i].mac,v[peer].core.mac,6))return i;
 return 0;
}
static void init_node(unsigned k,unsigned boot) {
 v[k]=(hr_view_t){.page=HR_PAGE_HOME};state[k]=(hr_touch_t){0};
 uint8_t mac[]={0x4c,0x11,0xae,0,0,(uint8_t)(10+k)};
 hr_init(&v[k].core,2,mac,boot,0);hr_identity_enable(&v[k].core);hr_room_enable(&v[k].core,0);
}
static void reset(void) {
 for(unsigned k=0;k<6;k++) {
  init_node(k,100+k);receive_enabled[k]=true;
  for(unsigned j=0;j<6;j++)signal[k][j]=-80;
 }
 signal[0][1]=signal[1][0]=-52;
 // Stronger observers on HOME must never win a manual search.
 signal[0][2]=signal[2][0]=signal[1][2]=signal[2][1]=-15;
}
static void open_friends(unsigned k,unsigned now) {
 hr_view_home(&v[k],now);v[k].home_selection=0;
 hr_handle_key(&v[k],HR_KEY_DOWN,now);
}
static void tick(unsigned now,bool save) {
 for(unsigned k=0;k<6;k++) {
  hr_touch_tick(&state[k],&v[k],now,true);
  hr_packet_t p;
  if(hr_control(&v[k].core,&p,now)) {
   uint8_t wire[HR_IDENTITY_WIRE_MAX];size_t n=hr_identity_pack(&v[k].core,&p,wire);assert(n);
   for(unsigned j=0;j<6;j++)if(k!=j&&receive_enabled[j]) {
    hr_identity_frame_t frame;assert(hr_identity_unpack(&frame,wire,n));
    if(hr_identity_route(&v[j].core,&frame,v[k].core.mac,now))hr_receive(&v[j].core,&frame.p,v[k].core.mac,signal[k][j],now);
   }
  }
  hr_social_tick(&v[k].core,now);
  if(save&&v[k].core.social.pending)hr_social_saved(&v[k].core,true);
  hr_handle_key(&v[k],HR_KEY_NONE,now);
 }
}
static void run(unsigned a,unsigned b,bool save){for(unsigned t=a;t<b;t+=10)tick(t,save);}
static void paired(unsigned a,unsigned b) {
 assert(v[a].page==HR_PAGE_PAIR&&v[b].page==HR_PAGE_PAIR);
 assert(v[a].core.social.candidate==slot(a,b)&&v[b].core.social.candidate==slot(b,a));
 assert(!v[a].core.social.accepted&&!v[b].core.social.accepted);
}
int main(void) {
 reset();run(0,1000,false);open_friends(0,1000);open_friends(1,1000);run(1000,4000,false);
 paired(0,1);assert(v[2].page==HR_PAGE_HOME);
 // Explicit confirmation is still needed, even though both opened Friends.
 assert(!v[0].core.social.friends&&!v[1].core.social.friends);
 hr_handle_key(&v[0],HR_KEY_DOWN,4000);run(4000,4300,true);assert(!v[0].core.social.friends);
 hr_handle_key(&v[1],HR_KEY_DOWN,4300);run(4300,5200,true);
 assert(v[0].page==HR_PAGE_CARD&&v[1].page==HR_PAGE_CARD);
 assert(v[0].core.social.complete&&v[1].core.social.complete);
 // A manual retry includes an already-saved friend and ignores touch cooldown.
 hr_handle_key(&v[0],HR_KEY_DOWN_HOLD,5200);hr_handle_key(&v[1],HR_KEY_DOWN_HOLD,5200);
 v[1].core.social.friends=0;open_friends(0,5210);open_friends(1,5210);run(5210,8000,false);paired(0,1);
 hr_handle_key(&v[0],HR_KEY_DOWN_HOLD,8000);hr_handle_key(&v[1],HR_KEY_DOWN_HOLD,8000);
 open_friends(0,8010);open_friends(1,8010);run(8010,10500,false);paired(0,1);
 // Leaving/phone connection withdraws explicit availability and confirmation.
 hr_handle_key(&v[1],HR_KEY_DOWN_HOLD,10500);run(10500,11500,false);
 hr_handle_key(&v[0],HR_KEY_DOWN,11500);assert(!v[0].core.social.accepted);
 reset();run(0,1000,false);open_friends(0,1000);run(1000,4000,false);
 assert(v[0].page==HR_PAGE_FRIENDS&&!v[0].core.social.accepted);
 // HOME observers may auto-pair with each other, but not with the manual user.
 if(v[1].page==HR_PAGE_PAIR)assert(v[1].core.social.candidate!=slot(1,0));
 open_friends(1,4000);v[1].phone_connected=true;run(4000,6500,false);
 assert(v[0].page==HR_PAGE_FRIENDS&&!v[1].core.social.active);
 v[1].phone_connected=false;open_friends(1,6500);run(6500,9500,false);paired(0,1);
 // Deep wake reinitializes RAM state; reopening Friends must still work.
 init_node(0,900);open_friends(0,9600);open_friends(1,9600);run(9600,13000,false);paired(0,1);
 // No activation bypass; two far participants cannot pair.
 reset();v[0].activation_required=true;open_friends(0,0);open_friends(1,0);run(0,4000,false);
 assert(!v[0].core.social.active&&v[1].page==HR_PAGE_FRIENDS);
 reset();signal[0][1]=signal[1][0]=-75;open_friends(0,0);open_friends(1,0);run(0,4000,false);
 assert(v[0].page==HR_PAGE_FRIENDS&&v[1].page==HR_PAGE_FRIENDS);
 // Three simultaneous searchers choose the mutual strongest pair, not the
 // first advertised role or a previously saved candidate.
 reset();signal[0][1]=signal[1][0]=-40;
 signal[0][2]=signal[2][0]=signal[1][2]=signal[2][1]=-52;
 for(unsigned k=0;k<3;k++)open_friends(k,0);
 run(0,4000,false);paired(0,1);assert(v[2].page==HR_PAGE_FRIENDS);
 // A newly stronger third participant cannot replace an already shown person.
 signal[0][2]=signal[2][0]=signal[1][2]=signal[2][1]=-20;
 run(4000,6500,false);paired(0,1);assert(v[2].page==HR_PAGE_FRIENDS);
 // Equal-strength choices wait rather than guess; stale proposals cannot pair.
 reset();for(unsigned k=0;k<3;k++){open_friends(k,0);for(unsigned j=0;j<3;j++)signal[k][j]=-45;}
 run(0,4000,false);for(unsigned k=0;k<3;k++)assert(v[k].page==HR_PAGE_FRIENDS);
 reset();open_friends(0,0);open_friends(1,0);run(0,300,false);
 receive_enabled[1]=false;run(300,4000,false);
 assert(v[0].page==HR_PAGE_FRIENDS&&v[1].page==HR_PAGE_FRIENDS);
 // All six can search concurrently as three mutually nearest pairs.
 reset();for(unsigned k=0;k<6;k++)for(unsigned j=0;j<6;j++)signal[k][j]=(j==(k^1))?-45:-75;
 for(unsigned k=0;k<6;k++)open_friends(k,0);
 run(0,4000,false);for(unsigned k=0;k<6;k+=2)paired(k,k+1);
 puts("PASS six-node manual Friends: strongest/mutual/ambiguous/locked pairs, MAC routing, explicit participation, cooldown retry, saved peer refresh, confirm, leave/phone/activation/wake/range");
}
