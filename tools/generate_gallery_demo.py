"""Decorative album placeholder. No QR encoder, payload or QR finder patterns."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
S = 3
im = Image.new('RGB', (240*S, 320*S), '#080f20')
d = ImageDraw.Draw(im)
font = ROOT/'assets/fonts/LXGWWenKai-Medium.ttf'
def label(y, text, size, fill):
    f = ImageFont.truetype(str(font), size*S)
    box = d.textbbox((0, 0), text, font=f)
    d.text(((240*S-(box[2]-box[0]))/2, y*S-box[1]), text, font=f, fill=fill)
def rect(box, fill, radius=0, outline=None, width=1):
    d.rounded_rectangle(tuple(int(x*S) for x in box), radius=radius*S, fill=fill, outline=outline, width=width*S)
label(16, 'MY ALBUM', 14, '#91a4bb')
label(44, '示例二维码', 24, '#f0f5ff')
rect((40, 84, 200, 234), '#f0f5ff', 16)
# Disconnected decorative dashes and dots; intentionally unlike a valid QR matrix.
for x,y in [(54,98),(172,98),(54,202),(172,202)]:
    rect((x,y,x+14,y+14), '#304d72', 4)
    rect((x+4,y+4,x+10,y+10), '#f0f5ff', 2)
for x,y,w,h in [(80,99,24,6),(116,99,8,16),(136,100,20,6),(56,126,6,20),(177,126,6,18),(59,164,6,20),(174,165,6,20),(81,207,14,6),(106,204,6,14),(124,209,20,6),(150,201,6,14)]:
    rect((x,y,x+w,y+h), '#91a4bb', 2)
rect((62, 142, 178, 181), '#ef3340', 10)
label(150, '仅作展示', 22, '#ffffff')
label(190, '不可扫描', 12, '#304d72')
label(247, '二维码 / 照片，都可以', 16, '#f0f5ff')
label(273, '用小程序换成你的图片', 14, '#91a4bb')
label(299, '长按下键返回', 12, '#91a4bb')
im = im.resize((240,320), Image.Resampling.LANCZOS)
p = ROOT/'assets/gallery/demo.png';p.parent.mkdir(parents=True,exist_ok=True);im.save(p)
raw=[]
for r,g,b in im.getdata():
    v=((r>>3)<<11)|((g>>2)<<5)|(b>>3);raw.extend([v&255,v>>8])
lines=[','.join(f'0x{v:02x}' for v in raw[i:i+24])+',' for i in range(0,len(raw),24)]
(ROOT/'main/hr_gallery_demo.c').write_text('/* Generated decorative album image; contains no encoded QR payload. */\n#include "lvgl.h"\nstatic const uint8_t pixels[] = {\n'+'\n'.join(lines)+'\n};\nconst lv_image_dsc_t hr_gallery_demo = {\n .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_RGB565, .w=240, .h=320, .stride=480},\n .data_size=sizeof pixels, .data=pixels\n};\n')
print(p)

# Reuse the same non-scannable illustration on all saved friend cards.
friend = im.crop((40, 80, 200, 240)).resize((72, 72), Image.Resampling.LANCZOS)
friend.save(ROOT/'assets/gallery/friend-example.png')
raw=[]
for r,g,b in friend.getdata():
    v=((r>>3)<<11)|((g>>2)<<5)|(b>>3);raw.extend([v&255,v>>8])
lines=[','.join(f'0x{v:02x}' for v in raw[i:i+24])+',' for i in range(0,len(raw),24)]
with (ROOT/'main/hr_gallery_demo.c').open('a') as f:
    f.write('static const uint8_t friend_pixels[] = {\n'+'\n'.join(lines)+'\n};\nconst lv_image_dsc_t hr_friend_qr_demo = {\n .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_RGB565, .w=72, .h=72, .stride=144},\n .data_size=sizeof friend_pixels, .data=friend_pixels\n};\n')
