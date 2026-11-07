#include "hr_configure.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned erases,settings,marks,clears;static int fail_at=-1;static uint8_t role_saved;static uint32_t txn_saved;
static bool mark(uint8_t role,uint8_t action,uint32_t txn){assert(action==1||action==2);marks++;role_saved=role;txn_saved=txn;return true;}
static bool erase(unsigned region,uint32_t at){assert(region<3);assert(!(at%4096));assert(at<(region==2?0xc0000u:0x30000u));return (int)erases++!=fail_at;}
static bool save(uint8_t role){settings++;return role==role_saved;}
static bool clear(void){clears++;return true;}
static const hr_config_ops_t ops={mark,erase,save,clear};
static uint8_t mac[6]={2,3,4,5,6,7};
static void req(uint8_t p[18],unsigned role,unsigned txn){memset(p,0,18);p[0]=p[1]=1;p[2]=role;p[4]=42;p[8]=txn;memcpy(p+12,mac,6);}
static void finish(hr_config_t*s){while(s->state==HR_CONFIG_ERASING)hr_config_tick(s,&ops);assert(s->state==HR_CONFIG_APPLY);assert(hr_config_finish(s,&ops,true));assert(s->state==HR_CONFIG_DONE);}
int main(void){hr_config_t s={0};uint8_t p[18];req(p,2,9);
 p[12]^=1;assert(hr_config_begin(&s,&ops,mac,42,p,18)==0x14);p[12]^=1;
 assert(hr_config_begin(&s,&ops,mac,41,p,18)==0x14);p[2]=0;assert(hr_config_begin(&s,&ops,mac,42,p,18)==0x14);p[2]=2;
 assert(!marks&&!erases);assert(hr_config_begin(&s,&ops,mac,42,p,18)==0);assert(marks==1&&txn_saved==9);
 assert(hr_config_begin(&s,&ops,mac,42,p,18)==0&&marks==1);p[8]=10;assert(hr_config_begin(&s,&ops,mac,42,p,18)==0x1b);p[8]=9;
 finish(&s);assert(erases==288&&settings==1&&clears==1);assert(hr_config_begin(&s,&ops,mac,42,p,18)==1&&erases==288);
 p[2]=3;assert(hr_config_begin(&s,&ops,mac,42,p,18)==0x14);
 req(p,3,10);assert(hr_config_begin(&s,&ops,mac,42,p,18)==0);fail_at=erases;hr_config_tick(&s,&ops);assert(s.state==HR_CONFIG_ERROR&&clears==1);fail_at=-1;
 hr_config_recover(&s,3,1,10);finish(&s);assert(settings==2&&clears==2);
 hr_config_recover(&s,7,1,11);assert(s.state==HR_CONFIG_ERROR);
 req(p,4,12);p[1]=2;erases=0;assert(hr_config_begin(&s,&ops,mac,42,p,18)==0);assert(s.region==1);finish(&s);assert(erases==240);
 puts("PASS guards, duplicate transactions, exact erase limits, failure, power-loss recovery");return 0;}
