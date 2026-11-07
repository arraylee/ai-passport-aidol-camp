#include "hr_game.h"
#include "hr_game_pose.h"
#include "hr_controls.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static void tone(int16_t x[640],double hz,int gain,unsigned harmonic) {
 for(unsigned i=0;i<640;i++)x[i]=(int16_t)(gain*(sin(i*hz*6.28318530718/16000)+(harmonic?.4*sin(i*hz*12.56637061436/16000):0)));
}
static void hello(hr_core_t*a,hr_core_t*b,unsigned now) {
 a->next_hello=now;hr_packet_t p;assert(hr_control(a,&p,now));
 if(p.kind!=HR_HELLO)assert(hr_control(a,&p,now));
 assert(p.kind==HR_HELLO);
 hr_receive(b,&p,a->mac,-30,now);
}
int main(void) {
 hr_jump_animation_t animation={0};hr_game_t pose_game={.start=100,.phase=HR_GAME_READY,.grounded=true,.height=30};
 assert(hr_jump_pose(&animation,&pose_game,100)==HR_JOY_IDLE);
 pose_game.phase=HR_GAME_RUNNING;pose_game.grounded=false;pose_game.height=37;
 assert(hr_jump_pose(&animation,&pose_game,200)==HR_JOY_PUSH);
 assert(hr_jump_pose(&animation,&pose_game,299)==HR_JOY_PUSH);
 assert(hr_jump_pose(&animation,&pose_game,300)==HR_JOY_TUCK);
 pose_game.height=90;assert(hr_jump_pose(&animation,&pose_game,400)==HR_JOY_TUCK);
 pose_game.height=80;assert(hr_jump_pose(&animation,&pose_game,440)==HR_JOY_LAND);
 assert(hr_jump_pose(&animation,&pose_game,480)==HR_JOY_LAND);
 pose_game.height=82;assert(hr_jump_pose(&animation,&pose_game,520)==HR_JOY_TUCK);
 pose_game.height=50;pose_game.grounded=true;pose_game.landings=1;pose_game.ground_at=600;
 assert(hr_jump_pose(&animation,&pose_game,600)==HR_JOY_LAND);
 assert(hr_jump_pose(&animation,&pose_game,739)==HR_JOY_LAND);
 assert(hr_jump_pose(&animation,&pose_game,740)==HR_JOY_IDLE);
 pose_game.phase=HR_GAME_CLEAR;assert(hr_jump_pose(&animation,&pose_game,750)==HR_JOY_IDLE);
 animation=(hr_jump_animation_t){0};pose_game=(hr_game_t){.start=0xffffff00u,.phase=HR_GAME_READY,.grounded=true,.height=30};
 assert(hr_jump_pose(&animation,&pose_game,0xffffff00u)==HR_JOY_IDLE);
 pose_game.phase=HR_GAME_RUNNING;pose_game.grounded=false;
 assert(hr_jump_pose(&animation,&pose_game,0xffffff80u)==HR_JOY_PUSH);
 assert(hr_jump_pose(&animation,&pose_game,0xffffffe4u)==HR_JOY_TUCK);

 /* A comfortable baseline must stay low; it must not lift into mid obstacles. */
 hr_game_t low;hr_game_start(&low,0);hr_game_tick(&low,2999,180);
 for(unsigned now=3000;now<4500;now+=20)hr_game_tick(&low,now,180);
 assert(low.height>=30 && low.height<=36 && !low.rescues);
 int16_t x[640];
 for(unsigned f=90;f<=700;f+=37)for(unsigned amp=500;amp<=12000;amp*=4) {
  tone(x,f,amp,1);unsigned hz=hr_pitch_hz(x);
  assert(hz>f*95/100 && hz<f*105/100);
 }
 tone(x,220,500,0);unsigned quiet=hr_pitch_hz(x);tone(x,220,16000,0);
 unsigned loud=hr_pitch_hz(x);assert(abs((int)quiet-(int)loud)<4);
 memset(x,0,sizeof x);assert(!hr_pitch_hz(x));
 for(unsigned i=0;i<640;i++)x[i]=24000;assert(!hr_pitch_hz(x));
 uint32_t rng=123;for(unsigned i=0;i<640;i++){rng=rng*1664525+1013904223;x[i]=(int16_t)(rng>>16)/3;}
 assert(!hr_pitch_hz(x));
 uint8_t stored[16];unsigned score;
 hr_best_encode(3,1234,stored);assert(hr_best_decode(3,stored,&score)&&score==1234);
 assert(!hr_best_decode(2,stored,&score));stored[8]^=1;assert(!hr_best_decode(3,stored,&score));
 hr_game_t g;hr_game_start(&g,1000);hr_game_tick(&g,3999,180);
 assert(g.phase==HR_GAME_READY&&hr_game_seconds(&g,3999)==1);
 hr_game_tick(&g,4000,180);assert(g.phase==HR_GAME_RUNNING&&hr_game_seconds(&g,4000)==0);
 int apex[2]={0};
 for(unsigned pitch=0;pitch<2;pitch++) {
  hr_game_start(&g,0);hr_game_tick(&g,2999,180);
  for(unsigned now=3000;now<3900;now+=20) {
   hr_game_tick(&g,now,pitch?330:150);if(g.height>apex[pitch])apex[pitch]=g.height;
  }
 }
 assert(apex[1]>apex[0]+65);
 hr_game_start(&g,0);hr_game_tick(&g,2999,180);
 for(unsigned now=3000;now<=4900;now+=20) {
  hr_game_tick(&g,now,180);if(now>=3500)assert(abs(g.height-32)<=1);
 }
 hr_game_tick(&g,5000,0);assert(abs(g.height-32)<=1);
 for(unsigned now=5020;now<=5500;now+=20)hr_game_tick(&g,now,0);
 assert(g.height<=30);
 int held[3];const unsigned pitches[3]={180,240,324};
 for(unsigned p=0;p<3;p++) {
  hr_game_start(&g,0);hr_game_tick(&g,2999,180);
  for(unsigned now=3000;now<=4900;now+=20)hr_game_tick(&g,now,pitches[p]);
  held[p]=g.height;
 }
 assert(held[0]==32&&held[1]>held[0]+35&&held[1]<HR_GAME_MAX_HEIGHT&&held[2]==HR_GAME_MAX_HEIGHT);
 hr_game_start(&g,0);hr_game_tick(&g,3000,0);
 g.coins[0].x=55;g.coins[0].height=50;
 hr_game_tick(&g,3020,0);assert(g.score==50&&g.coins_taken==1&&g.coins[0].taken);
 unsigned once=g.score;hr_game_tick(&g,3040,0);assert(g.score==once);
 g.score=200;g.grounded=false;g.height=-60;g.height_q8=-60*256;g.velocity_q8=-256;
 hr_game_tick(&g,3060,0);assert(g.rescues==1&&g.score==150&&g.rescue_kind==1&&g.hit_flash&&g.height==30&&g.progress==0);
 hr_game_start(&g,0);hr_game_tick(&g,3000,0);g.score=200;
 g.obstacles[0].x=g.obstacles[1].x=56;g.obstacles[0].height=g.obstacles[1].height=56;
 hr_game_tick(&g,3020,0);assert(g.rescues==1&&g.score==150&&g.rescue_kind==2);
 hr_game_tick(&g,3040,0);assert(g.rescues==1&&g.score==150);
 hr_best_encode(1,300,stored);memcpy(stored,"HRS5",4);assert(!hr_best_decode(1,stored,&score));
 hr_core_t a,b;uint8_t ma[6]={1},mb[6]={2};
 hr_init(&a,1,ma,100,0);hr_init(&b,2,mb,200,0);a.hardware_ok=b.hardware_ok=true;
 hr_room_enable(&a,0);hr_room_enable(&b,0);hr_room_join(&a,true,100);hr_room_join(&b,true,100);
 hello(&a,&b,200);hello(&b,&a,200);
 uint8_t ids[6];assert(!hr_ranked(&a,200,ids));
 b.best_valid=true;b.best_score=500;hr_packet_t p;assert(hr_score_packet(&b,&p,210));
 uint8_t wire[200];size_t bytes=hr_pack(&p,wire);assert(bytes==36&&hr_unpack(&p,wire,bytes));
 hr_receive(&a,&p,mb,-30,210);assert(hr_ranked(&a,210,ids)==1&&ids[0]==2);
 a.best_valid=true;a.best_score=700;assert(hr_ranked(&a,210,ids)==2&&ids[0]==1);
 b.best_score=500;assert(hr_score_packet(&b,&p,220));p.token=20;hr_receive(&a,&p,mb,-30,220);assert(a.peers[2].best_score==500);
 b.game_busy=true;hello(&b,&a,230);assert(hr_game_peer_busy(&a,2)&&hr_room_member(&a,2,230));
 hr_press(&a,2,1000);assert(a.blocked);hr_release(&a,1001);
 hr_room_join(&b,false,1002);hello(&b,&a,1003);assert(hr_ranked(&a,1003,ids)==1&&!a.peers[2].score_valid);
 uint32_t old=p.request;hr_room_join(&b,true,1004);hello(&b,&a,1005);
 p.seq=++b.seq;p.request=old;hr_receive(&a,&p,mb,-30,1006);assert(!a.peers[2].score_valid);
 assert(hr_score_packet(&b,&p,1007));hr_receive(&a,&p,mb,-30,1007);assert(a.peers[2].score_valid);
 assert(hr_ranked(&a,5000,ids)==1); // Offline peers vanish; own best remains.
 puts("PASS pitch frequency/amplitude/noise, held pitch heights, checkpoint rescue, one-shot coins, CRC records, room score/session/busy/expiry");
}
