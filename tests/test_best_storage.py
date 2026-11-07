"""Exercise production NVS record functions, including failed commits and CRC."""
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'main/hr_platform.c').read_text()
def function(name):
 a=s.index('bool '+name+'(');b=s.index('{',a)+1;depth=1
 while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
fixture=r'''
#include "hr_game.h"
#include "hr_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int nvs_handle_t;typedef int esp_err_t;
enum{ESP_OK=0,NVS_READONLY=1,NVS_READWRITE=2};
static bool storage_ready=true,have,fail_set,fail_commit;
static uint8_t saved[16],staged[16];
static int nvs_open_from_partition(const char *p,const char*n,int mode,int*h){assert(!strcmp(p,"demo_cfg")&&!strcmp(n,"hideseek"));(void)mode;*h=1;return 0;}
static int nvs_get_blob(int h,const char*k,void*b,size_t*n){assert(h==1&&!strcmp(k,"best_v1")&&*n==16);if(!have)return -1;memcpy(b,saved,16);return 0;}
static int nvs_set_blob(int h,const char*k,const void*b,size_t n){assert(h==1&&!strcmp(k,"best_v1")&&n==16);memcpy(staged,b,16);return fail_set?-1:0;}
static int nvs_commit(int h){assert(h==1);if(fail_commit)return -1;memcpy(saved,staged,16);have=true;return 0;}
static void nvs_close(int h){assert(h==1);}
'''
main=r'''
int main(void){
 uint32_t value;assert(!hr_storage_best_load(2,&value));
 assert(hr_storage_best_save(2,0));assert(hr_storage_best_load(2,&value)&&value==0);
 fail_set=true;assert(!hr_storage_best_save(2,200));fail_set=false;
 assert(hr_storage_best_load(2,&value)&&value==0);
 fail_commit=true;assert(!hr_storage_best_save(2,200));fail_commit=false;
 assert(hr_storage_best_load(2,&value)&&value==0);
 assert(hr_storage_best_save(2,200)&&hr_storage_best_load(2,&value)&&value==200);
 assert(!hr_storage_best_load(3,&value));saved[8]^=1;assert(!hr_storage_best_load(2,&value));
 assert(!hr_storage_best_save(2,HR_SCORE_MAX+1));
 storage_ready=false;assert(!hr_storage_best_save(2,500));
 puts("PASS best score: absent, zero, atomic failed writes/commit retry, CRC, owner binding");
}
'''
with tempfile.TemporaryDirectory(prefix='hr-best-') as td:
 p=Path(td);(p/'test.c').write_text(fixture+function('hr_storage_best_load')+'\n'+function('hr_storage_best_save')+main)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I',str(root/'main'),str(p/'test.c'),str(root/'main/hr_game.c'),str(root/'main/hr_core.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
