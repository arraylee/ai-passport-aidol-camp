"""Validate the shipped miniapp-compatible seed against original RGB565 pixels."""
from pathlib import Path
import re,json,hashlib,struct
from PIL import Image
R=Path(__file__).resolve().parents[1]
meta=json.loads((R/'assets/profiles/live-manifest.json').read_text());code=(R/'main/hr_profiles.c').read_text()
assert hashlib.sha256((R/'assets/profiles'/meta['seed']).read_bytes()).hexdigest()==meta['sha256']
assert meta['rgb565_images']==3 and 'seed_gallery_pixels' not in code
for kind,w,h in [('card',96,132),('small',32,32),('portrait',96,156)]:
 im=Image.open(R/f'assets/profiles/miaomiao-{kind}.png').convert('RGB');assert im.size==(w,h)
 want=b''.join(struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3)) for r,g,b in im.getdata())
 literal=re.search(r'seed_'+kind+r'_pixels\[\]=\{(.*?)\};',code,re.S)[1]
 assert bytes(map(int,re.findall(r'\d+',literal)))==want
assert '&seed_card,NULL,&seed_small,&seed_portrait' in code
print('PASS original seed hash, 3 compiled RGB565 images, 96x156 miniapp portrait; unused gallery removed')
