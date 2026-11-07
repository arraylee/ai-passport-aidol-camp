#!/usr/bin/env python3
from pathlib import Path
import re,subprocess,json,hashlib,argparse
from PIL import ImageFont
ROOT=Path(__file__).resolve().parents[1]
FONT=ROOT/'assets/fonts/LXGWWenKai-Medium.ttf'
TOKEN=re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"')
def chars():
 text=''
 for p in (ROOT/'main').glob('*.[ch]'):
  if p.name.startswith('hr_font'):continue
  for m in TOKEN.finditer(p.read_text()):
   if m[0].startswith('"'):text+=re.sub(r'\\(?:x[0-9a-fA-F]+|[0-7]{1,3}|.)','',m[0][1:-1])
 return ''.join(sorted(set(c for c in text if ord(c)>=32 and not c.isspace())|set(map(chr,range(32,127)))))
if __name__=='__main__':
 ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');args=ap.parse_args();text=chars();records=[]
 for size in (14,16,20,24,40,80):
  subset='123' if size==80 else '0123456789' if size==40 else text;dest=ROOT/f'main/hr_font{size}.c'
  if args.check:
   code=dest.read_text();packed=set(int(cp,16) for cp in re.findall(r'HR_CMAP U\+([A-F0-9]+)',code));assert set(map(ord,subset))<=packed,f'font {size}: missing characters';assert re.search(r'\.bpp\s*=\s*4',code)
  else:
   font=ImageFont.truetype(str(FONT),size);unknown=bytes(font.getmask(chr(0x10ffff)))
   for c in subset:
    if not c.isspace():assert bytes(font.getmask(c))!=unknown,f'missing source glyph U+{ord(c):04X}'
   subprocess.run(['npx','--yes','lv_font_conv@1.5.3','--font',str(FONT),'--size',str(size),'--bpp','4','--no-kerning','--format','lvgl','--lv-include','lvgl.h','--symbols',subset,'--output',str(dest)],check=True)
   fallback=f'hr_font_common{size}' if size == 20 else 'hr_font_common'
   code=dest.read_text();code=code.replace('.fallback = NULL', '.fallback = &'+fallback) if size<40 else code;code='#include \"lvgl.h\"\nLV_FONT_DECLARE('+fallback+');\n'+code if size<40 else code;attribution='/* HideSeek Radio UI: bitmap derivative of LXGW WenKai Medium v1.522.\n * Source attribution and complete SIL OFL 1.1: assets/fonts/FONT-NOTICES.txt.\n * Original source names are attribution, not the derivative family name. */\n'
   dest.write_text(attribution+'\n'.join(f'/* HR_CMAP U+{ord(c):04X} */' for c in subset)+'\n'+code.replace(str(ROOT)+'/','').rstrip()+'\n')
  records.append({'size':size,'codepoints':list(map(ord,subset))})
  print(f'{size}px: {len(subset)} glyphs, native 4bpp',flush=True)
 if not args.check:(ROOT/'assets/fonts/manifest.json').write_text(json.dumps({'family':'HideSeek Radio UI','source_family':'LXGW WenKai Medium','source_sha256':hashlib.sha256(FONT.read_bytes()).hexdigest(),'bpp':4,'fonts':records},indent=2)+'\n')
