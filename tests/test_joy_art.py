"""Verify shipped JOY pixels, transparency and the exact Flash sprite format."""
from pathlib import Path
import hashlib,json,re,struct
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
code=(ROOT/'main/hr_game_art.c').read_text()
manifest=json.loads((ROOT/'assets/game/manifest.json').read_text())
for pose in ('idle','push','tuck','land'):
    name='joy_'+pose
    record=next((r for r in manifest['sprites'] if r['name']==name),None)
    assert record is not None,'missing JOY pose '+pose
    assert record['alignment']==4
    im=Image.open(ROOT/'assets/game/joy'/f'{name}.png').convert('RGBA')
    assert im.size==(56,56) and im.getchannel('A').getextrema()==(0,255)
    bbox=im.getbbox();assert bbox and bbox[0]>=0 and bbox[2]<=56 and bbox[3]<=56
    rgb=bytearray();alpha=bytearray()
    for r,g,b,a in im.getdata():
        rgb+=struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3));alpha.append(a)
    raw=rgb+alpha
    assert len(raw)==9408 and hashlib.sha256(raw).hexdigest()==record['sha256']
    match=re.search(r'hr_game_'+name+r'_pixels\[\]=\{(.*?)\};',code,re.S)
    assert match and bytes(map(int,re.findall(r'\d+',match[1])))==raw
    assert f'.cf=LV_COLOR_FORMAT_RGB565A8,.w=56,.h=56,.stride=112' in code
assert not re.search(r'hr_game_chicken',code+(ROOT/'main/hr_game_scene.c').read_text()+(ROOT/'main/hr_ui.c').read_text())
print('PASS4 JOY alpha/size/PNG/packed-pixel hashes and no compiled chicken')
# Red hazards must remain visually distinct from the navy venue and gold coins.
for name in ('spike0','spike1'):
    hazard=Image.open(ROOT/'assets/game'/f'{name}.png').convert('RGBA')
    red=sum(1 for r,g,b,a in hazard.getdata() if a>=200 and r>=150 and r>g*1.7 and r>b*1.4)
    assert red>=100,(name,'hazard lost clear red core',red)
# The repeating player-column backdrop stays dark enough for the white JOY.
backdrop=Image.open(ROOT/'assets/game/sky.png').convert('RGB')
assert max(max(rgb) for rgb in backdrop.getdata())<80
print('PASS red hazard cores and quiet navy character backdrop')
