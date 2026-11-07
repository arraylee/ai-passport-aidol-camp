"""Seed one editable personal namecard; other slots have no fabricated profile."""
from pathlib import Path
from PIL import Image,ImageOps
import hashlib,json
R=Path(__file__).resolve().parents[1];p=R/'assets/profiles/miaomiao-original.png'
im=Image.open(p).convert('RGBA');bg=Image.new('RGBA',im.size,(8,15,32,255));bg.alpha_composite(im);im=bg.convert('RGB')
out=['#include "hr_profiles.h"']
for mode,w,h in [('card',96,132),('small',32,32),('portrait',96,156)]:
 # Preserve the complete original image on the miniapp's 96x156 portrait canvas.
 image=ImageOps.pad(im,(w,h),method=Image.Resampling.LANCZOS,color=(8,15,32))
 data=bytearray()
 for r,g,b in image.get_flattened_data():
  value=((r>>3)<<11)|((g>>2)<<5)|(b>>3);data.extend((value&255,value>>8))
 sym='seed_'+mode;out.append(f'static const uint8_t {sym}_pixels[]={{\n'+',\n'.join(','.join(map(str,data[i:i+32])) for i in range(0,len(data),32))+'\n};')
 out.append(f'static const lv_image_dsc_t {sym}={{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w={w},.h={h},.stride={w*2}}},.data_size=sizeof {sym}_pixels,.data={sym}_pixels}};')
 image.save(R/f'assets/profiles/miaomiao-{mode}.png')
out.append('static const hr_profile_t profiles[7]={{0},{"喵喵","FoloToy\\n简单开放\\n人人可造","",&seed_card,NULL,&seed_small,&seed_portrait},')
for i in range(2,7):out.append(f'{{"{i}号设备","用小程序编辑我的名片","",NULL,NULL,NULL,NULL}},')
out.append('};\nconst hr_profile_t *hr_profile_base(unsigned id){return &profiles[id>=1&&id<=6?id:1];}')
(R/'main/hr_profiles.c').write_text('\n'.join(out)+'\n')
(R/'assets/profiles/live-manifest.json').write_text(json.dumps({'seed':'miaomiao-original.png','sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'rgb565_images':3,'customizable':True,'other_slots':'numbered, no portrait'},ensure_ascii=False,indent=2)+'\n')
