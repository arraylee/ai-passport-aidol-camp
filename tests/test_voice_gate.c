#include "hr_game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static int16_t pcm[640];
static void tone(unsigned hz,unsigned amplitude,int dc) {
 for(unsigned i=0;i<640;i++)pcm[i]=(int16_t)(dc+amplitude*sin(i*hz*6.28318530718/16000));
}
int main(void) {
 hr_voice_gate_t gate={0};hr_game_t game;hr_game_start(&game,0);
 tone(150,200,0);
 for(unsigned t=0;t<8000;t+=40) {
  unsigned hz=hr_voice_hz(&gate,pcm);assert(!hz && !gate.open);
  hr_game_tick(&game,t,hz);
 }
 assert(game.base_hz==180 && game.progress==0 && !game.rescues);
 // An isolated loud syllable cannot open; sustained close humming can.
 tone(180,750,0);assert(!hr_voice_hz(&gate,pcm));
 tone(180,0,0);assert(!hr_voice_hz(&gate,pcm));
 tone(180,750,0);assert(!hr_voice_hz(&gate,pcm));
 unsigned hz=hr_voice_hz(&gate,pcm);assert(hz>=175 && hz<=185 && gate.open);
 assert(gate.level>500 && gate.level<560);
 // Hysteresis permits a modest dip in an already established voice.
 tone(180,350,0);assert(hr_voice_hz(&gate,pcm));
 tone(180,200,0);assert(!hr_voice_hz(&gate,pcm) && !gate.open);
 tone(180,350,0);assert(!hr_voice_hz(&gate,pcm));
 assert(gate.candidate_hz>=175 && gate.candidate_hz<=185);
 // DC bias must not turn quiet speech into a close, loud voice.
 for(int dc=-24000;dc<=24000;dc+=24000) {
  tone(220,200,dc);assert(!hr_voice_hz(&gate,pcm));
  assert(gate.level>130 && gate.level<150);
 }
 uint32_t rng=123;
 for(unsigned w=0;w<20;w++) {
  for(unsigned i=0;i<640;i++){rng=rng*1664525+1013904223;pcm[i]=(int16_t)(rng>>16)/3;}
  assert(!hr_voice_hz(&gate,pcm));
 }
 // Loud enough real input still calibrates the owner, rather than background.
 hr_game_start(&game,0);gate=(hr_voice_gate_t){0};
 for(unsigned t=0;t<3000;t+=40) {
  tone(145,t<1600?200:750,0);hr_game_tick(&game,t,hr_voice_hz(&gate,pcm));
 }
 assert(game.base_hz>=144 && game.base_hz<=150);
 puts("PASS close-voice gate: weak background cannot calibrate/move, two-window onset, hysteresis, DC rejection, noise and owner calibration");
}
