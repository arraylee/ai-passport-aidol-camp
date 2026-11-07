#include "hr_sfx.h"
#include <string.h>
/* Original sine cues streamed into the existing 16 kHz speaker buffer. */
static const int16_t sine32[32]={0,6393,12539,18204,23170,27245,30273,32137,32767,32137,30273,27245,23170,18204,12539,6393,0,-6393,-12539,-18204,-23170,-27245,-30273,-32137,-32767,-32137,-30273,-27245,-23170,-18204,-12539,-6393};
unsigned hr_sfx_samples(unsigned kind) {
 const uint16_t count[]={0,1120,1120,1600,2880,8000};
 return kind<=HR_SFX_WIN?count[kind]:0;
}
void hr_sfx_begin(hr_sfx_player_t *p,unsigned kind) {
 *p=(hr_sfx_player_t){.kind=hr_sfx_samples(kind)?kind:HR_SFX_NONE};
}
bool hr_sfx_render(hr_sfx_player_t *p,int16_t *pcm,size_t n) {
 memset(pcm,0,n*sizeof *pcm);unsigned total=hr_sfx_samples(p->kind);
 if(!total)return false;
 for(size_t i=0;i<n && p->position<total;i++) {
  unsigned pos=p->position++,hz,local=pos,length=total;
  if(p->kind<=HR_SFX_ONE)hz=p->kind==HR_SFX_THREE?660:p->kind==HR_SFX_TWO?784:988;
  /* Keep the fall in the badge speaker's midrange, rather than sub-bass. */
  else if(p->kind==HR_SFX_DEATH)hz=1200-650*pos/total;
  else {
   unsigned note=pos/1600;if(note>3)note=3;
   const uint16_t notes[]={523,659,784,1046};hz=notes[note];
   local=pos-note*1600;length=note==3?3200:1440;
   if(local>=length)continue;
  }
  unsigned envelope=256;
  if(local<80)envelope=local*256/80;
  if(length-1-local<160 && (length-1-local)*256/160<envelope)envelope=(length-1-local)*256/160;
  p->phase+=(uint16_t)(hz*65536u/16000u);
  unsigned at=p->phase>>11,fraction=p->phase&2047u;
  int wave=(sine32[at]*(int)(2048-fraction)+sine32[(at+1)&31]*(int)fraction)/2048;
  int gain=p->kind==HR_SFX_DEATH?12000:10000;
  pcm[i]=(int16_t)((wave*gain/32767)*(int)envelope/256);
 }
 if(p->position>=total)p->kind=HR_SFX_NONE;
 return true;
}
unsigned hr_sfx_event(hr_sfx_watch_t *w,const hr_game_t *g,bool enabled,uint32_t now) {
 if(!enabled){memset(w,0,sizeof *w);return HR_SFX_NONE;}
 if(!w->active||w->start!=g->start)*w=(hr_sfx_watch_t){.active=true,.start=g->start};
 if(g->phase>=HR_GAME_CLEAR) {
  if(!w->won){w->won=true;return HR_SFX_WIN;}
  return HR_SFX_NONE;
 }
 if(g->rescues && (w->collision!=g->collision_at || w->rescues!=g->rescues)) {
  w->collision=g->collision_at;w->rescues=g->rescues;return HR_SFX_DEATH;
 }
 unsigned count=hr_game_seconds(g,now);
 if(g->phase==HR_GAME_READY && count && count<=3 && count!=w->count) {
  w->count=count;return 4-count;
 }
 return HR_SFX_NONE;
}
bool hr_sfx_guarded(const hr_sfx_bus_t *b,uint32_t now) {
 return atomic_load(&b->playing) || (uint32_t)(now-atomic_load(&b->guard_at))<atomic_load(&b->guard_ms);
}
void hr_sfx_enable(hr_sfx_bus_t *b,bool enabled,uint32_t now) {
 bool previous=atomic_exchange(&b->enabled,enabled);
 if(previous && !enabled) {
  bool tail=hr_sfx_guarded(b,now);
  atomic_store(&b->pending,0);atomic_store(&b->playing,false);
  atomic_store(&b->guard_at,now);atomic_store(&b->guard_ms,tail?200:0);
 }
}
void hr_sfx_request(hr_sfx_bus_t *b,unsigned kind,uint32_t now) {
 if(!atomic_load(&b->enabled)||!hr_sfx_samples(kind))return;
 atomic_store(&b->guard_at,now);atomic_store(&b->guard_ms,hr_sfx_samples(kind)/16+240);
 atomic_store(&b->pending,kind);
}
bool hr_sfx_frame(hr_sfx_bus_t *b,hr_sfx_player_t *p,int16_t *pcm,size_t n,uint32_t now) {
 if(!atomic_load(&b->enabled)) {
  hr_sfx_begin(p,HR_SFX_NONE);atomic_store(&b->playing,false);memset(pcm,0,n*sizeof *pcm);return false;
 }
 unsigned cue=atomic_exchange(&b->pending,0);if(cue)hr_sfx_begin(p,cue);
 bool active=hr_sfx_render(p,pcm,n);
 if(!atomic_load(&b->enabled)){memset(pcm,0,n*sizeof *pcm);active=false;hr_sfx_begin(p,0);}
 atomic_store(&b->playing,active);
 if(active){atomic_store(&b->guard_at,now);atomic_store(&b->guard_ms,200);}
 return active;
}
