#include "hr_sfx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
 hr_game_t game;hr_game_start(&game,1000);hr_sfx_watch_t w={0};
 assert(hr_sfx_event(&w,&game,true,1000)==HR_SFX_THREE);
 assert(hr_sfx_event(&w,&game,true,1001)==HR_SFX_NONE);
 assert(hr_sfx_event(&w,&game,true,2000)==HR_SFX_TWO);
 assert(hr_sfx_event(&w,&game,true,3000)==HR_SFX_ONE);
 game.phase=HR_GAME_RUNNING;assert(!hr_sfx_event(&w,&game,true,4000));
 game.rescues=1;game.collision_at=4500;
 assert(hr_sfx_event(&w,&game,true,4500)==HR_SFX_DEATH);
 assert(!hr_sfx_event(&w,&game,true,4501));
 game.rescues=255;game.collision_at=5000;assert(hr_sfx_event(&w,&game,true,5000)==HR_SFX_DEATH);
 game.collision_at=6000;assert(hr_sfx_event(&w,&game,true,6000)==HR_SFX_DEATH);
 game.phase=HR_GAME_CLEAR;assert(hr_sfx_event(&w,&game,true,7000)==HR_SFX_WIN);
 assert(!hr_sfx_event(&w,&game,true,7001));
 assert(!hr_sfx_event(&w,&game,false,7010));hr_game_start(&game,7010);
 assert(hr_sfx_event(&w,&game,true,7010)==HR_SFX_THREE);
 int16_t pcm[320];
 for(unsigned cue=HR_SFX_THREE;cue<=HR_SFX_WIN;cue++) {
  hr_sfx_player_t p={0};hr_sfx_begin(&p,cue);unsigned frames=0,peak=0,crossings=0;int previous=0;
  while(hr_sfx_render(&p,pcm,320)) {
   for(unsigned i=0;i<320;i++) {
    unsigned sample=frames*320+i,a=abs(pcm[i]);if(a>peak)peak=a;
    assert(a<=12000);assert(abs(pcm[i]-previous)<6000);
    if(cue==HR_SFX_DEATH && sample>=160 && sample<800 && previous<=0 && pcm[i]>0)crossings++;
    previous=pcm[i];
   }
   assert(++frames<=25);
  }
  assert(peak>2000 && peak<=12000);
  /* A tiny speaker needs a midrange death cue and stronger event levels.
     Keep headroom: global playback remains 90%, no digital full-scale clips. */
  if(cue==HR_SFX_DEATH){assert(peak>=11000);assert(crossings>=30);}
  if(cue==HR_SFX_WIN)assert(peak>=9500);
  if(cue<=HR_SFX_ONE)assert(peak>=9500);
  assert(frames==(hr_sfx_samples(cue)+319)/320);
  for(unsigned i=0;i<320;i++)assert(pcm[i]==0);
 }
 hr_sfx_bus_t bus={0};hr_sfx_player_t p={0};hr_sfx_enable(&bus,true,1000);
 hr_sfx_request(&bus,HR_SFX_WIN,1000);assert(hr_sfx_guarded(&bus,1000));
 assert(hr_sfx_frame(&bus,&p,pcm,320,1000));
 hr_sfx_enable(&bus,false,1010);assert(!hr_sfx_frame(&bus,&p,pcm,320,1020));
 for(unsigned i=0;i<320;i++)assert(!pcm[i]);
 assert(hr_sfx_guarded(&bus,1100)&&!hr_sfx_guarded(&bus,1300));
 hr_sfx_enable(&bus,true,1400);assert(!hr_sfx_frame(&bus,&p,pcm,320,1400));
 hr_sfx_request(&bus,HR_SFX_THREE,1500);hr_sfx_request(&bus,HR_SFX_DEATH,1501);
 assert(hr_sfx_frame(&bus,&p,pcm,320,1520)&&p.kind==HR_SFX_DEATH);
 hr_sfx_enable(&bus,false,1600);hr_sfx_enable(&bus,true,UINT32_MAX-100);
 hr_sfx_request(&bus,HR_SFX_ONE,UINT32_MAX-100);
 assert(hr_sfx_guarded(&bus,10)&&!hr_sfx_guarded(&bus,500));
 puts("PASS one-shot countdown/death/victory, bounded/enveloped PCM, cancellation/latest cue, echo guard and clock wrap");
}
