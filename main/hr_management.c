#include "hr_management.h"
#include "hr_configure.h"
#include "hr_controls.h"
#include "hr_platform.h"
#include "hr_profile_store.h"
#include "hr_friend_store.h"
#include "hr_cards.h"
#include "hr_preview.h"
#include "hr_gallery.h"
#include "hr_identity.h"
#include "bsp_display.h"
#include "esp_app_desc.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static hr_config_t config;
static const hr_config_ops_t ops={hr_storage_config_mark,hr_storage_config_erase,hr_storage_config_reset,hr_storage_config_clear};
static atomic_uint nonce;
static uint32_t boot,reset_count;
static char sn[25];
static uint8_t snapshot[HR_MANAGEMENT_BYTES];
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static bool prepared;
static void put32(uint8_t*p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=x>>(i*8);}
void hr_management_init(const char*serial){
 snprintf(sn,sizeof sn,"%s",serial);boot=esp_random();if(!boot)boot=1;
 uint8_t role,action;uint32_t txn;
 if(hr_storage_config_pending(&role,&action,&txn))hr_config_recover(&config,role,action,txn);
}
void hr_management_connection(bool connected){uint32_t n=connected?esp_random():0;atomic_store(&nonce,connected?(n?n:1):0);}
bool hr_management_busy(void){return config.state==HR_CONFIG_ERASING||config.state==HR_CONFIG_APPLY||(config.state==HR_CONFIG_ERROR&&config.txn);}
void hr_management_read(uint8_t out[HR_MANAGEMENT_BYTES]){portENTER_CRITICAL(&mux);memcpy(out,snapshot,sizeof snapshot);portEXIT_CRITICAL(&mux);put32(out+216,atomic_load(&nonce));}
unsigned hr_management_command(hr_view_t*v,const uint8_t*p,size_t n){
 if(n==18&&p[1]==2&&p[2]!=v->core.id)return 0x14;
 if(hr_profile_upload_busy()||hr_gallery_busy()||!hr_audio_paused())return 0x1b;
 uint32_t previous=config.txn;
 unsigned s=hr_config_begin(&config,&ops,v->core.mac,atomic_load(&nonce),p,n);
 if(!s&&previous!=config.txn)prepared=false;
 return s;
}
static bool apply(hr_view_t*v){
 uint8_t mac[6];memcpy(mac,v->core.mac,6);
 hr_view_t fresh={.radio_ok=v->radio_ok,.audio_ok=v->audio_ok,.buttons_ok=v->buttons_ok,.phone_connected=v->phone_connected,.volume=90,.choice=config.role,.activation_required=true,.now=v->now,.page=HR_PAGE_HOME};
 hr_init(&fresh.core,config.role,mac,esp_random(),v->now);hr_identity_enable(&fresh.core);hr_identity_set_sn(&fresh.core,sn);hr_room_enable(&fresh.core,v->now);
 *v=fresh;hr_view_home(v,v->now);hr_profile_store_select(config.role);hr_profile_remote_reset(config.role);hr_friend_store_bind(&v->core);
 bool ok=hr_cards_init(config.role)&&hr_gallery_init();hr_audio_volume(90);return ok;
}
static void publish(const hr_view_t*v){
 uint8_t b[HR_MANAGEMENT_BYTES]={0};memcpy(b,"HBC1",4);b[4]=1;b[5]=v->core.id;b[6]=v->volume;b[7]=!v->activation_required;
 memcpy(b+8,v->core.mac,6);snprintf((char*)b+14,25,"%s",sn);snprintf((char*)b+39,16,"%.15s",esp_app_get_description()->version);
 const hr_edit_t*p=hr_profile_current();
 if(p){memcpy(b+55,p->name,49);memcpy(b+104,p->intro,85);put32(b+201,p->revision);put32(b+205,p->avatar?p->image_crc:0);}
 put32(b+189,(uint32_t)(esp_timer_get_time()/1000));put32(b+193,boot);put32(b+197,reset_count);put32(b+209,config.txn);
 b[213]=config.state;b[214]=hr_config_percent(&config);b[215]=config.error;put32(b+216,atomic_load(&nonce));b[220]=v->core.social.friends;b[221]=v->core.best_valid;put32(b+222,v->core.best_score);b[226]=hr_gallery_image_descriptor()!=NULL;b[227]=hr_storage_welcomed();
 portENTER_CRITICAL(&mux);memcpy(snapshot,b,sizeof b);portEXIT_CRITICAL(&mux);
}
bool hr_management_poll(hr_view_t*v){
 bool applied=false;
 if(config.state==HR_CONFIG_ERASING&&bsp_lvgl_lock(5)){
  if(!prepared){
   hr_cards_reset();hr_preview_reset();hr_profile_abort();hr_gallery_detach();
   if(config.action==1)hr_profile_store_detach();
   v->page=HR_PAGE_HOME;v->pair_preview=NULL;v->intro_kind=0;v->best_dirty=false;v->core.held=false;v->core.speaker=0;
   v->activation_required=true;memset(&v->core.social,0,sizeof v->core.social);hr_ui_render(v);prepared=true;
  }
  hr_config_tick(&config,&ops);bsp_lvgl_unlock();
 }
 if(config.state==HR_CONFIG_APPLY&&bsp_lvgl_lock(5)){
  applied=apply(v);hr_config_finish(&config,&ops,applied);if(config.state==HR_CONFIG_DONE)reset_count++;
  hr_ui_render(v);bsp_lvgl_unlock();
 }
 static uint32_t at;static unsigned state=~0u;
 if((uint32_t)(v->now-at)>=100||state!=config.state){publish(v);at=v->now;state=config.state;}return applied;
}
