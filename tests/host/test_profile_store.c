#include "hr_profile_store.h"
#include "esp_partition.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char flash[0x30000];
static const esp_partition_t partition={0x380000,sizeof flash};
static bool session=true;static int fail=-1;
bool hr_profile_session_valid(void){return session;}
const esp_partition_t *esp_partition_find_first(int type,int sub,const char *name){assert(type==1&&sub==0x41&&!strcmp(name,"profile_store"));return &partition;}
int esp_partition_mmap(const esp_partition_t*p,unsigned offset,unsigned size,int type,const void **out,esp_partition_mmap_handle_t*h){(void)h;assert(p==&partition&&!offset&&size==sizeof flash&&!type);*out=flash;return 0;}
int esp_partition_write(const esp_partition_t*p,unsigned offset,const void *bytes,size_t n){assert(p==&partition&&offset+n<=sizeof flash);if(fail==0)return -1;if(fail>0)fail--;const uint8_t *b=bytes;for(size_t i=0;i<n;i++){assert((flash[offset+i]&b[i])==b[i]);flash[offset+i]&=b[i];}return 0;}
int esp_partition_erase_range(const esp_partition_t*p,unsigned offset,unsigned n){assert(p==&partition&&offset+n<=sizeof flash&&!(offset%4096)&&!(n%4096));if(fail==0)return -1;if(fail>0)fail--;memset(flash+offset,255,n);return 0;}
static unsigned config(const char*s){return hr_profile_config((const uint8_t*)s,strlen(s));}
static unsigned upload(const uint8_t *data,size_t n){uint8_t begin[]={0,n&255,(n>>8)&255,(n>>16)&255,(n>>24)&255};unsigned r=hr_profile_image(begin,5,100);if(r)return r;for(size_t at=0;at<n;at+=255){size_t len=n-at<255?n-at:255;uint8_t p[256]={1};memcpy(p+1,data+at,len);r=hr_profile_image(p,len+1,101);if(r)return r;}uint8_t end=2;return hr_profile_image(&end,1,102);}
int main(int argc,char**argv){assert(argc==2);lv_init();memset(flash,255,sizeof flash);assert(hr_profile_store_init(1));assert(config("{\"nickname\":\"直播小猫\",\"title\":\"开发者\",\"intro\":\"你好世界\"}")==1);assert(!strcmp(hr_profile_current()->name,"直播小猫"));assert(!strcmp(hr_display_title(1),"开发者"));assert(hr_profile_store_init(1));assert(!strcmp(hr_profile_current()->intro,"你好世界"));
 fail=1;assert(config("{\"nickname\":\"不能提交\"}")==0x16);fail=-1;assert(hr_profile_store_init(1));assert(!strcmp(hr_profile_current()->name,"直播小猫"));
 uint8_t begin[]={0,100,0,0,0};assert(hr_profile_image(begin,5,100)==0);assert(config("{\"img_mode\":\"avatar\"}")==0x1b);hr_profile_expire(10101);assert(config("{\"img_mode\":\"avatar\"}")==1);
 assert(hr_profile_image(begin,5,20000)==0);uint8_t end=2;assert(hr_profile_image(&end,1,20001)==0x1a);assert(!hr_profile_current()->avatar);
 FILE*f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);uint8_t *png=malloc(n);assert(fread(png,1,n,f)==n);fclose(f);assert(upload(png,n)==1);assert(hr_profile_current()->avatar);uint32_t crc=hr_profile_current()->image_crc;assert(crc);printf("PRESET_IMAGE_CRC=%u\n",crc);assert(hr_profile_store_init(1));assert(hr_profile_current()->avatar&&hr_profile_current()->image_crc==crc);
 png[60]^=1;assert(upload(png,n)==0x17);assert(hr_profile_store_init(1));assert(hr_profile_current()->avatar&&hr_profile_current()->image_crc==crc);png[60]^=1;
 session=false;assert(upload(png,n)==0x16);session=true;assert(hr_profile_store_init(1));assert(hr_profile_current()->image_crc==crc);
 assert(config("{\"nickname\":\"自定义喵喵\"}")==1);assert(hr_profile_store_init(1));assert(!strcmp(hr_profile_current()->name,"自定义喵喵")&&hr_profile_current()->image_crc==crc);
 for(unsigned offset=0;offset<224;offset++){uint8_t saved[sizeof flash];memcpy(saved,flash,sizeof flash);unsigned bank=hr_profile_current()->revision%2?0:1;flash[bank*65536+offset]^=1;assert(hr_profile_store_init(1));assert(hr_profile_current()->id==1);memcpy(flash,saved,sizeof flash);assert(hr_profile_store_init(1));}
 free(png);puts("PASS profile atomic banks, commit failure, timeout retry, disconnect abort, PNG CRC, reboot, image preservation");}
