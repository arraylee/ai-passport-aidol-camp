"""Exercise production profile storage for first boot, corruption and commit failure."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def function(source, declaration):
    start = source.index(declaration)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

core = (ROOT/'main/hr_core.c').read_text()
platform = (ROOT/'main/hr_platform.c').read_text()
fixture = r'''
#include "hr_platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
typedef int nvs_handle_t;
enum { ESP_OK=0, NVS_READONLY=1, NVS_READWRITE=2,
       ESP_ERR_NVS_NO_FREE_PAGES=10, ESP_ERR_NVS_NEW_VERSION_FOUND=11 };
static bool storage_ready, exists, key_exists, commit_fail;
static int init_error;
static unsigned commits;
static unsigned char committed[12], staged[12];
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
static const char *esp_err_to_name(int error) { (void)error; return "failure"; }
static int nvs_flash_init_partition(const char *p) {
  assert(!strcmp(p,"demo_cfg")); return init_error;
}
static int nvs_flash_erase_partition(const char *p) {
  assert(!strcmp(p,"demo_cfg")); exists=key_exists=false; return 0;
}
static int nvs_open_from_partition(const char *p,const char *n,int mode,int *h) {
  assert(!strcmp(p,"demo_cfg") && !strcmp(n,"hideseek"));
  *h=1; memcpy(staged,committed,12);
  return mode==NVS_READONLY && !exists ? -1 : 0;
}
static int nvs_get_blob(int h,const char *key,void *data,size_t *n) {
  assert(h==1 && !strcmp(key,"settings") && *n==12);
  if(!key_exists)return -1;
  memcpy(data,committed,12); return 0;
}
static int nvs_set_blob(int h,const char *key,const void *data,size_t n) {
  assert(h==1 && !strcmp(key,"settings") && n==12);
  memcpy(staged,data,12); return 0;
}
static int nvs_set_u8(int h,const char *key,unsigned char value) {
  assert(h==1 && !strcmp(key,"volume90_v1") && value==1); return 0;
}
static int nvs_commit(int h) {
  assert(h==1); ++commits;
  if(commit_fail)return -1;
  memcpy(committed,staged,12); exists=key_exists=true; return 0;
}
static void nvs_close(int h) { assert(h==1); }
'''
body = '\n'.join(function(core, name) for name in
    ('static bool valid_id(', 'static uint32_t get32(', 'static void put32(',
     'uint32_t hr_crc(', 'void hr_config_encode(', 'bool hr_config_decode('))
body += '\n' + '\n'.join(function(platform, 'bool '+name+'(') for name in
    ('hr_storage_save', 'hr_storage_load'))
cases = r'''
int main(void) {
  uint8_t id=0, volume=0;
  /* With no namespace, activation must not lead to network-number setup. */
  assert(hr_storage_load(&id,&volume));
  assert(id==1 && volume==90 && commits==1);
  unsigned before=commits;
  id=volume=0; assert(hr_storage_load(&id,&volume));
  assert(id==1 && volume==90 && commits==before);
  /* A real saved persona and the user's volume survive boot unchanged. */
  assert(hr_storage_save(6,35)); before=commits;
  assert(hr_storage_load(&id,&volume));
  assert(id==6 && volume==35 && commits==before);
  /* Missing and corrupt settings receive a valid persisted default. */
  key_exists=false; assert(hr_storage_load(&id,&volume));
  assert(id==1 && volume==90);
  committed[8]^=1; assert(hr_storage_load(&id,&volume));
  assert(id==1 && volume==90);
  /* A failed default commit is never reported as successful setup. */
  exists=key_exists=false; commit_fail=true;
  assert(!hr_storage_load(&id,&volume));
  assert(!key_exists);
  commit_fail=false; assert(hr_storage_load(&id,&volume));
  init_error=99; before=commits;
  assert(!hr_storage_load(&id,&volume) && commits==before);
  puts("PASS first boot persists default profile; saved persona/volume preserved; corrupt settings and failed writes handled");
}
'''
with tempfile.TemporaryDirectory(prefix='hr-first-boot-') as temp:
    p=Path(temp)
    (p/'test.c').write_text(fixture+body+cases)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',
        '-Wno-unused-function','-fsanitize=undefined','-I',str(ROOT/'main'),
        str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
