"""Production NVS settings: initialize 90 once, preserve later volume choices."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'main/hr_platform.c').read_text()
def function(name):
 start=s.index('bool '+name+'(');end=s.index('{',start)+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
fixture=r'''
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef int nvs_handle_t;typedef int esp_err_t;
enum{HR_PLAYERS=6,ESP_OK=0,NVS_READONLY=1,NVS_READWRITE=2};
static bool storage_ready=true,open_fail,blob_fail,marker_fail,commit_fail;
static uint8_t blob[12],pending[12],marker,staged_marker;
static unsigned commits;
static int nvs_open_from_partition(const char*p,const char*n,int m,int*h){
 assert(!strcmp(p,"demo_cfg")&&!strcmp(n,"hideseek"));(void)m;*h=1;
 memcpy(pending,blob,12);staged_marker=marker;return open_fail?-1:0;
}
static void nvs_close(int h){assert(h==1);}
static int nvs_get_u8(int h,const char*k,uint8_t*v){assert(h==1&&!strcmp(k,"volume90_v1"));*v=marker;return marker?0:-1;}
static int nvs_set_u8(int h,const char*k,uint8_t v){assert(h==1&&!strcmp(k,"volume90_v1")&&v==1);staged_marker=v;return marker_fail?-1:0;}
static int nvs_set_blob(int h,const char*k,const void*p,size_t n){assert(h==1&&!strcmp(k,"settings")&&n==12);memcpy(pending,p,n);return blob_fail?-1:0;}
static int nvs_commit(int h){assert(h==1);commits++;if(commit_fail)return -1;memcpy(blob,pending,12);marker=staged_marker;return 0;}
static void hr_config_encode(uint8_t id,uint8_t v,uint8_t out[12]){memset(out,0,12);out[0]=id;out[1]=v;}
'''
main=r'''
int main(void){
 uint8_t volume=60;blob[0]=2;blob[1]=60;
 open_fail=true;assert(!hr_storage_apply_volume_default(2,&volume));open_fail=false;
 commit_fail=true;assert(!hr_storage_apply_volume_default(2,&volume));assert(volume==60&&blob[1]==60&&!marker);commit_fail=false;
 marker_fail=true;unsigned before=commits;assert(!hr_storage_apply_volume_default(2,&volume)&&commits==before&&!marker);marker_fail=false;
 blob_fail=true;assert(!hr_storage_apply_volume_default(2,&volume)&&commits==before);blob_fail=false;
 assert(hr_storage_apply_volume_default(2,&volume)&&volume==90&&blob[0]==2&&blob[1]==90&&marker==1);
 assert(hr_storage_save(2,35));volume=35;before=commits;
 assert(hr_storage_apply_volume_default(2,&volume)&&volume==35&&commits==before);
 storage_ready=false;assert(!hr_storage_apply_volume_default(2,&volume));
 puts("PASS 90 percent initialization, atomic marker, failed commit retry, later user volume preserved");
}
'''
body=function('hr_storage_save')+'\n'+function('hr_storage_apply_volume_default')
with tempfile.TemporaryDirectory(prefix='hr-volume-default-') as td:
 p=Path(td);(p/'test.c').write_text(fixture+body+main)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
