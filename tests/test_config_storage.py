"""Run the real reset engine and production partition adapter against 8MB Flash."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'main/hr_platform.c').read_text();start=s.index('bool hr_storage_config_erase(');end=s.index('{',start)+1;depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
fixture=r'''
#include "hr_configure.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_PARTITION_TYPE_DATA 1
typedef struct {unsigned address,size;} esp_partition_t;
static unsigned char flash[0x800000];
static esp_partition_t parts[]={{0x380000,0x30000},{0x3b0000,0x30000},{0x400000,0xc0000}};
static const char*names[]={"profile_store","friend_store","gallery_store"};
static const esp_partition_t*esp_partition_find_first(int type,int sub,const char*name){assert(type==1&&sub>=0x41&&sub<=0x43);assert(!strcmp(names[sub-0x41],name));return &parts[sub-0x41];}
static int esp_partition_erase_range(const esp_partition_t*p,unsigned at,unsigned n){assert(n==4096&&at+n<=p->size);memset(flash+p->address+at,255,n);return 0;}
static bool mark(uint8_t r,uint8_t a,uint32_t t){return r==2&&a<=2&&t==7;}
static bool save(uint8_t r){return r==2;}
static bool clear(void){return true;}
'''
main=r'''
int main(void){
 assert(!hr_storage_config_erase(3,0)&&!hr_storage_config_erase(0,1)&&!hr_storage_config_erase(0,0x30000));
 unsigned original=parts[0].address;parts[0].address=0x356000;assert(!hr_storage_config_erase(0,0));parts[0].address=original;
 parts[2].size--;assert(!hr_storage_config_erase(2,0));parts[2].size++;
 hr_config_ops_t ops={mark,hr_storage_config_erase,save,clear};
 for(unsigned action=1;action<=2;action++){
  memset(flash,0x55,sizeof flash);hr_config_t state={0};hr_config_recover(&state,2,action,7);
  while(state.state==HR_CONFIG_ERASING)hr_config_tick(&state,&ops);
  assert(hr_config_finish(&state,&ops,true));
  for(unsigned i=0;i<sizeof flash;i++){
   bool erased=(i>=0x3b0000&&i<0x3e0000)||(i>=0x400000&&i<0x4c0000)||(action==1&&i>=0x380000&&i<0x3b0000);
   assert(flash[i]==(erased?255:0x55));
  }
 }
 puts("PASS production exact partition adapter: full8MB untouched outside selected demo data");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(fixture+s[start:end]+main);out=Path(td)/'test'
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root/'main'),str(p),str(root/'main/hr_configure.c'),'-o',str(out)],check=True)
 subprocess.run([str(out)],check=True)
