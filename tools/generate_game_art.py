"""Compact background art and generated JOY poses; packed at badge size, no runtime decoder."""
from pathlib import Path
from PIL import Image, ImageDraw
import hashlib,json,struct,math
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/game';OUT.mkdir(exist_ok=True)
S=4
sprites={}
def make(name,w,h,draw):
 im=Image.new('RGBA',(w*S,h*S));d=ImageDraw.Draw(im)
 def box(b,c,r=0):d.rounded_rectangle(tuple(round(t*S) for t in b),radius=r*S,fill=c)
 def poly(p,c):d.polygon([(round(x*S),round(y*S)) for x,y in p],fill=c)
 def ellipse(b,c):d.ellipse(tuple(round(t*S) for t in b),fill=c)
 def line(p,c,width=1):d.line([(round(x*S),round(y*S)) for x,y in p],fill=c,width=round(width*S))
 draw(box,poly,ellipse,line)
 im=im.resize((w,h),Image.Resampling.LANCZOS)
 im.save(OUT/(name+'.png'));sprites[name]=im
# Selected imagegen-derived JOY poses, extracted by import_joy_sprites.py.
for pose in ('idle','push','tuck','land'):
 im=Image.open(OUT/'joy'/f'joy_{pose}.png').convert('RGBA')
 assert im.size==(56,56)
 sprites['joy_'+pose]=im
def platform(box,poly,el,line):
 # Small repeating chrome lip: the top is solid support, not a grass fringe.
 box((0,0,23,12),'#1a385c',2)
 box((0,0,23,2),'#edf5ff',1);box((0,3,23,5),'#a0b5cc')
 box((0,6,23,8),'#f0f5fb');box((0,9,23,11),'#52779f')
 line([(0,13),(23,13)],'#5eacf2',1)
make('platform',24,16,platform)
def wave(box,poly,el,line):
 poly([(0,6),(8,0),(16,6),(24,0),(32,6),(32,16),(0,16)],'#81bee8')
 poly([(0,10),(8,4),(16,10),(24,4),(32,10),(32,16),(0,16)],'#245e9d')
 poly([(0,15),(8,9),(16,15),(24,9),(32,15),(32,16),(0,16)],'#11345f')
make('wave',32,16,wave)
for frame in range(2):
 def spike(box,poly,el,line,f=frame):
  for i in range(8):
   angle=(i*45+f*22.5)*math.pi/180
   center=(16,16)
   point=lambda r,a:(center[0]+r*math.cos(a),center[1]+r*math.sin(a))
   poly([point(9,angle-.22),point(16,angle),point(9,angle+.22)],'#f0ecdf')
  el((6,6,26,26),'#8c1f24');el((8,8,25,25),'#e1251b')
  el((10,9,17,15),'#ff8a78');el((16,18,23,24),'#b42126')
  for x,y in [(11,18),(20,12)]:el((x,y,x+2,y+2),'#ffe9e4')
 make('spike'+str(frame),32,32,spike)
def flag(box,poly,el,line):
 box((2,2,5,41),'#a8bfd5',1);el((1,0,6,5),'#edf5ff')
 box((5,4,23,22),'#fffef7')
 for y in range(3):
  for x in range(3):
   if (x+y)%2==0:box((5+x*6,4+y*6,10+x*6,9+y*6),'#334660')
make('flag',24,44,flag)
def coin(box,poly,el,line):
 el((0,0,11,15),'#d69628');el((1,0,11,13),'#ffd45f');el((3,2,10,11),'#ffe28c');line([(6,4),(6,9)],'#e6ad36',2)
make('coin',12,16,coin)
def glint(box,poly,el,line):
 poly([(6,0),(7,5),(12,6),(7,7),(6,12),(5,7),(0,6),(5,5)],'#a8c8e7')
 poly([(6,3),(7,6),(6,9),(5,6)],'#eef7ff')
make('glint',13,13,glint)
# Quiet navy halftone strip, tiled directly from Flash: no full-frame decoder.
sky=Image.new('RGBA',(16,320));bayer=((0,8,2,10),(12,4,14,6),(3,11,1,9),(15,7,13,5))
for y in range(320):
 t=y/319
 base=(5+5*math.sin(t*math.pi),14+15*math.sin(t*math.pi),35+24*math.sin(t*math.pi))
 for x in range(16):
  fraction=(bayer[y%4][x%4]+.5)/16
  dot=3 if x%8==2 and y%8==2 else 0
  rgb=[max(0,min(255,round(base[k]+(fraction-.5)*step+dot))) for k,step in enumerate((4,4,8))]
  sky.putpixel((x,y),(*rgb,255))
sky.save(OUT/'sky.png');sprites['sky']=sky
parts=['/* JOY adaptation and original background art. Regenerate: tools/generate_game_art.py. See assets/game/joy/sprites.json. */\n#include "lvgl.h"\n']
records=[]
for name,im in sprites.items():
 raw=bytearray();alpha=bytearray()
 for r,g,b,a in im.getdata():
  raw+=struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3));alpha.append(a)
 if name!='sky':raw+=alpha
 symbol='hr_game_'+name
 parts.append('_Alignas(4) static const uint8_t '+symbol+'_pixels[]={\n'+''.join(','.join(map(str,raw[i:i+32]))+',\n' for i in range(0,len(raw),32))+'};\n')
 fmt='LV_COLOR_FORMAT_RGB565' if name=='sky' else 'LV_COLOR_FORMAT_RGB565A8'
 parts.append(f'const lv_image_dsc_t {symbol}={{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf={fmt},.w={im.width},.h={im.height},.stride={im.width*2}}},.data_size=sizeof {symbol}_pixels,.data={symbol}_pixels}};\n')
 records.append({'name':name,'size':list(im.size),'packed_bytes':len(raw),'alignment':4,'format':'RGB565' if name=='sky' else 'RGB565A8','sha256':hashlib.sha256(raw).hexdigest()})
(ROOT/'main/hr_game_art.c').write_text(''.join(parts))
(OUT/'manifest.json').write_text(json.dumps({'original_art':'background vector recipes; JOY adaptation from imagegen and official references in assets/game/joy/sprites.json','theme':'Aidol venue navy/chrome; supplied reference assets/game/venue/aidol-reference.png','format':'RGB565A8 sprites and opaque RGB565 dither sky strip; direct Flash source','sprites':records,'packed_bytes':sum(r['packed_bytes'] for r in records)},indent=2)+'\n')
print('Original sprite art:',len(records),'sprites,',sum(r['packed_bytes'] for r in records),'bytes')
