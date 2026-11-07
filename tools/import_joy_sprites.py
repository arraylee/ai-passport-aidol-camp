"""Extract generated JOY pose cells into consistent badge-sized source sprites.

No drawing here: preserve the selected generated character pixels, isolate each
connected silhouette, resize uniformly and align its feet. Raw generator output
and official-reference provenance remain beside the resulting PNGs.
"""
from pathlib import Path
from collections import deque
import hashlib,json
from PIL import Image,ImageFilter
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/game/joy'
source=OUT/'joy-jump-sheet-clean.png'
sheet=Image.open(source).convert('RGBA')
records=[]
for index,name in enumerate(('idle','push','tuck','land')):
    x0=index%2*sheet.width//2;x1=(index%2+1)*sheet.width//2
    y0=index//2*sheet.height//2;y1=(index//2+1)*sheet.height//2
    cell=sheet.crop((x0,y0,x1,y1));w,h=cell.size
    alpha=cell.getchannel('A');data=alpha.tobytes();seen=bytearray(w*h);best=[]
    for pos,value in enumerate(data):
        if value<128 or seen[pos]:continue
        component=[];queue=deque([pos]);seen[pos]=1
        while queue:
            n=queue.popleft();component.append(n);x=n%w;y=n//w
            for k,valid in ((n-1,x>0),(n+1,x+1<w),(n-w,y>0),(n+w,y+1<h)):
                if valid and not seen[k] and data[k]>=128:
                    seen[k]=1;queue.append(k)
        if len(component)>len(best):best=component
    assert len(best)>w*h//5,(name,len(best))
    mask=Image.new('L',(w,h));raw=bytearray(w*h)
    for n in best:raw[n]=255
    mask.frombytes(bytes(raw));mask=mask.filter(ImageFilter.MaxFilter(5))
    # Keep original antialiasing only adjacent to the selected character.
    from PIL import ImageChops
    cell.putalpha(ImageChops.multiply(alpha,mask))
    box=cell.getbbox();cut=cell.crop(box)
    scale=min(54/cut.width,54/cut.height)
    size=(round(cut.width*scale),round(cut.height*scale))
    cut=cut.resize(size,Image.Resampling.LANCZOS)
    sprite=Image.new('RGBA',(56,56));sprite.alpha_composite(cut,((56-size[0])//2,55-size[1]))
    target=OUT/f'joy_{name}.png';sprite.save(target)
    records.append({'pose':name,'file':str(target.relative_to(ROOT)), 'size':[56,56],
                    'source_cell':[x0,y0,x1,y1],'source_character_bounds':list(box),
                    'foot_baseline':55,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
(OUT/'sprites.json').write_text(json.dumps({'character':'JD JOY','status':'generated game adaptation; not official game assets or brand approval',
 'official_reference_page':'https://jdrdl.jd.com/Brand-ip.html','source':str(source.relative_to(ROOT)),
 'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'frames':records},indent=2)+'\n')
print('Extracted JOY poses',[(r['pose'],r['size']) for r in records])
