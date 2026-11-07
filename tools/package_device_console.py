#!/usr/bin/env python3
"""Package the local USB helper, preserving the double-click launcher's mode."""
from pathlib import Path
import hashlib,json,shutil,zipfile
ROOT=Path(__file__).resolve().parents[1]
NAME='AI PASSPORT 测试助手'
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 source=ROOT/'tools/device_console';release=ROOT/'releases/device-console-v0.2.0';bundle=release/NAME
 release.mkdir(parents=True,exist_ok=True)
 if bundle.exists():shutil.rmtree(bundle)
 shutil.copytree(source,bundle,ignore=shutil.ignore_patterns('__pycache__','.DS_Store','.venv','SHA256SUMS.txt'))
 files={str(p.relative_to(bundle)):p.read_bytes() for p in bundle.rglob('*') if p.is_file()}
 (bundle/'SHA256SUMS.txt').write_text(''.join(f'{sha(b)}  {n}\n' for n,b in sorted(files.items())))
 files['SHA256SUMS.txt']=(bundle/'SHA256SUMS.txt').read_bytes()
 archive=release/'AI-PASSPORT-Demo-Console-v0.2.0.zip'
 with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for name,data in sorted(files.items()):
   item=zipfile.ZipInfo(NAME+'/'+name,(2026,10,21,0,0,0));item.create_system=3;item.compress_type=zipfile.ZIP_DEFLATED;item.external_attr=(0o100755 if name.endswith('.command') else 0o100644)<<16;z.writestr(item,data)
 with zipfile.ZipFile(archive) as z:
  assert len(z.namelist())==len(files)
  for name,data in files.items():assert z.read(NAME+'/'+name)==data
  assert (z.getinfo(NAME+'/启动AI PASSPORT助手.command').external_attr>>16)&0o111
 (release/'SHA256SUMS.txt').write_text(f'{sha(archive.read_bytes())}  {archive.name}\n')
 print(json.dumps({'zip':str(archive),'files':len(files),'sha256':sha(archive.read_bytes()),'launcher_executable':True,'zip_bytes_equal':True},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
