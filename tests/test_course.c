#include "hr_game.h"
#include <assert.h>
#include <stdio.h>
#include "voice_route.h"

static void prepared(hr_game_t *g) {
 hr_game_start(g,0);
 for(unsigned t=0;t<3000;t+=20)hr_game_tick(g,t,180);
 hr_game_tick(g,3000,0);
}

static void spike_revival_lands_beyond_hit_ball(void) {
 for(unsigned obstacle=0;obstacle<2;obstacle++) {
  hr_game_t g;prepared(&g);
  unsigned ball_x=g.obstacles[obstacle].x;
  g.progress=ball_x-54;g.height=g.obstacles[obstacle].height-26;
  g.height_q8=g.height*256;g.grounded=false;g.voice_at=0;g.score=200;
  g.coins[0].taken=true;g.coins_taken=1;
  hr_game_tick(&g,3020,0);
  fprintf(stderr,"spike %u revive_x=%u ball_x=%u height=%d\n",obstacle,hr_game_player_x(&g),ball_x,g.height);
  assert(g.rescues==1&&g.rescue_kind==2&&g.score==150);
  assert(hr_game_player_x(&g)>=ball_x+44&&g.grounded);
  assert(g.progress==g.checkpoint&&g.height==g.checkpoint_height);
  unsigned revived=g.progress;
  // Silence after the shield expires must leave the player on safe ground.
  for(unsigned t=3040;t<6040;t+=20)hr_game_tick(&g,t,0);
  assert(g.progress==revived&&g.rescues==1&&g.grounded&&g.score==150);
  assert(g.coins[0].taken&&g.coins_taken==1);
  // A later water fall returns to this new safe checkpoint, not the old ball.
  g.height=-60;g.height_q8=-60*256;g.grounded=false;
  hr_game_tick(&g,6040,0);
  assert(g.progress==revived&&g.rescues==2&&g.score==100);
 }
}

int main(void) {
 spike_revival_lands_beyond_hit_ball();
 hr_game_t g;prepared(&g);
 for(unsigned t=3020;t<120000;t+=20)hr_game_tick(&g,t,0);
 assert(g.phase==HR_GAME_RUNNING && g.progress==0 && g.rescues==0);
 unsigned began=g.progress;
 for(unsigned t=120000;t<121000;t+=20)hr_game_tick(&g,t,180);
 assert(g.progress>began && g.progress<=began+55);
 for(unsigned t=121000;t<121400;t+=20)hr_game_tick(&g,t,0);
 unsigned stopped=g.progress;
 for(unsigned t=121400;t<140000;t+=20)hr_game_tick(&g,t,0);
 assert(g.progress==stopped && g.grounded && !g.rescues);
 // A visible, hand-authored high/low route can collect every coin, with no death.
 prepared(&g);unsigned done=0;
 for(unsigned t=3020;t<40000;t+=20) {
  hr_game_tick(&g,t,reference_voice(hr_game_player_x(&g)));
  if(g.rescues){fprintf(stderr,"route failed x=%u height=%d kind=%u coins=%u\n",hr_game_player_x(&g),g.height,g.rescue_kind,g.coins_taken);break;}
  if(g.phase==HR_GAME_CLEAR){done=t;break;}
 }
 fprintf(stderr,"route coins=%u score=%u progress=%u\n",g.coins_taken,(unsigned)g.score,g.progress);
 assert(done && !g.rescues && g.coins_taken==12 && g.score==900);
 unsigned final=g.score;hr_game_tick(&g,done+1700,800);
 assert(g.phase==HR_GAME_CLEAR && g.score==final);
 hr_game_tick(&g,done+1800,800);assert(g.phase==HR_GAME_DONE);
 hr_game_tick(&g,done+90000,800);assert(g.score==final);
 // Four breath stops extend play beyond 20s without losing the full route.
 prepared(&g);unsigned pause=0,pause_left=0;const unsigned rests[]={155,345,645,845};done=0;
 for(unsigned t=3020;t<60000;t+=20) {
  unsigned x=hr_game_player_x(&g);
  if(pause<4 && !pause_left && x>=rests[pause]){pause++;pause_left=60;}
  unsigned hz=pause_left?0:reference_voice(x);if(pause_left)pause_left--;
  hr_game_tick(&g,t,hz);
  if(g.phase==HR_GAME_CLEAR){done=t;break;}
 }
 assert(done>27000 && g.coins_taken==12 && !g.rescues && g.score==900);
 // Both obstacle lanes have an achievable, sustained-voice route without pixel precision.
 prepared(&g);g.progress=430;g.height=32;g.height_q8=32*256;g.grounded=false;
 for(unsigned t=3020;t<4520;t+=20)hr_game_tick(&g,t,180);
 assert(hr_game_player_x(&g)>550 && !g.rescues && g.height<=36);
 prepared(&g);g.progress=706;g.height=102;g.height_q8=102*256;g.grounded=false;
 for(unsigned t=3020;t<4320;t+=20)hr_game_tick(&g,t,270);
 assert(hr_game_player_x(&g)>820 && !g.rescues);
 // Waiting on a saved footing is unlimited, and water falls return to that footing.
 prepared(&g);g.progress=g.checkpoint=240;g.checkpoint_height=56;g.height=56;g.height_q8=56*256;
 for(unsigned t=3020;t<60000;t+=20)hr_game_tick(&g,t,0);
 assert(g.progress==240 && g.grounded && !g.rescues);
 g.progress=370;g.height=-60;g.height_q8=-60*256;g.grounded=false;g.score=200;
 hr_game_tick(&g,60020,0);
 assert(g.progress==240 && g.rescues==1 && g.score==150 && g.grounded);
 unsigned rewound=g.progress;hr_game_tick(&g,60040,0);assert(g.progress==rewound && g.score==150);
 // The bright high coin refuses a middle voice even at its exact coordinates.
 prepared(&g);g.progress=280;g.height=124;g.height_q8=124*256;
 g.coins[3].x=335;g.coins[3].height=150;
 hr_game_tick(&g,3020,180);assert(!g.coins[3].taken);
 g.height=124;g.height_q8=124*256;hr_game_tick(&g,3040,330);
 assert(g.coins[3].taken && g.reward_points==100);
 unsigned collected=g.score;hr_game_tick(&g,3060,330);assert(g.score==collected);
 // USB/UI stalls may catch up at most five steps, never teleport the level.
 prepared(&g);hr_game_tick(&g,90000,180);assert(g.progress<=5 && g.phase==HR_GAME_RUNNING);
 // Timers remain valid over uint32 wrap, including the actual finish celebration.
 hr_game_start(&g,UINT32_MAX-1000);hr_game_tick(&g,1999,180);
 assert(g.phase==HR_GAME_RUNNING && g.progress==1);
 puts("PASS voice-only progress, indefinite silence, and safe breathing stops");
}
