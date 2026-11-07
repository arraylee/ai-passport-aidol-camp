#include "hr_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static hr_core_t nodes[7];
static const uint8_t macs[7][6]={{0},{2,0,0,0,0,1},{2,0,0,0,0,2},{2,0,0,0,0,3},{2,0,0,0,0,4},{2,0,0,0,0,5},{2,0,0,0,0,6}};
static unsigned messages[HR_KIND_MAX+1];
static void init(void) {
 memset(messages,0,sizeof messages);
 for(unsigned i=1;i<=6;i++)hr_init(&nodes[i],i,macs[i],100+i,0);
}
static void step(unsigned t,unsigned opportunity,unsigned loss_begin,unsigned loss_end) {
 for(unsigned i=1;i<=6;i++)hr_tick(&nodes[i],t);
 if(t%opportunity==0)for(unsigned i=1;i<=6;i++) {
  hr_packet_t p;
  if(!hr_control(&nodes[i],&p,t))continue;
  messages[p.kind]++;
  uint8_t wire[HR_WIRE_MAX];size_t n=hr_pack(&p,wire);hr_packet_t decoded;
  assert(hr_unpack(&decoded,wire,n));
  for(unsigned j=1;j<=6;j++)if(i!=j) {
   if(t>=loss_begin&&t<loss_end&&i==1&&j==2&&p.kind==HR_GRANT)continue;
   hr_receive(&nodes[j],&decoded,macs[i],-40,t);
  }
 }
 unsigned speakers=0;
 for(unsigned i=1;i<=6;i++)speakers+=hr_can_talk(&nodes[i],t);
 assert(speakers<=1);
}
static void run(unsigned a,unsigned b,unsigned opportunity,unsigned loss_begin,unsigned loss_end) {
 for(unsigned t=a;t<b;t+=10)step(t,opportunity,loss_begin,loss_end);
}
static void delayed_send_opportunities(void) {
 init();run(0,1500,150,0,0);
 // Actual asynchronous sends can leave only one opportunity every 150 ms.
 // Repeated HELLOs must not starve either REQUEST or GRANT indefinitely.
 hr_press(&nodes[2],0,1500);unsigned audible=0;
 for(unsigned t=1500;t<6500;t+=10) {
  step(t,150,0,0);audible+=hr_can_talk(&nodes[2],t);
 }
 printf("slow send: hello=%u request=%u grant=%u audible_ticks=%u\n",messages[HR_HELLO],messages[HR_REQUEST],messages[HR_GRANT],audible);
 fflush(stdout);
 assert(messages[HR_REQUEST]>0&&messages[HR_GRANT]>0&&audible>200);
 for(unsigned i=1;i<=6;i++)for(unsigned j=1;j<=6;j++)assert(hr_online(&nodes[i],j,6500));
}
static void recover_without_repress(void) {
 init();run(0,1000,10,0,0);hr_press(&nodes[2],0,1000);
 run(1000,2000,10,0,0);assert(hr_can_talk(&nodes[2],2000));
 run(2000,2900,10,2000,2900);assert(!hr_can_talk(&nodes[2],2890));
 assert(nodes[2].held); // User is still physically pressing the original key.
 run(2900,4200,10,0,0);
 printf("recovered: held=%u blocked=%u talking=%u\n",nodes[2].held,nodes[2].blocked,hr_can_talk(&nodes[2],4200));fflush(stdout);
 assert(hr_can_talk(&nodes[2],4200));
 hr_release(&nodes[2],4210);run(4210,5500,10,0,0);assert(!hr_can_talk(&nodes[2],5500));
}
int main(int argc,char **argv) {
 if(argc==2&&!strcmp(argv[1],"recovery"))recover_without_repress();
 else {delayed_send_opportunities();recover_without_repress();}
 puts("PASS delayed radio scheduling and held-key lease recovery");
}
