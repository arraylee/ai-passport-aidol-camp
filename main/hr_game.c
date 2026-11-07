#include "hr_game.h"
#include "hr_core.h"
#include <string.h>
#include <stdlib.h>
/* Fixed course; only voiced steps advance. Silence never wins the game. */
const hr_game_platform_t hr_game_platforms[HR_GAME_PLATFORMS]={
 { -24,209,30 }, {235,120,56}, {405,120,18},
 {575,120,74}, {745,120,36}, {915,230,50}
};
static void points(hr_game_t*g,unsigned n) {g->score=g->score+n>HR_SCORE_MAX?HR_SCORE_MAX:g->score+n;}
unsigned hr_game_player_x(const hr_game_t*g){return 54+g->progress;}
void hr_game_start(hr_game_t *g,uint32_t now) {
 memset(g,0,sizeof *g);g->start=now;g->step_at=now;g->base_hz=180;
 g->height=30;g->height_q8=30*256;g->grounded=true;g->ground_at=now;
 g->checkpoint_height=30;
 g->obstacles[0]=(hr_obstacle_t){.x=525,.height=146};
 g->obstacles[1]=(hr_obstacle_t){.x=790,.height=84};
 /* Four sparkling high notes need >=165% baseline, not simply louder sound. */
 const hr_coin_t route[HR_GAME_COINS]={
  {.x=125,.height=64}, {.x=185,.height=106}, {.x=285,.height=126},
  {.x=335,.height=150,.high=true}, {.x=445,.height=132}, {.x=495,.height=90},
  {.x=615,.height=120}, {.x=665,.height=150,.high=true},
  {.x=785,.height=150,.high=true}, {.x=835,.height=90},
  {.x=985,.height=150,.high=true}, {.x=1035,.height=86}
 };
 memcpy(g->coins,route,sizeof route);
}
unsigned hr_game_seconds(const hr_game_t*g,uint32_t now) {
 uint32_t elapsed=now-g->start;
 return elapsed<HR_GAME_PREP_MS?(HR_GAME_PREP_MS-elapsed+999)/1000:0;
}
static void rescue(hr_game_t*g,uint32_t now,unsigned kind) {
 g->score=g->score>HR_GAME_DEATH_PENALTY?g->score-HR_GAME_DEATH_PENALTY:0;
 g->rescue_kind=kind;g->combo=0;if(g->rescues<255)g->rescues++;
 g->collision_at=now;g->rescue_until=now+1100;
 /* Water/wood collisions retry the footing. A spike collision first moves
  * the checkpoint beyond that ball so repeated singing cannot trap a player. */
 g->progress=g->checkpoint;g->height=g->checkpoint_height;
 g->height_q8=g->height*256;g->velocity_q8=0;g->grounded=true;
 g->voice_at=0;g->voice_hz=0;
 for(unsigned i=0;i<HR_GAME_COINS;i++)
  if(!g->coins[i].taken && g->coins[i].x>(int)hr_game_player_x(g)-26)g->coins[i].missed=false;
}
static void beyond_spike(hr_game_t *g,const hr_obstacle_t *o) {
 // Keep the entire 56 px dog clear of the ball, then land inside a platform.
 int clear=o->x+48;
 for(unsigned i=0;i<HR_GAME_PLATFORMS;i++) {
  const hr_game_platform_t *p=&hr_game_platforms[i];
  int x=p->x+16;if(x<clear)x=clear;
  if(x>p->x+p->width-16 || x<54 || x>54+HR_GAME_COURSE)continue;
  g->checkpoint=(uint16_t)(x-54);g->checkpoint_height=p->height;
  return;
 }
}
void hr_game_tick(hr_game_t*g,uint32_t now,unsigned hz) {
 if(g->phase==HR_GAME_DONE)return;
 if(g->phase==HR_GAME_CLEAR) {
  if((uint32_t)(now-g->clear_at)>=HR_GAME_CLEAR_MS)g->phase=HR_GAME_DONE;
  return;
 }
 if(hz<70||hz>800)hz=0;
 g->pitch_hz=hz;
 if(hz){g->voice_hz=hz;g->voice_at=now;}
 if((uint32_t)(now-g->start)<HR_GAME_PREP_MS) {
  if(hz)g->base_hz=(3*g->base_hz+hz)/4;
  g->step_at=now;return;
 }
 if(g->phase==HR_GAME_READY) {
  g->step_at=now-20;g->voice_at=hz?now:0;g->voice_hz=hz;
 }
 if((uint32_t)(now-g->step_at)>100)g->step_at=now-100;
 g->phase=HR_GAME_RUNNING;
 unsigned steps=(now-g->step_at)/20;
 unsigned age=now-g->voice_at;
 unsigned held_hz=hz?hz:g->voice_at && age<240?g->voice_hz:0;
 bool walking=hz || (g->voice_at && age<100);
 for(unsigned tick=0;tick<steps;tick++) {
  g->step_at+=20;uint32_t at=g->step_at;
  if(walking && g->progress<HR_GAME_COURSE)g->progress++;
  int x=(int)hr_game_player_x(g),old_height=g->height,support=-100;
  for(unsigned i=0;i<HR_GAME_PLATFORMS;i++) {
   const hr_game_platform_t*p=&hr_game_platforms[i];
   if(x+10>=p->x && x-10<p->x+p->width && g->height>=p->height-3) {
    support=p->height;break;
   }
  }
  if(g->grounded && support<0){g->grounded=false;g->velocity_q8=0;}
  bool was_grounded=g->grounded;
  if(held_hz) {
   int ratio=(int)held_hz*100/g->base_hz;
   // Baseline stays low; a deliberate rise reaches the high-note coins.
   int target=ratio<=100?(ratio-70)*32/30:32+(ratio-100)*92/65;
   if(target<0)target=0;
   if(target>HR_GAME_MAX_HEIGHT)target=HR_GAME_MAX_HEIGHT;
   int delta=target-g->height;if(delta>7)delta=7;if(delta< -5)delta=-5;
   g->height+=delta;g->height_q8=g->height*256;g->velocity_q8=0;
   g->grounded=false;
  } else {
   g->velocity_q8-=128;g->height_q8+=g->velocity_q8;g->height=g->height_q8/256;
  }
  if(support>=0 && old_height>=support && g->height<=support) {
   g->height=support;g->height_q8=support*256;g->velocity_q8=0;g->grounded=true;
   if(!was_grounded){g->ground_at=at;g->landings++;}
   g->checkpoint=g->progress;g->checkpoint_height=support;
  }
  bool protected=(int32_t)(g->rescue_until-at)>0,failed=false;
  if(g->height< -30 && !protected){rescue(g,at,1);failed=true;}
  for(unsigned i=0;i<2 && !failed;i++) {
   hr_obstacle_t*o=&g->obstacles[i];
   if(!protected && !o->hit && abs(x-o->x)<22 && abs(g->height+26-o->height)<25) {
    beyond_spike(g,o);
    rescue(g,at,2);failed=true;
   }
  }
  for(unsigned i=0;i<HR_GAME_PLATFORMS && !failed;i++) {
   const hr_game_platform_t*p=&hr_game_platforms[i];int width=p->width*45/100;if(width>82)width=82;
   int center=p->x+p->width/2;
   if(!protected && abs(x-center)<width/2+12 && g->height<p->height-6) {
    rescue(g,at,3);failed=true;
   }
  }
  if(failed){held_hz=0;walking=false;continue;}
  for(unsigned i=0;i<HR_GAME_COINS;i++) {
   hr_coin_t*c=&g->coins[i];
   bool pitch_ok=!c->high || (held_hz && held_hz*100>=g->base_hz*165u);
   if(!c->taken && pitch_ok && abs(x-c->x)<18 && abs(g->height+26-c->height)<(c->high?10:24)) {
    c->taken=true;g->coins_taken++;if(g->combo<20)g->combo++;
    unsigned reward=c->high?100:50;points(g,reward);g->reward_at=at;g->reward_points=reward;
   }
   if(!c->taken && !c->missed && x-c->x>26){c->missed=true;g->combo=0;}
  }
  if(g->progress>=HR_GAME_COURSE) {
   g->phase=HR_GAME_CLEAR;g->clear_at=at;g->height=50;g->height_q8=50*256;
   g->grounded=true;g->velocity_q8=0;points(g,100);g->hit_flash=false;break;
  }
 }
 if(g->phase==HR_GAME_RUNNING)g->hit_flash=g->collision_at && (uint32_t)(now-g->collision_at)<1000;
}
unsigned hr_pitch_hz(const int16_t pcm[640]) {
 /* YIN-style normalized difference, 4kHz box-filtered input, lags 5..57.
  * No FFT, heap, floating point, or amplitude-to-pitch substitution. */
 int16_t x[160];uint32_t power=0;
 for(unsigned i=0;i<160;i++) {
  int v=((int)pcm[i*4]+pcm[i*4+1]+pcm[i*4+2]+pcm[i*4+3])/4;
  x[i]=v/16;power+=(int32_t)x[i]*x[i];
 }
 if(power/160<81)return 0; // ~144 PCM RMS; ignore silence/background hiss.
 uint32_t diff[59]={0};uint64_t sum=0;
 for(unsigned lag=1;lag<=58;lag++) {
  uint32_t d=0;for(unsigned j=0;j<100;j++){int v=x[j]-x[j+lag];d+=(uint32_t)(v*v);}
  sum+=d;diff[lag]=sum?(uint32_t)((uint64_t)d*lag*1024/sum):1024;
 }
 for(unsigned lag=5;lag<=57;lag++) {
  if(diff[lag]<205 && diff[lag]<=diff[lag-1] && diff[lag]<diff[lag+1]) {
   // Quadratic interpolation of the normalized valley, in Q8 periods.
   int den=2*((int)diff[lag-1]+(int)diff[lag+1]-2*(int)diff[lag]);
   int offset=den?((int)diff[lag-1]-(int)diff[lag+1])*256/den:0;
   if(offset>128){offset=128;}
   if(offset< -128){offset=-128;}
   return 4000u*256/(lag*256+offset);
  }
 }
 return 0;
}
unsigned hr_voice_hz(hr_voice_gate_t *gate,const int16_t pcm[640]) {
 int64_t sum=0;uint64_t squares=0;
 for(unsigned i=0;i<640;i++){int v=pcm[i];sum+=v;squares+=(int64_t)v*v;}
 uint32_t energy=(uint32_t)((squares-(uint64_t)(sum*sum/640))/640),root=0,bit=1u<<30;
 while(bit>energy)bit>>=2;
 while(bit){if(energy>=root+bit){energy-=root+bit;root=(root>>1)+bit;}else root>>=1;bit>>=2;}
 gate->level=(uint16_t)root;
 unsigned hz=root>=180?hr_pitch_hz(pcm):0;gate->candidate_hz=hz;
 if(root<(gate->open?180u:300u)){gate->strong=0;gate->open=false;return 0;}
 if(!hz){gate->strong=0;gate->open=false;return 0;}
 if(gate->strong<2)gate->strong++;
 gate->open=gate->strong>=2;
 return gate->open?hz:0;
}
void hr_best_encode(unsigned id,unsigned score,uint8_t out[16]) {
 memset(out,0,16);memcpy(out,"HRS6",4);out[4]=id;
 for(unsigned i=0;i<4;i++)out[8+i]=score>>(8*i);
 uint32_t crc=hr_crc(out,12);for(unsigned i=0;i<4;i++)out[12+i]=crc>>(8*i);
}
bool hr_best_decode(unsigned id,const uint8_t in[16],unsigned *score) {
 uint32_t s=0,crc=0;for(unsigned i=0;i<4;i++){s|=(uint32_t)in[8+i]<<(8*i);crc|=(uint32_t)in[12+i]<<(8*i);}
 if(id<1||id>HR_PLAYERS||memcmp(in,"HRS6",4)||in[4]!=id||in[5]||in[6]||in[7]||s>HR_SCORE_MAX||hr_crc(in,12)!=crc)return false;
 *score=s;return true;
}
