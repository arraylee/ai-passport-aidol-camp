#include "hr_core.h"
#include <assert.h>
#include <stdio.h>
static hr_core_t c[4];
static unsigned char mac[4][6]={{0},{2,0,0,0,0,1},{2,0,0,0,0,2},{2,0,0,0,0,3}};
static unsigned offset[4]={0,100000,800,49000};
static void step(unsigned t){
 for(unsigned i=1;i<=3;i++)hr_tick(&c[i],offset[i]+t);
 for(unsigned i=1;i<=3;i++){
  hr_packet_t p;
  if(hr_control(&c[i],&p,offset[i]+t)){
   for(unsigned j=1;j<=3;j++)if(i!=j)hr_receive(&c[j],&p,mac[i],-60,offset[j]+t);
  }
 }
}
int main(void){
 for(unsigned i=1;i<=3;i++)hr_init(&c[i],i,mac[i],100+i,offset[i]);
 for(unsigned t=0;t<1000;t+=10)step(t);
 hr_press(&c[2],3,offset[2]+1000);
 for(unsigned t=1000;t<2000;t+=10)step(t);
 assert(hr_can_talk(&c[2],offset[2]+2000));
 hr_packet_t p;unsigned char audio[HR_AUDIO]={0};
 assert(hr_voice(&c[2],&p,audio,offset[2]+2000));
 assert(hr_receive(&c[3],&p,mac[2],-60,offset[3]+2000));
 printf("PASS independent clocks: host100000, speaker800, receiver49000 origins\n");
 hr_init(&c[1],1,mac[1],101,0);
 p=(hr_packet_t){.kind=HR_HELLO,.id=2,.boot=102,.seq=1};
 hr_receive(&c[1],&p,mac[2],-80,0);
 printf("RSSI step -80 -> -48 dBm, HELLO every314ms:\n");
 for(unsigned t=314;t<=5024;t+=314){
  p.seq++;hr_receive(&c[1],&p,mac[2],-48,t);
  printf("t=%u filtered=%d zone=%d trend=%d\n",t,c[1].peers[2].signal_q8/256,c[1].peers[2].zone,c[1].peers[2].trend);
 }
 unsigned cached_now=1000,async_at=1001;
 printf("Queue race cached=%u callback=%u unsigned_age=%u RX_drop=%u TX_drop=%u\n",cached_now,async_at,cached_now-async_at,(cached_now-async_at)>180,!((cached_now-async_at)<80));
}
