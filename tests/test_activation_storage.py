"""Exercise production activation persistence against fail-closed NVS stubs."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
s=(root/'main/hr_platform.c').read_text()
parts=[]
for name in ('hr_storage_activated','hr_storage_activation_save'):
 start=s.index('bool '+name+'(');end=s.index('{',start)+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 parts.append(s[start:end])
fixture=r'''
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef int nvs_handle_t;
typedef int esp_err_t;
enum {ESP_OK=0,NVS_READONLY=1,NVS_READWRITE=2};
static bool storage_ready,exists,open_failed,set_failed,commit_failed;
static uint8_t committed,staged;
static unsigned commits,closes;
static int nvs_open_from_partition(const char*p,const char*n,int mode,int*h){
 assert(!strcmp(p,"demo_cfg")&&!strcmp(n,"hideseek"));
 assert(mode==NVS_READONLY||mode==NVS_READWRITE);*h=7;return open_failed?-1:0;
}
static int nvs_get_u8(int h,const char*k,uint8_t*v){
 assert(h==7);if(!strcmp(k,"welcome_v1")){*v=1;return 0;}
 assert(!strcmp(k,"activated_v1"));if(!exists)return -1;*v=committed;return 0;
}
static int nvs_set_u8(int h,const char*k,uint8_t v){
 assert(h==7&&!strcmp(k,"activated_v1")&&v==1);staged=v;return set_failed?-1:0;
}
static int nvs_commit(int h){assert(h==7);commits++;if(commit_failed)return -1;exists=true;committed=staged;return 0;}
static void nvs_close(int h){assert(h==7);closes++;}
'''
main=r'''
int main(void){
 assert(!hr_storage_activated()&&!hr_storage_activation_save());
 storage_ready=true;
 assert(!hr_storage_activated()); /* Old welcome marker does not activate. */
 commit_failed=true;assert(!hr_storage_activation_save()&&!hr_storage_activated());
 commit_failed=false;assert(hr_storage_activation_save()&&hr_storage_activated());
 /* A fresh reader after the writer closes sees the committed flag. */
 assert(closes>=5);assert(hr_storage_activated());
 committed=2;assert(!hr_storage_activated());
 open_failed=true;assert(!hr_storage_activation_save()&&!hr_storage_activated());
 open_failed=false;set_failed=true;unsigned before=commits;
 assert(!hr_storage_activation_save()&&commits==before);
 puts("PASS activation: separate welcome marker, atomic commit, reboot reader, corrupt value and write failures");
}
'''
with tempfile.TemporaryDirectory(prefix='hr-activation-test-') as td:
 p=Path(td);(p/'test.c').write_text(fixture+'\n'.join(parts)+main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
