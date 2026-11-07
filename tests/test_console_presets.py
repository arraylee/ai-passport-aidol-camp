"""Verify desktop presets through the actual firmware C decoders."""
from pathlib import Path
import json,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
C=r'''
#include "hr_edit.h"
#include "hr_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=4)return 2;
 unsigned id=(unsigned)atoi(argv[1]);
 unsigned char *p=malloc(0x30000),*c=malloc(0x6000);
 FILE *f=fopen(argv[2],"rb");if(!f||fread(p,1,0x30000,f)!=0x30000)return 3;fclose(f);
 f=fopen(argv[3],"rb");if(!f||fread(c,1,0x6000,f)!=0x6000)return 4;fclose(f);
 if(id==1){for(unsigned i=0;i<0x30000;i++)if(p[i]!=255)return 5;}
 else{hr_edit_t e;if(!hr_edit_decode(&e,p)||e.id!=id||!e.avatar||e.image_crc!=hr_crc(p+HR_EDIT_FULL_OFFSET,HR_EDIT_IMAGE_END-HR_EDIT_FULL_OFFSET))return 6;}
 unsigned found=0;for(unsigned i=0;i<0x6000-12;i++)if(!memcmp(c+i,"HRC1",4)){uint8_t n=0,v=0;if(!hr_config_decode(c+i,&n,&v)||n!=id||v!=90)return 7;found++;}
 if(found!=1)return 8;
 free(p);free(c);return 0;
}
'''
class PresetDecoders(unittest.TestCase):
 def test_six_role_presets_decode_in_production_firmware(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td);(p/'check.c').write_text(C)
   subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'main'),'-I'+str(ROOT/'tests/vendor/cjson'),str(p/'check.c'),str(ROOT/'main/hr_edit.c'),str(ROOT/'main/hr_core.c'),str(ROOT/'main/hr_game.c'),str(ROOT/'tests/vendor/cjson/cJSON.c'),'-lm','-o',str(p/'check')],check=True)
   presets=ROOT/'tools/device_console/presets'
   for r in json.loads((presets/'manifest.json').read_text())['roles']:
    subprocess.run([str(p/'check'),str(r['id']),str(presets/r['profile_file']),str(presets/r['config_file'])],check=True)
if __name__=='__main__':unittest.main()
