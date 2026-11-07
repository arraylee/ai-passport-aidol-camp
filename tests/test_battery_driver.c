#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../components/bsp/src/bsp_battery.c"
static uint8_t regs[256];static uint32_t now,last_restart;
static unsigned io,writes,profile_writes,reads_since_write,delays,adds,removes;
static bool present=true,corrupt_verify,drop_flag;
static int fail_profile=-1;
esp_err_t bsp_i2c_init(void){return ESP_OK;}
i2c_master_bus_handle_t bsp_i2c_bus(void){return (void*)1;}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t b,const i2c_device_config_t*c,i2c_master_dev_handle_t*d){assert(b==(void*)1&&c->device_address==0x63);*d=(void*)2;adds++;return 0;}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t d){assert(d==(void*)2);removes++;return 0;}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d,const uint8_t*r,size_t rn,uint8_t*b,size_t n,int ms){
 assert(d==(void*)2&&rn==1&&ms<=5);io++;if(!present)return ESP_FAIL;
 memcpy(b,regs+*r,n);
 if(*r>=0x10&&*r<0x60){reads_since_write++;if(corrupt_verify&&gauge.phase==VERIFY_PROFILE&&gauge.index==25){b[0]^=1;corrupt_verify=false;}}
 return 0;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d,const uint8_t*b,size_t n,int ms){
 assert(d==(void*)2&&n==2&&ms<=5);io++;if(!present)return ESP_FAIL;
 if(b[0]>=0x10&&b[0]<0x60){
  assert(regs[8]==0xf0 && !(regs[11]&0x80));
  if(fail_profile==b[0]-0x10){fail_profile=-1;return ESP_FAIL;}
  profile_writes++;reads_since_write=0;
 }
 if(b[0]==8){if(b[1]==0x30)last_restart=now;else assert((uint32_t)(now-last_restart)>=20);}
 if(b[0]==11&&(b[1]&0x80)){
  assert(!memcmp(regs+0x10,s_battery_profile,80)&&reads_since_write>=80);
  if(drop_flag){drop_flag=false;return ESP_OK;}
 }
 regs[b[0]]=b[1];writes++;return 0;
}
void vTaskDelay(uint32_t ms){delays++;now+=ms;}
uint32_t xTaskGetTickCount(void){return now;}
static void reset(bool blank){
 s_dev=NULL;memset(&gauge,0,sizeof gauge);gauge.raw_soc=-1;
 if(blank){memset(regs,0,sizeof regs);regs[0]=0x0f;regs[4]=254;regs[5]=232;regs[11]=0x14;}
 now=last_restart=io=writes=profile_writes=reads_since_write=delays=adds=removes=0;
 present=true;corrupt_verify=drop_flag=false;fail_profile=-1;
}
static bool drive(unsigned steps){
 for(unsigned i=0;i<steps;i++){
  unsigned before_io=io,before_delay=delays;
  esp_err_t e=bsp_battery_service(now);
  assert(io-before_io<=1&&delays==before_delay);now+=5;
  if(e==ESP_OK)return true;
 }
 return false;
}
static void fixed(void){assert(!memcmp(regs+0x10,s_battery_profile,80));assert(regs[11]==0x94&&regs[8]==0&&gauge.prepared);}
int main(void){
 reset(true);assert(bsp_battery_soc()==-1);assert(drive(1000));fixed();assert(profile_writes==80);
 assert(bsp_battery_soc()==-1);regs[4]=76;assert(bsp_battery_soc()==76);regs[4]=0;assert(bsp_battery_soc()==0);regs[4]=101;assert(bsp_battery_soc()==-1);
 unsigned old_writes=writes;assert(drive(1)&&writes==old_writes);
 // Reboots preserve a committed, different device calibration, including its SOC.
 reset(true);for(unsigned i=0;i<80;i++)regs[0x10+i]=(uint8_t)(0x55^i);regs[11]=0x94;regs[4]=64;
 uint8_t original[80];memcpy(original,regs+0x10,80);assert(drive(200));assert(!writes&&!memcmp(original,regs+0x10,80)&&bsp_battery_soc()==64);
 // Persistent invalid SOC restarts calculation but preserves a valid calibration.
 bsp_battery_recover();assert(drive(300));assert(writes==2&&!profile_writes&&!memcmp(original,regs+0x10,80));
 // A sleeping gauge only wakes; no profile rewrite.
 reset(false);regs[8]=0xf0;assert(drive(300));assert(!profile_writes&&writes==2&&regs[8]==0);
 // A known verified table with a missing commit flag needs no80-byte rewrite.
 reset(true);memcpy(regs+0x10,s_battery_profile,80);assert(drive(500));fixed();assert(!profile_writes);
 // Erased profile with a stale update flag is cleared before any profile write.
 reset(true);memset(regs+0x10,0xff,80);regs[11]=0x94;assert(drive(1000));fixed();assert(profile_writes==80);
 // Unknown uncommitted calibration is not guessed or overwritten.
 reset(true);memset(regs+0x10,0x77,80);assert(!drive(2000));assert(!writes&&bsp_battery_soc()==-1);
 // Missing chip reconnects without reboot; probe handles are released.
 reset(true);present=false;assert(!drive(200));assert(adds==1&&removes==1&&!writes);present=true;assert(drive(1000));fixed();
 reset(true);regs[0]=0xff;assert(!drive(250));assert(!writes&&s_dev==NULL);
 // Interrupted sequential initialization resumes after an MCU reboot.
 const unsigned cuts[]={1,8,33,79};
 for(unsigned j=0;j<4;j++){
  reset(true);while(profile_writes<cuts[j]){assert(!drive(1));}
  assert(!(regs[11]&0x80));reset(false);assert(drive(1000));fixed();
 }
 reset(true);fail_profile=33;assert(drive(1800));fixed();assert(profile_writes>80);
 reset(true);corrupt_verify=true;assert(drive(1800));fixed();
 reset(true);drop_flag=true;assert(drive(1800));fixed();
 reset(true);now=0xffffff00u;assert(drive(1000));fixed();
 // Sleep aborts a partial transaction; recovery still verifies before publishing.
 reset(true);while(profile_writes<20)assert(!drive(1));assert(bsp_battery_sleep()==0);assert(bsp_battery_wake()==0);assert(drive(1000));fixed();
 puts("PASS actual CW2017: blank/erased repair, preserve calibration, bounded I/O, commit/readback, power-cut retry, failure/reconnect, wake and rollover");
}
