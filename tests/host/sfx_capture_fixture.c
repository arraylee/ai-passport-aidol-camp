#include "hr_core.h"
#include "hr_sfx.h"
#include "hr_welcome.h"
#include <stdlib.h>
#include <assert.h>
#include <setjmp.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
enum {ESP_OK=0,pdTRUE=1};
#define ESP_LOGI(...) ((void)0)
typedef unsigned TickType_t;
const uint8_t hr_welcome_data[]={0};const size_t hr_welcome_size=1;
#define pdMS_TO_TICKS(x) (x)
typedef struct {uint8_t data[HR_AUDIO];uint32_t gate,at;} audio_t;
static atomic_uint tx_gate,audio_errors,game_hz,game_hz_at,game_mic_stack,game_rms,game_raw_hz,mic_peak,capture_count,drops;
static atomic_bool game_capture=true,welcome_done;
static atomic_uint welcome_request,rx_gate,volume=90,play_count;
static hr_sfx_bus_t game_sound;
static int16_t game_pcm[640];static void *tx_audio,*rx_audio;
static unsigned output_frames,output_nonzero,output_peak,cancel_at,volume_calls;
static unsigned frame,events[6],radio_enqueues;static uint32_t fake_now;
static hr_game_t game;static hr_sfx_player_t speaker;static hr_sfx_watch_t watch;
static jmp_buf stopped;
static uint32_t ms(void){return fake_now;}
static unsigned uxTaskGetStackHighWaterMark(void *p){(void)p;return 2440;}
static void audio_barrier(unsigned bit){assert(bit==1||bit==2);}
static void vTaskDelay(unsigned n){(void)n;assert(!"unexpected read failure");}
static int xQueueSend(void *queue,const void *data,unsigned timeout){(void)queue;(void)data;(void)timeout;radio_enqueues++;return pdTRUE;}
static int bsp_audio_read(void *out,size_t bytes) {
 assert(bytes==HR_SAMPLES*2);
 if(frame==300)longjmp(stopped,1);
 fake_now=frame++*20;
 if(fake_now<4600)assert(!atomic_load(&game_hz));
 unsigned hz=hr_sfx_guarded(&game_sound,fake_now)?0:atomic_load(&game_hz);
 hr_game_tick(&game,fake_now,hz);
 unsigned cue=hr_sfx_event(&watch,&game,true,fake_now);
 if(cue){events[cue]++;hr_sfx_request(&game_sound,cue,fake_now);}
 /* Include the stronger event cues as actual microphone feedback too. */
 if(fake_now==400)hr_sfx_request(&game_sound,HR_SFX_DEATH,fake_now);
 if(fake_now==1400)hr_sfx_request(&game_sound,HR_SFX_WIN,fake_now);
 int16_t *mic=out;
 hr_sfx_frame(&game_sound,&speaker,mic,HR_SAMPLES,fake_now);
 /* Before 3.3s the only microphone input is the actual speaker's cue PCM. */
 if(fake_now>=3400)
  for(unsigned i=0;i<HR_SAMPLES;i++)mic[i]=(int16_t)((fake_now<4600?200:750)*sin((frame*HR_SAMPLES+i)*180*6.28318530718/16000));
 return ESP_OK;
}
static unsigned uxQueueMessagesWaiting(void *q){(void)q;return 0;}
static int xQueueReceive(void *q,void *data,unsigned wait){(void)q;(void)data;(void)wait;return 0;}
static TickType_t xTaskGetTickCount(void){return fake_now;}
static void bsp_audio_set_volume(unsigned v){assert(v==90);volume_calls++;}
static int bsp_audio_write(const void *data,size_t bytes) {
 assert(bytes==HR_SAMPLES*2);const int16_t *pcm=data;bool nonzero=false;
 for(unsigned i=0;i<HR_SAMPLES;i++){
  unsigned a=abs(pcm[i]);assert(a<=12000);if(a>output_peak)output_peak=a;
  if(pcm[i])nonzero=true;
 }
 if(nonzero)output_nonzero++;
 if(cancel_at && output_frames>=cancel_at)assert(!nonzero);
 output_frames++;return ESP_OK;
}
static void vTaskDelayUntil(TickType_t *wake,unsigned delay) {
 *wake+=delay;fake_now+=delay;
 if(cancel_at && output_frames==cancel_at)hr_sfx_enable(&game_sound,false,fake_now);
 if(output_frames==40)longjmp(stopped,1);
}
#include "capture.inc"
#include "playback.inc"

int main(void) {
 hr_game_start(&game,0);hr_sfx_enable(&game_sound,true,0);
 if(!setjmp(stopped))capture_task(NULL);
 assert(events[HR_SFX_THREE]==1&&events[HR_SFX_TWO]==1&&events[HR_SFX_ONE]==1);
 assert(game.base_hz==180&&game.phase==HR_GAME_RUNNING&&game.progress>40&&game.progress<80);
 assert(atomic_load(&game_hz)>175&&atomic_load(&game_hz)<185);
 assert(!radio_enqueues&&!atomic_load(&audio_errors)&&!atomic_load(&drops));
 for(unsigned kind=HR_SFX_DEATH;kind<=HR_SFX_WIN;kind++)for(unsigned pass=0;pass<2;pass++) {
  output_frames=output_nonzero=output_peak=volume_calls=0;cancel_at=pass?5:0;fake_now=10000;
  hr_sfx_enable(&game_sound,false,fake_now);hr_sfx_enable(&game_sound,true,fake_now);
  hr_sfx_request(&game_sound,kind,fake_now);
  if(!setjmp(stopped))playback_task(NULL);
  assert(volume_calls==1&&output_nonzero==(pass?5:hr_sfx_samples(kind)/HR_SAMPLES));
  assert(output_peak>=(kind==HR_SFX_DEATH?11000:9500));
 }
 puts("PASS production speaker worker: stronger death/victory PCM during game, volume 90, stop-on-exit");
 puts("PASS production microphone worker: real cue PCM never becomes pitch/movement, history flush, live voice resumes, no PTT leak");
}
