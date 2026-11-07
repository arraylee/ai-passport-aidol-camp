#include "hr_core.h"
#include <assert.h>
#include <stdio.h>
static hr_core_t c;
static uint8_t mine[6]={2,0,0,0,0,1},peer[6]={2,0,0,0,0,2};
static hr_packet_t hello={.id=2,.boot=2,.kind=HR_HELLO};
static void sample(int rssi,unsigned now){hello.seq++;hr_receive(&c,&hello,peer,rssi,now);}
int main(void){
 hr_init(&c,1,mine,1,0);
 for(unsigned t=0;t<1000;t+=120)sample(-80,t);
 sample(-30,1080);assert(c.peers[2].zone==0); // isolated spike must not create a near person
 for(unsigned t=1200;t<1800;t+=120)sample(-80,t);
 unsigned near_at=0;
 for(unsigned t=1800;t<=2760;t+=120){sample(-48,t);if(c.peers[2].zone==2&&!near_at)near_at=t;}
 printf("RSSI far-to-near settled after %u ms\n",near_at?near_at-1800:9999);
 assert(near_at && near_at-1800<=840);
 for(unsigned t=2880;t<6000;t+=120){sample((t/120)%2?-58:-60,t);assert(c.peers[2].zone==2);}
 unsigned far_at=0;
 for(unsigned t=6000;t<=7080;t+=120){sample(-85,t);if(c.peers[2].zone==0&&!far_at)far_at=t;}
 assert(far_at && far_at-6000<=840);
 assert(!hr_online(&c,2,11000));
 puts("PASS fast proximity step response, isolated spike rejection, hysteresis and offline");
}
