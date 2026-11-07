#include "hr_configure.h"
#include <string.h>
static uint32_t get32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
void hr_config_recover(hr_config_t*s,uint8_t role,uint8_t action,uint32_t txn){
 *s=(hr_config_t){.role=role,.action=action,.txn=txn,.region=action==2?1:0,.state=HR_CONFIG_ERASING};
 if(role<1||role>6||action<1||action>2||!txn){s->state=HR_CONFIG_ERROR;s->error=0x14;}
}
unsigned hr_config_begin(hr_config_t*s,const hr_config_ops_t*o,const uint8_t mac[6],uint32_t nonce,const uint8_t*p,size_t n){
 if(n!=18||p[0]!=1||p[1]<1||p[1]>2||p[2]<1||p[2]>6||p[3]||!nonce||get32(p+4)!=nonce||!get32(p+8)||memcmp(p+12,mac,6))return 0x14;
 uint32_t txn=get32(p+8);
 if(s->state!=HR_CONFIG_IDLE&&txn==s->txn){
  if(p[2]!=s->role||p[1]!=s->action)return 0x14;
  return s->state==HR_CONFIG_DONE?1:s->state==HR_CONFIG_ERROR?s->error:0;
 }
 if(s->state==HR_CONFIG_ERASING||s->state==HR_CONFIG_APPLY)return 0x1b;
 if(!o->mark(p[2],p[1],txn))return 0x16;
 hr_config_recover(s,p[2],p[1],txn);return 0;
}
void hr_config_tick(hr_config_t*s,const hr_config_ops_t*o){
 if(s->state!=HR_CONFIG_ERASING)return;
 if(s->region<3){
  if(!o->erase(s->region,s->offset)){s->state=HR_CONFIG_ERROR;s->error=0x16;return;}
  s->offset+=4096;s->completed++;
  if(s->offset==(s->region==2?0xc0000u:0x30000u)){s->region++;s->offset=0;}
 }else if(o->settings(s->role))s->state=HR_CONFIG_APPLY;
 else{s->state=HR_CONFIG_ERROR;s->error=0x16;}
}
bool hr_config_finish(hr_config_t*s,const hr_config_ops_t*o,bool applied){
 if(s->state!=HR_CONFIG_APPLY)return false;
 if(!applied||!o->clear()){s->state=HR_CONFIG_ERROR;s->error=0x16;return false;}
 s->state=HR_CONFIG_DONE;return true;
}
unsigned hr_config_percent(const hr_config_t*s){unsigned all=s->action==2?240:288;return s->state==HR_CONFIG_DONE?100:s->completed*99/all;}
