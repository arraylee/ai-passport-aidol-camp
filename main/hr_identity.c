#include "hr_identity.h"
#include <string.h>
#include <stdio.h>
static const uint8_t zero[6];
static bool empty(const uint8_t *m){return !memcmp(m,zero,6);}
static bool valid_mac(const uint8_t *m){return !empty(m)&&!(m[0]&1);}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static const uint8_t *address(const hr_core_t *c,unsigned id){return !id?zero:id==c->id?c->mac:id<=HR_PLAYERS?c->peers[id].mac:zero;}
static unsigned slot(const hr_core_t *c,const uint8_t *mac){
 if(empty(mac))return 0;
 if(!memcmp(c->mac,mac,6))return c->id;
 for(unsigned i=1;i<=HR_PLAYERS;i++)if(i!=c->id&&!memcmp(c->peers[i].mac,mac,6))return i;
 return 0;
}
static void fallback_sn(char sn[25],const uint8_t mac[6]) {
 memset(sn,0,25);
 snprintf(sn,25,"%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}
static bool valid_sn(const uint8_t sn[24]) {
 if(!sn[0])return false;
 bool end=false;
 for(unsigned i=0;i<24;i++) {
  if(!sn[i])end=true;
  else if(end||sn[i]<32||sn[i]==127)return false;
 }
 return true;
}
void hr_identity_set_sn(hr_core_t *c,const char *sn) {
 memset(c->sn,0,sizeof c->sn);
 size_t n=0;
 if(sn)while(n<25&&sn[n])n++;
 if(n&&n<=24)memcpy(c->sn,sn,n);
 if(!valid_sn((const uint8_t*)c->sn))fallback_sn(c->sn,c->mac);
}
void hr_identity_enable(hr_core_t *c){c->identity_mode=true;c->host=0;c->epoch=0;hr_identity_set_sn(c,NULL);}
size_t hr_identity_pack(const hr_core_t *c,const hr_packet_t *p,uint8_t out[HR_IDENTITY_WIRE_MAX]){
 if(!valid_mac(c->mac)||!p->id||p->id!=c->id)return 0;
 const uint8_t *to=address(c,p->target),*speaker=address(c,p->speaker);
 if((p->target&&empty(to))||(p->speaker&&empty(speaker)))return 0;
 size_t n=hr_pack(p,out);
 memmove(out+16,out+4,n-4);memcpy(out,"HSW2",4);memcpy(out+4,to,6);memcpy(out+10,speaker,6);n+=12;
 if(p->kind==HR_HELLO) {
  memcpy(out+n-4,c->sn,24);n+=24;
 }
 put32(out+n-4,hr_crc(out,n-4));return n;
}
bool hr_identity_unpack(hr_identity_frame_t *f,const uint8_t *wire,size_t n){
 if(!f||!wire||(n!=48&&n!=72&&n!=212)||memcmp(wire,"HSW2",4)||get32(wire+n-4)!=hr_crc(wire,n-4))return false;
 bool hello=wire[16]==HR_HELLO;
 if(hello!=(n==72))return false;
 uint8_t original[HR_WIRE_MAX];size_t length=hello?36:n-12;
 memcpy(original,"HSW1",4);memcpy(original+4,wire+16,length-8);put32(original+length-4,hr_crc(original,length-4));
 if(!hr_unpack(&f->p,original,length))return false;
 if(hello) {
  /* Only the public DeviceKey is present. Blank/malformed SN falls back to
   * source MAC after routing; never treat two missing SNs as duplicates. */
  if(valid_sn(wire+44))memcpy(f->p.audio,wire+44,24);
 }
 memcpy(f->target,wire+4,6);memcpy(f->speaker,wire+10,6);
 return (!f->p.target==empty(f->target))&&(!f->p.speaker==empty(f->speaker));
}
bool hr_identity_route(hr_core_t *c,hr_identity_frame_t *f,const uint8_t source[6],uint32_t now){
 if(!c->identity_mode||!c->id||!valid_mac(source)||!memcmp(source,c->mac,6))return false;
 unsigned id=slot(c,source);
 if(!id){
  if(f->p.kind!=HR_HELLO)return false;
  for(unsigned i=1;i<=HR_PLAYERS;i++)if(i!=c->id&&empty(c->peers[i].mac)){id=i;break;}
  if(!id&&!c->held&&!c->speaker&&!c->social.locked_candidate){
   uint32_t oldest=0;
   for(unsigned i=1;i<=HR_PLAYERS;i++)if(i!=c->id&&!(c->social.friends&(1u<<i))&&!hr_online(c,i,now)){
    uint32_t age=now-c->peers[i].last;if(!id||age>oldest){id=i;oldest=age;}
   }
  }
  if(!id)return false;
  memset(&c->peers[id],0,sizeof c->peers[id]);memcpy(c->peers[id].mac,source,6);c->roster_dirty=true;
 }
 unsigned target=slot(c,f->target),speaker=slot(c,f->speaker);
 /* A peer may be confirming a third badge outside our radio range. Its
  * HELLO identity/RSSI is still useful; unresolved social references are zero.
  * Addressed control/audio must instead wait for a complete MAC mapping. */
 if(f->p.kind!=HR_HELLO&&((!empty(f->target)&&!target)||(!empty(f->speaker)&&!speaker)))return false;
 if(f->p.kind==HR_HELLO&&!valid_sn(f->p.audio)) {char sn[25];fallback_sn(sn,source);memcpy(f->p.audio,sn,24);}
 f->p.id=(uint8_t)id;f->p.target=(uint8_t)target;f->p.speaker=(uint8_t)speaker;return true;
}
void hr_identity_elect(hr_core_t *c,uint32_t now){
 if(!c->identity_mode)return;
 /* Compare only freshly accepted HELLOs. A duplicate affects those hardware
  * identities, not unrelated devices using the same role or display name. */
 for(unsigned i=1;i<=HR_PLAYERS;i++) {
  if(i==c->id||!hr_online(c,i,now)||(uint32_t)(now-c->peers[i].hello_at)>=HR_OFFLINE||!c->peers[i].sn[0])continue;
  hr_peer_t *p=&c->peers[i];
  if(!memcmp(c->sn,p->sn,24)) {
   c->conflict_until=now+HR_OFFLINE;p->conflict_until=now+HR_OFFLINE;
  }
  for(unsigned j=1;j<i;j++)if(j!=c->id&&hr_online(c,j,now)&&
      (uint32_t)(now-c->peers[j].hello_at)<HR_OFFLINE&&c->peers[j].sn[0]&&!memcmp(p->sn,c->peers[j].sn,24)) {
   p->conflict_until=now+HR_OFFLINE;c->peers[j].conflict_until=now+HR_OFFLINE;
  }
 }
 unsigned host=0;
 for(unsigned i=1;i<=HR_PLAYERS;i++)if(hr_room_member(c,i,now)&&(!host||memcmp(address(c,i),address(c,host),6)<0))host=i;
 hr_coordinator_set(c,(uint8_t)host,now);
}
void hr_roster_encode(const hr_core_t *c,uint8_t b[HR_ROSTER_BYTES]){
 memset(b,0,HR_ROSTER_BYTES);memcpy(b,"HRM1",4);b[4]=c->id;b[5]=c->social.friends;b[6]=1;memcpy(b+8,c->mac,6);
 for(unsigned i=1;i<=HR_PLAYERS;i++)if(i!=c->id)memcpy(b+14+(i-1)*6,c->peers[i].mac,6);
 put32(b+50,hr_crc(b,50));
}
bool hr_roster_decode(hr_core_t *c,const uint8_t b[HR_ROSTER_BYTES]){
 if(memcmp(b,"HRM1",4)||b[4]!=c->id||b[6]!=1||b[7]||memcmp(b+8,c->mac,6)||get32(b+50)!=hr_crc(b,50)||(b[5]&~0x7e)||(b[5]&(1u<<c->id)))return false;
 for(unsigned i=1;i<=HR_PLAYERS;i++){
  const uint8_t *mac=b+14+(i-1)*6;
  if(i==c->id){if(!empty(mac))return false;continue;}
  if(empty(mac)){if(b[5]&(1u<<i))return false;continue;}
  if(!valid_mac(mac)||!memcmp(mac,c->mac,6))return false;
  for(unsigned j=1;j<i;j++)if(!memcmp(mac,b+14+(j-1)*6,6))return false;
 }
 for(unsigned i=1;i<=HR_PLAYERS;i++)if(i!=c->id)memcpy(c->peers[i].mac,b+14+(i-1)*6,6);
 c->social.friends=b[5];c->roster_dirty=false;return true;
}
