"""Exercise the production single-commit MAC roster/friend-mask record."""
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1]
source=(root/'main/hr_platform.c').read_text()
def function(name):
 a=source.index('bool '+name+'(');b=source.index('{',a)+1;depth=1
 while depth:depth+=(source[b]=='{')-(source[b]=='}');b+=1
 return source[a:b]
fixture=r'''
#include "hr_identity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int nvs_handle_t;typedef int esp_err_t;
enum{ESP_OK=0,NVS_READONLY=1,NVS_READWRITE=2};
static bool storage_ready=true,have,fail_set,fail_commit;
static uint8_t saved[HR_ROSTER_BYTES],staged[HR_ROSTER_BYTES];
static size_t record_size=HR_ROSTER_BYTES;
static int nvs_open_from_partition(const char*p,const char*n,int mode,int*h){assert(!strcmp(p,"demo_cfg")&&!strcmp(n,"hideseek"));(void)mode;*h=1;return 0;}
static int nvs_get_blob(int h,const char*k,void*b,size_t*n){assert(h==1&&!strcmp(k,"roster_mac_v1")&&*n==HR_ROSTER_BYTES);if(!have)return -1;memcpy(b,saved,record_size);*n=record_size;return 0;}
static int nvs_set_blob(int h,const char*k,const void*b,size_t n){assert(h==1&&!strcmp(k,"roster_mac_v1")&&n==HR_ROSTER_BYTES);if(fail_set)return -1;memcpy(staged,b,n);return 0;}
static int nvs_commit(int h){assert(h==1);if(fail_commit)return -1;memcpy(saved,staged,sizeof saved);have=true;return 0;}
static void nvs_close(int h){assert(h==1);}
'''
main=r'''
int main(void){
 uint8_t mac[6]={0x4c,0x11,0xae,0,0,1},peer[6]={0x4c,0x11,0xae,0,0,2};
 hr_core_t c,reboot;hr_init(&c,2,mac,1,0);hr_identity_enable(&c);
 assert(!hr_storage_roster_load(&c));memcpy(c.peers[1].mac,peer,6);
 assert(hr_storage_roster_save(&c,1u<<1));
 hr_init(&reboot,2,mac,2,0);hr_identity_enable(&reboot);assert(hr_storage_roster_load(&reboot));
 assert(reboot.social.friends==(1u<<1)&&!memcmp(reboot.peers[1].mac,peer,6));
 memcpy(c.peers[3].mac,peer,6);c.peers[3].mac[5]=3;
 fail_set=true;assert(!hr_storage_roster_save(&c,(1u<<1)|(1u<<3)));fail_set=false;
 assert(hr_storage_roster_load(&reboot)&&reboot.social.friends==(1u<<1)&&!reboot.peers[3].mac[0]);
 fail_commit=true;assert(!hr_storage_roster_save(&c,(1u<<1)|(1u<<3)));fail_commit=false;
 hr_init(&reboot,2,mac,3,0);hr_identity_enable(&reboot);assert(hr_storage_roster_load(&reboot));
 assert(reboot.social.friends==(1u<<1)&&!reboot.peers[3].mac[0]);
 assert(hr_storage_roster_save(&c,(1u<<1)|(1u<<3)));assert(hr_storage_roster_load(&reboot));
 assert(reboot.social.friends==((1u<<1)|(1u<<3))&&reboot.peers[3].mac[5]==3);
 saved[20]^=1;assert(!hr_storage_roster_load(&reboot));saved[20]^=1;
 reboot.mac[5]=99;assert(!hr_storage_roster_load(&reboot));
 record_size=12;assert(!hr_storage_roster_load(&c));record_size=HR_ROSTER_BYTES;
 storage_ready=false;assert(!hr_storage_roster_save(&c,0));
 puts("PASS MAC roster: atomic mapping+friends commit, interrupted writes, owner/CRC/length checks");
}
'''
with tempfile.TemporaryDirectory(prefix='hr-roster-') as td:
 p=Path(td);(p/'test.c').write_text(fixture+function('hr_storage_roster_load')+'\n'+function('hr_storage_roster_save')+main)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I',str(root/'main'),str(p/'test.c'),str(root/'main/hr_identity.c'),str(root/'main/hr_core.c'),str(root/'main/hr_game.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
