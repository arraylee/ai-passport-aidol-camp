"""A new welcome generation replays once without changing activation."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'main/hr_platform.c').read_text()
def f(name):
 a=s.index('bool '+name+'(');b=s.index('{',a)+1;d=1
 while d:d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
prefix=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef int nvs_handle_t;typedef int esp_err_t;
enum {ESP_OK=0,NVS_READONLY=1,NVS_READWRITE=2};
static bool storage_ready=true,failed,exists;static uint8_t marker;
static int nvs_open_from_partition(const char*p,const char*n,int m,int*h){assert(!strcmp(p,"demo_cfg")&&!strcmp(n,"hideseek"));(void)m;*h=3;return 0;}
static void nvs_close(int h){assert(h==3);}
static int nvs_get_u8(int h,const char*k,uint8_t*v){assert(h==3);if(!strcmp(k,"activated_v1")||!strcmp(k,"welcome_v1")){*v=1;return 0;}assert(!strcmp(k,"welcome_v2"));*v=marker;return exists?0:-1;}
static int nvs_set_u8(int h,const char*k,uint8_t v){assert(h==3&&!strcmp(k,"welcome_v2")&&v==1);return 0;}
static int nvs_commit(int h){assert(h==3);if(failed)return -1;marker=1;exists=true;return 0;}
'''
main=r'''
int main(void){
 assert(!hr_storage_welcomed());failed=true;assert(!hr_storage_welcome_save()&&!hr_storage_welcomed());
 failed=false;assert(hr_storage_welcome_save()&&hr_storage_welcomed());
 uint8_t active=0;assert(!nvs_get_u8(3,"activated_v1",&active)&&active==1);
 storage_ready=false;assert(!hr_storage_welcomed()&&!hr_storage_welcome_save());
 puts("PASS old welcome record does not skip upgrade welcome; new record persists; activation untouched");
}
'''
with tempfile.TemporaryDirectory(prefix='hr-welcome-storage-') as td:
 p=Path(td);(p/'test.c').write_text(prefix+f('hr_storage_welcomed')+f('hr_storage_welcome_save')+main)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
