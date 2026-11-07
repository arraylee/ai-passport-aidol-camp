#include "hr_power.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "esp_sleep.h"
#include <assert.h>
#include <stdio.h>
static unsigned light,stops,resumes,sleeps;
static bool ready,locked,button_prepared,panel_asleep,battery_asleep,held;
static int fail;
static int soc = -1;
static unsigned soc_reads, services, recoveries;
static bool gauge_ready=true;
esp_err_t bsp_battery_service(uint32_t now){(void)now;services++;return gauge_ready?0:-1;}
void bsp_battery_recover(void){recoveries++;}
int bsp_battery_soc(void){soc_reads++;return soc;}
void bsp_display_backlight(uint8_t v){light=v;}
void bsp_lvgl_arm_first_frame(void){ready=false;}
bool bsp_lvgl_first_frame_ready(void){return ready;}
bool bsp_lvgl_suspend(int t){assert(t>0&&t<=100);if(fail==2)return false;locked=true;return true;}
void bsp_lvgl_resume(void){locked=false;}
esp_err_t bsp_display_sleep(void){assert(locked);panel_asleep=true;return fail==4?-1:0;}
esp_err_t bsp_display_wake(void){panel_asleep=false;return 0;}
void bsp_display_sleep_hold(void){held=true;}
void bsp_display_sleep_unhold(void){held=false;}
bool bsp_button_prepare_sleep(void){button_prepared=true;return fail!=1;}
void bsp_button_cancel_sleep(void){button_prepared=false;}
esp_err_t bsp_battery_probe(void){return 0;}
esp_err_t bsp_battery_sleep(void){battery_asleep=true;return 0;}
esp_err_t bsp_battery_wake(void){battery_asleep=false;return 0;}
esp_err_t esp_deep_sleep_enable_gpio_wakeup(uint64_t pins,int level){assert(pins==1&&level==0);return fail==5?-1:0;}
esp_err_t esp_sleep_disable_wakeup_source(int source){assert(source==7);return 0;}
esp_err_t esp_deep_sleep_try_to_start(void){assert(locked&&button_prepared&&panel_asleep&&held&&light==0&&stops);sleeps++;return -1;}
static bool stop(void*c){assert(c==(void*)123);stops++;return fail!=3;}
static void resume(void*c){assert(c==(void*)123);resumes++;}
static void init(uint32_t now){light=99;stops=resumes=sleeps=soc_reads=0;soc=-1;services=recoveries=0;gauge_ready=true;ready=locked=button_prepared=panel_asleep=battery_asleep=held=false;fail=0;hr_power_init(now,stop,resume,(void*)123);}
static void reveal(uint32_t now){hr_power_first_frame(now);hr_power_tick(now,false,false);assert(light==0);ready=true;hr_power_tick(now,false,false);hr_power_tick(now+150,false,false);assert(light>0&&light<80);hr_power_tick(now+300,false,false);assert(light==80);}
static void sleep_at(uint32_t now) {
 uint32_t elapsed;
 hr_power_tick(now,false,false);assert(!stops&&hr_power_goodbye(now,&elapsed)&&elapsed==0&&light==80);
 hr_power_tick(now+1199,false,false);assert(!stops&&light==80);
 hr_power_tick(now+1200,false,false);assert(!stops&&light==80);
 hr_power_tick(now+1350,false,false);assert(light>0&&light<80);
 hr_power_tick(now+1500,false,false);
}
int main(void){
 init(0);assert(hr_power_battery()==-1&&soc_reads==1);reveal(0);
 soc=63;hr_power_tick(999,false,false);assert(hr_power_battery()==-1&&soc_reads==1);
 hr_power_tick(1000,false,false);assert(hr_power_battery()==63&&soc_reads==2);
 soc=-1;hr_power_tick(6000,false,false);assert(hr_power_battery()==63);
 hr_power_tick(7000,false,true);assert(hr_power_battery()==63);
 hr_power_tick(8000,true,false);assert(recoveries==2&&hr_power_battery()==63);
 hr_power_tick(9000,false,false);assert(recoveries==2);
 soc=0;hr_power_tick(10000,false,true);assert(hr_power_battery()==0);
 soc=101;hr_power_tick(15000,false,false);assert(hr_power_battery()==0);
 soc=100;hr_power_tick(16000,true,false);assert(hr_power_battery()==100);
 soc=20;hr_power_tick(21000,false,true);assert(hr_power_battery()==20);
 // Temporary service failure uses the cache; stale after exactly 30 seconds.
 gauge_ready=false;unsigned n=soc_reads,service_n=services;
 hr_power_tick(50999,false,true);assert(hr_power_battery()==20&&soc_reads==n&&services>service_n);
 hr_power_tick(51000,false,true);assert(hr_power_battery()==-1&&soc_reads==n);
 gauge_ready=true;soc=72;hr_power_tick(51001,false,true);assert(hr_power_battery()==72);
 // A failed/pending startup recovers during intro even before its first frame.
 init(0);gauge_ready=false;hr_power_init(0,stop,resume,(void*)123);
 n=soc_reads;hr_power_tick(1500,false,true);assert(soc_reads==n&&hr_power_battery()==-1);
 gauge_ready=true;soc=87;hr_power_tick(1505,false,true);assert(hr_power_battery()==87);
 // Sustained invalid SOC rechecks configuration at most every 30 seconds.
 init(0);hr_power_tick(1000,false,false);hr_power_tick(2000,false,false);
 assert(recoveries==2);hr_power_tick(3000,false,false);hr_power_tick(31000,false,false);
 assert(recoveries==2);hr_power_tick(32000,false,false);assert(recoveries==3);
 init(0xffffff00u);reveal(0xffffff00u);soc=50;
 hr_power_tick(0xffffff00u+1000u,false,false);assert(hr_power_battery()==50&&soc_reads==2);
 gauge_ready=false;hr_power_tick(0xffffff00u+31000u,false,false);assert(hr_power_battery()==-1);
 init(0);assert(light==0);hr_power_first_frame(0);hr_power_tick(5000,false,false);assert(light==0&&!stops);init(0);reveal(0);
 assert(!hr_power_filter_input(0,1));assert(!hr_power_filter_input(604,800));assert(!hr_power_filter_input(-1,900));assert(!hr_power_filter_input(2900,1000));assert(!hr_power_filter_input(2900,1029));assert(hr_power_filter_input(2900,1030));
 uint32_t elapsed;hr_power_tick(600000,false,false);assert(!hr_power_goodbye(600000,&elapsed));
 hr_power_tick(900000,false,false);assert(!hr_power_goodbye(900000,&elapsed));
 hr_power_tick(1199999,false,false);assert(!stops&&light==80);
 sleep_at(1200000);assert(stops==1&&sleeps==1&&resumes==1&&!locked&&!panel_asleep&&!button_prepared&&!held&&!battery_asleep);
 hr_power_tick(1201800,false,false);assert(light==80);
 init(0);reveal(0);hr_power_tick(700000,false,true);hr_power_tick(1899999,false,false);assert(!stops);sleep_at(1900000);assert(stops==1);
 // Both a key and a received audio/busy event cancel the visible JOY outro.
 for(unsigned busy=0;busy<2;busy++) {
  uint32_t elapsed;init(0);reveal(0);hr_power_tick(1200000,false,false);
  assert(hr_power_goodbye(1200010,&elapsed));hr_power_tick(1200100,!busy,busy);
  assert(!hr_power_goodbye(1200100,&elapsed)&&!stops&&light==80);
 }
 // Cancellation during the fade restores light and never starts sleep.
 init(0);reveal(0);hr_power_tick(1200000,false,false);hr_power_tick(1201200,false,false);
 hr_power_tick(1201300,true,false);hr_power_tick(1201600,false,false);assert(!stops&&light==80);
 init(0xffffff00u);reveal(0xffffff00u);sleep_at(0xffffff00u+1200000u);assert(stops==1);
 for(int f=1;f<=5;f++){
  init(0);reveal(0);fail=f;sleep_at(1200000);
  assert(!locked&&!button_prepared&&!panel_asleep&&!battery_asleep&&!held&&!sleeps);
  assert(resumes==(f==3||f==4));hr_power_tick(1201800,false,false);assert(light==80);
 }
 puts("PASS JOY pre-sleep timing, cancellation, first-frame fade, 20min idle, wake consumption, rollover and shutdown rollback");
}
