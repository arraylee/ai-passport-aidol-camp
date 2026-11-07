from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
chars=set(chr(i) for i in range(32,127))
for a in range(0xa1,0xf8):
 for b in range(0xa1,0xff):
  try: chars.add(bytes([a,b]).decode('gb2312'))
  except UnicodeError:pass
text=''.join(sorted(chars))
for size in (16,20):
 name='hr_font_common' if size==16 else f'hr_font_common{size}'
 dest=ROOT/f'main/{name}.c'
 subprocess.run(['npx','--yes','lv_font_conv@1.5.3','--font',str(ROOT/'assets/fonts/LXGWWenKai-Medium.ttf'),'--size',str(size),'--bpp','2','--no-kerning','--format','lvgl','--lv-include','lvgl.h','--symbols',text,'--output',str(dest)],check=True)
 s=dest.read_text().replace(str(ROOT)+'/','');dest.write_text('/* Common Chinese fallback: LXGW WenKai, SIL OFL, see assets/fonts/FONT-NOTICES.txt */\n'+s)
 print('Common font',size,len(text),'characters',flush=True)
