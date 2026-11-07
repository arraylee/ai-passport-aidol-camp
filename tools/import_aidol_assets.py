from pathlib import Path
import re,shutil,hashlib,json
from PIL import Image
root=Path(__file__).resolve().parents[1]
src=root.parent/'ai-passport-event-firmware'
out=root/'assets/aidol';out.mkdir(exist_ok=True)
for n in ['welcome.adpcm','welcome.json','joy-32.txt']:
 shutil.copyfile(src/'assets/aidol'/n,out/n)
s=(src/'main/aidol_assets.c').read_text()
data=bytes(map(lambda v:int(v,0),re.search(r'aidol_joy_data\[\]=\{(.*?)\};',s,re.S)[1].replace('\n','').strip().rstrip(',').split(',')))
assert len(data)==32*32*3
rgba=[]
for n in range(1024):
 v=int.from_bytes(data[n*2:n*2+2],'little');rgba.append(((v>>11)*255//31,((v>>5)&63)*255//63,(v&31)*255//31,data[2048+n]))
im=Image.new('RGBA',(32,32));im.putdata(rgba);im=im.resize((80,80),Image.Resampling.NEAREST);im.save(out/'joy-80.png')
rgb=bytearray();alpha=bytearray()
for r,g,b,a in im.getdata():
 rgb.extend((((r>>3)<<11)|((g>>2)<<5)|(b>>3)).to_bytes(2,'little'));alpha.append(a)
raw=rgb+alpha
welcome=(out/'welcome.adpcm').read_bytes()
def array(name,raw):
 return 'const uint8_t '+name+'[] = {\n'+'\n'.join(','.join(str(v) for v in raw[i:i+32])+',' for i in range(0,len(raw),32))+'\n};\n'
(root/'main/hr_aidol_assets.c').write_text('/* Reused Aidol S2 assets; see assets/aidol/sources.json. */\n#include "lvgl.h"\n#include <stddef.h>\n'+array('hr_joy_pixels',raw)+'const lv_image_dsc_t hr_joy={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565A8,.w=80,.h=80,.stride=160},.data_size=sizeof hr_joy_pixels,.data=hr_joy_pixels};\n'+array('hr_welcome_data',welcome)+'const size_t hr_welcome_size=sizeof hr_welcome_data;\n')
(out/'sources.json').write_text(json.dumps({'source_project':'ai-passport-event-firmware','source_joy_file':'main/aidol_assets.c / aidol_joy','source_joy_sha256':hashlib.sha256(data).hexdigest(),'joy_conversion':'exact existing 32px RGB565A8 pixels to nearest-neighbor 80px, RGB565A8 in Flash','welcome_sha256':hashlib.sha256(welcome).hexdigest(),'welcome_unmodified':True},indent=2)+'\n')
print('welcome',len(welcome),'joy',len(raw))
