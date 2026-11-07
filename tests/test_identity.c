#include "hr_core.h"
#include "hr_social.h"
#include "hr_touch.h"
#include "hr_controls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if __has_include("hr_identity.h")
#include "hr_identity.h"
#else
/* Exercise the shipped v059 path before the adapter exists. */
typedef struct {hr_packet_t p;uint8_t target[6],speaker[6];} hr_identity_frame_t;
#define HR_IDENTITY_WIRE_MAX 212
static void hr_identity_enable(hr_core_t*c){(void)c;}
static size_t hr_identity_pack(const hr_core_t*c,const hr_packet_t*p,uint8_t*b){(void)c;return hr_pack(p,b);}
static bool hr_identity_unpack(hr_identity_frame_t*f,const uint8_t*b,size_t n){return hr_unpack(&f->p,b,n);}
static bool hr_identity_route(hr_core_t*c,hr_identity_frame_t*f,const uint8_t*mac,uint32_t now){(void)c;(void)f;(void)mac;(void)now;return true;}
static void hr_identity_elect(hr_core_t*c,uint32_t now){(void)c;(void)now;}
#endif
static hr_core_t nodes[6];static unsigned heard[6];static bool active[6];static bool touch_mode;static bool crowded_mode;static bool loss_mode;
static unsigned find(const hr_core_t*c,const uint8_t*mac){if(!memcmp(c->mac,mac,6))return c->id;for(unsigned i=1;i<=6;i++)if(i!=c->id&&!memcmp(c->peers[i].mac,mac,6))return i;return 0;}
static void deliver(unsigned source,const hr_packet_t*p,uint32_t now){
 uint8_t wire[HR_IDENTITY_WIRE_MAX];size_t n=hr_identity_pack(&nodes[source],p,wire);assert(n&&n<=250);
 for(unsigned k=0;k<6;k++)if(k!=source&&active[k]){
  if(loss_mode&&(p->seq*7+k*3+source)%5==0)continue;
  hr_identity_frame_t f;assert(hr_identity_unpack(&f,wire,n));
  if(hr_identity_route(&nodes[k],&f,nodes[source].mac,now))if(hr_receive(&nodes[k],&f.p,nodes[source].mac,crowded_mode?((k<2&&source<2)?-20:((k==2&&source<2)||(source==2&&k<2))?-27:-45):touch_mode&&!(k<2&&source<2)?-65:-20,now))heard[k]++;
  hr_identity_elect(&nodes[k],now);
 }
}
static void tick(uint32_t now){for(unsigned k=0;k<6;k++)if(active[k]){hr_identity_elect(&nodes[k],now);hr_tick(&nodes[k],now);hr_packet_t p;if(hr_control(&nodes[k],&p,now))deliver(k,&p,now);}}
static void run(uint32_t a,uint32_t b){for(uint32_t n=a;n<b;n+=10)tick(n);}
static void reset_nodes(void) {
 for(unsigned k=0;k<6;k++) {
  uint8_t mac[]={0x4c,0x11,0xae,0,0,(uint8_t)(10+k)};
  hr_init(&nodes[k],2,mac,100+k,0);hr_identity_enable(&nodes[k]);hr_room_enable(&nodes[k],0);active[k]=true;
 }
}
static void sn_cases(void) {
 reset_nodes();hr_identity_set_sn(&nodes[1],"PUBLIC-SN-CLONE");hr_identity_set_sn(&nodes[2],"PUBLIC-SN-CLONE");
 for(unsigned k=0;k<6;k++)hr_room_join(&nodes[k],true,0);
 run(0,2500);
 for(unsigned k=0;k<6;k++) {
  assert(hr_conflict(&nodes[k],find(&nodes[k],nodes[1].mac),2500));
  assert(hr_conflict(&nodes[k],find(&nodes[k],nodes[2].mac),2500));
  assert(!hr_conflict(&nodes[k],find(&nodes[k],nodes[0].mac),2500));
 }
 hr_packet_t p;uint8_t audio[HR_AUDIO]={0};memset(heard,0,sizeof heard);
 hr_press(&nodes[4],find(&nodes[4],nodes[5].mac),2500);run(2500,3300);
 assert(hr_voice(&nodes[4],&p,audio,3300));deliver(4,&p,3300);assert(heard[5]==1);
 hr_release(&nodes[4],3310);active[2]=false;run(3310,10500);
 assert(!hr_conflict(&nodes[1],nodes[1].id,10500));
 /* A replayed HELLO must never overwrite the SN of an accepted boot/seq. */
 unsigned id=find(&nodes[0],nodes[1].mac);uint32_t seq=nodes[0].peers[id].seq;
 p=(hr_packet_t){.kind=HR_HELLO,.id=id,.boot=nodes[1].boot,.seq=seq-1};
 memcpy(p.audio,"CHANGED",7);hr_receive(&nodes[0],&p,nodes[1].mac,-20,10501);
 assert(!strcmp(nodes[0].peers[id].sn,"PUBLIC-SN-CLONE"));
 /* Invalid provisioned/public fields fall back to each physical MAC. */
 hr_identity_set_sn(&nodes[1],"");hr_identity_set_sn(&nodes[2],"bad\nSN");
 assert(!strcmp(nodes[1].sn,"4c11ae00000b")&&strcmp(nodes[1].sn,nodes[2].sn));
 nodes[1].next_hello=10510;assert(hr_control(&nodes[1],&p,10510)&&p.kind==HR_HELLO);
 uint8_t wire[HR_IDENTITY_WIRE_MAX];size_t n=hr_identity_pack(&nodes[1],&p,wire);assert(n==72);
 memset(wire+44,0,24);uint32_t crc=hr_crc(wire,n-4);for(unsigned j=0;j<4;j++)wire[n-4+j]=crc>>(j*8);
 hr_identity_frame_t f;assert(hr_identity_unpack(&f,wire,n));assert(hr_identity_route(&nodes[0],&f,nodes[1].mac,10510));
 assert(!strcmp((char*)f.p.audio,"4c11ae00000b"));hr_receive(&nodes[0],&f.p,nodes[1].mac,-20,10510);
 assert(!strcmp(nodes[0].peers[id].sn,nodes[1].sn));
}
static void touch_cases(void) {
 reset_nodes();touch_mode=true;hr_view_t views[6]={{0}};hr_touch_t states[6]={{0}};
 for(unsigned t=0;t<5000;t+=10) {
  tick(t);
  for(unsigned k=0;k<6;k++) {
   views[k].core=nodes[k];hr_touch_tick(&states[k],&views[k],t,true);hr_social_tick(&views[k].core,t);nodes[k]=views[k].core;
  }
 }
 for(unsigned k=0;k<6;k++)assert(views[k].page==(k<2?HR_PAGE_PAIR:HR_PAGE_HOME));
 assert(!memcmp(nodes[0].peers[nodes[0].social.candidate].mac,nodes[1].mac,6));
 assert(!memcmp(nodes[1].peers[nodes[1].social.candidate].mac,nodes[0].mac,6));
 hr_social_confirm(&nodes[0],5000);hr_social_confirm(&nodes[1],5000);
 for(unsigned t=5000;t<5600;t+=10) {
  tick(t);for(unsigned k=0;k<2;k++){hr_social_tick(&nodes[k],t);if(nodes[k].social.pending)hr_social_saved(&nodes[k],true);}
 }
 assert(nodes[0].social.complete&&nodes[1].social.complete);
 touch_mode=false;
}
static void crowded_cases(void) {
 crowded_mode=true;touch_cases(); // Stable strongest pair7dB above nearby observers.
 loss_mode=true;touch_cases();loss_mode=false;crowded_mode=false;
}
static void unavailable_host_cases(void) {
 reset_nodes();for(unsigned k=0;k<6;k++)hr_room_join(&nodes[k],true,0);run(0,2500);
 nodes[0].hardware_ok=false;run(2500,4700);
 for(unsigned k=1;k<6;k++)assert(nodes[k].host==find(&nodes[k],nodes[1].mac));
 nodes[1].game_busy=true;run(4700,5200); // Gameplay keeps coordinator arbitration alive.
 memset(heard,0,sizeof heard);hr_press(&nodes[4],find(&nodes[4],nodes[5].mac),5200);run(5200,6100);
 uint8_t audio[HR_AUDIO]={0};hr_packet_t p;assert(hr_voice(&nodes[4],&p,audio,6100));deliver(4,&p,6100);assert(heard[5]==1);
 nodes[0].hardware_ok=true;run(6100,8100);
 for(unsigned k=0;k<6;k++)assert(nodes[k].host==find(&nodes[k],nodes[0].mac));
 assert(!hr_can_talk(&nodes[4],8100));hr_release(&nodes[4],8110);run(8110,9200);
 hr_press(&nodes[4],0,9200);run(9200,10100);assert(hr_can_talk(&nodes[4],10100));
}
static void unknown_hello_references(void) {
 reset_nodes();hr_identity_frame_t f={.p={.kind=HR_HELLO,.id=2,.boot=101,.seq=1,.flags=2|4|32},
  .target={0x4c,0x11,0xae,0,0,98},.speaker={0x4c,0x11,0xae,0,0,99}};
 assert(hr_identity_route(&nodes[0],&f,nodes[1].mac,100));
 hr_receive(&nodes[0],&f.p,nodes[1].mac,-20,100);
 unsigned id=find(&nodes[0],nodes[1].mac);
 assert(hr_online(&nodes[0],id,100)&&!nodes[0].peers[id].social_target&&!nodes[0].peers[id].touch_target);
 f.p.kind=HR_REQUEST;assert(!hr_identity_route(&nodes[0],&f,nodes[1].mac,101));
}
static void capacity_cases(void) {
 reset_nodes();run(0,2100);hr_core_t c=nodes[0];uint8_t newcomer[6]={0x4c,0x11,0xae,0,0,99};
 hr_identity_frame_t f={.p={.kind=HR_HELLO,.id=2,.boot=999,.seq=1}};
 assert(!hr_identity_route(&c,&f,newcomer,2100));
 c.social.friends=0x7e&~(1u<<c.id);assert(!hr_identity_route(&c,&f,newcomer,10000));
 unsigned victim=find(&c,nodes[1].mac);c.social.friends&=~(1u<<victim);c.held=true;
 assert(!hr_identity_route(&c,&f,newcomer,10000));c.held=false;assert(hr_identity_route(&c,&f,newcomer,10000));
 assert(f.p.id==victim&&!memcmp(c.peers[victim].mac,newcomer,6));
 /* A delayed old audio packet cannot acquire the reclaimed slot. */
 f=(hr_identity_frame_t){.p={.kind=HR_VOICE,.id=2,.boot=101,.seq=900}};
 assert(!hr_identity_route(&c,&f,nodes[1].mac,10001));
}
int main(void){
 for(unsigned k=0;k<6;k++){uint8_t mac[]={0x4c,0x11,0xae,0,0,(uint8_t)(10+k)};hr_init(&nodes[k],2,mac,100+k,0);hr_identity_enable(&nodes[k]);hr_room_enable(&nodes[k],0);active[k]=true;}
 run(0,2100);
 for(unsigned k=0;k<6;k++){
  unsigned seen=0;for(unsigned j=0;j<6;j++)if(j!=k){unsigned id=find(&nodes[k],nodes[j].mac);if(id&&hr_online(&nodes[k],id,2100))seen++;}
  if(seen!=5){fprintf(stderr,"FAIL same role: node %u sees %u/5 distinct MACs\n",k,seen);return 1;}
  assert(!hr_conflict(&nodes[k],2,2100));
 }
#ifdef HR_IDENTITY_WIRE_VERSION
 for(unsigned k=0;k<6;k++)hr_room_join(&nodes[k],true,2200);
 run(2200,5000);
 for(unsigned k=0;k<6;k++)assert(find(&nodes[k],nodes[0].mac)==nodes[k].host);
 uint8_t audio[HR_AUDIO]={0};
 for(unsigned sender=0;sender<6;sender++){
  unsigned recipient=(sender+2)%6;uint32_t at=5000+sender*2500;memset(heard,0,sizeof heard);
  hr_press(&nodes[sender],find(&nodes[sender],nodes[recipient].mac),at);
  run(at,at+800);assert(hr_can_talk(&nodes[sender],at+800));
  hr_packet_t voice;assert(hr_voice(&nodes[sender],&voice,audio,at+800));deliver(sender,&voice,at+800);
  for(unsigned k=0;k<6;k++)assert(heard[k]==(k==recipient?1u:0u));
  hr_release(&nodes[sender],at+810);run(at+810,at+2400);
 }
 memset(heard,0,sizeof heard);hr_press(&nodes[3],0,21000);run(21000,21800);hr_packet_t p;assert(hr_voice(&nodes[3],&p,audio,21800));deliver(3,&p,21800);
 for(unsigned k=0;k<6;k++)assert(heard[k]==(k==3?0u:1u));
 hr_release(&nodes[3],21810);run(21810,23000);
 /* Scores refer to physical source, despite every sender advertising role2. */
 for(unsigned k=0;k<6;k++){nodes[k].best_valid=true;nodes[k].best_score=100*(k+1);assert(hr_score_packet(&nodes[k],&p,23000));deliver(k,&p,23000);}
 uint8_t ranked[6];for(unsigned k=0;k<6;k++){assert(hr_ranked(&nodes[k],23001,ranked)==6);assert(ranked[0]==find(&nodes[k],nodes[5].mac));}
 /* Save mapping+friends atomically; reload followed by reverse discovery. */
 uint8_t record[HR_ROSTER_BYTES];unsigned friend=find(&nodes[4],nodes[0].mac);nodes[4].social.friends=1u<<friend;hr_roster_encode(&nodes[4],record);
 hr_core_t restored;hr_init(&restored,2,nodes[4].mac,900,24000);hr_identity_enable(&restored);assert(hr_roster_decode(&restored,record));assert(find(&restored,nodes[0].mac)==friend);assert(restored.social.friends==(1u<<friend));
 record[20]^=1;assert(!hr_roster_decode(&restored,record));record[20]^=1;
 hr_core_t wrong;hr_init(&wrong,2,nodes[5].mac,901,24000);hr_identity_enable(&wrong);assert(!hr_roster_decode(&wrong,record));
 /* Old versions and corrupt packets do not enter the new protocol. */
 uint8_t raw[HR_IDENTITY_WIRE_MAX];hr_identity_frame_t f;hr_control(&nodes[0],&p,24000);size_t n=hr_pack(&p,raw);assert(!hr_identity_unpack(&f,raw,n));
 n=hr_identity_pack(&nodes[0],&p,raw);assert(p.kind==HR_HELLO&&n==72);raw[n-1]^=1;assert(!hr_identity_unpack(&f,raw,n));
 /* Coordinator leaves during an active floor: held talk cannot revive. */
 hr_press(&nodes[4],0,25000);run(25000,25800);assert(hr_can_talk(&nodes[4],25800));active[0]=false;run(25800,31000);assert(!hr_can_talk(&nodes[4],31000));
 for(unsigned k=1;k<6;k++)assert(nodes[k].host==find(&nodes[k],nodes[1].mac));
 hr_release(&nodes[4],31010);run(31010,32200);hr_press(&nodes[4],0,32200);run(32200,33000);assert(hr_can_talk(&nodes[4],33000));
#endif
 sn_cases();crowded_cases();touch_cases();capacity_cases();unknown_hello_references();unavailable_host_cases();
 puts("PASS identity: six identical roles remain distinct; addressed voice, scores, roster and host failover");return 0;
}
