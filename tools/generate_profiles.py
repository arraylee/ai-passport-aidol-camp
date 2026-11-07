#!/usr/bin/env python3
"""Deterministic RGB565 device conversion. Originals are retained unchanged."""
from pathlib import Path
from PIL import Image, ImageOps
import hashlib,json
R=Path(__file__).resolve().parents[1]
profiles=[
 ('miaomiao','喵喵','简单开放 人人可造','User supplied original image and signature',''),
 ('huyanbin','胡彦斌','歌手','Weibo public verified description, not a fabricated signature','https://weibo.com/huyanbin'),
 ('wangluodan','王珞丹','演员，电视剧《奋斗》中饰演米莱','Weibo public verified description','https://weibo.com/wangluodan'),
 ('xiaoshi','小狮日记','浪漫 至死不渝','Bilibili signature excerpt excluding business contact','https://space.bilibili.com/444728505'),
 ('shougonggeng','手工耿','喜欢做一些有趣的小手工','Bilibili signature','https://space.bilibili.com/280793434'),
 ('lks','LKs','探索包罗万象的世界','Bilibili -LKs- signature excerpt excluding business contact','https://space.bilibili.com/125526')]
out=['#include "hr_profiles.h"'];meta=[]
for n,(slug,name,bio,source,url) in enumerate(profiles,1):
 p=next((R/'assets/profiles').glob(slug+'-original.*'));im=Image.open(p).convert('RGBA')
 bg=Image.new('RGBA',im.size,(8,15,32,255));bg.alpha_composite(im);im=bg.convert('RGB')
 for mode,w,h in [('card',96,132),('gallery',204,204),('small',32,32)]:
  # Miaomiao supplied full-body artwork is contained, never cut off.
  v=ImageOps.pad(im,(w,h),method=Image.Resampling.LANCZOS,color=(8,15,32)) if n==1 or mode=='gallery' else ImageOps.fit(im,(w,h),method=Image.Resampling.LANCZOS)
  raw=bytearray()
  for r,g,b in v.getdata():
   x=((r>>3)<<11)|((g>>2)<<5)|(b>>3);raw.extend((x&255,x>>8))
  sym=f'profile_{n}_{mode}'
  out.append(f'static const uint8_t {sym}_pixels[] = {{\n'+',\n'.join(','.join(str(x) for x in raw[i:i+32]) for i in range(0,len(raw),32))+'\n};')
  out.append(f'static const lv_image_dsc_t {sym} = {{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w={w},.h={h},.stride={w*2}}},.data_size=sizeof {sym}_pixels,.data={sym}_pixels}};')
  v.save(R/f'assets/profiles/{slug}-{mode}.png')
 meta.append(dict(id=n,name=name,signature=bio,source=source,url=url,original=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
out.append('static const hr_profile_t profiles[7] = { {0},')
for n,(_,name,bio,_,url) in enumerate(profiles,1):
 out.append(f'{{"{name}","{bio}","{url}",&profile_{n}_card,&profile_{n}_gallery,&profile_{n}_small}},')
out.append('};\nconst hr_profile_t *hr_profile_base(unsigned id) { return &profiles[id>=1&&id<=6?id:1]; }')
(R/'main/hr_profiles.c').write_text('\n'.join(out)+'\n')
(R/'assets/profiles/manifest.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n')
print('6 originals, 18 RGB565 assets, 240x320-safe fixed dimensions')
