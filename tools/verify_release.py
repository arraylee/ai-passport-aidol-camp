#!/usr/bin/env python3
import json
import sys
import zipfile
from pathlib import Path
from release_layout import load_release,sha
from verify_web_image import verify

def main(directory):
    root=Path(directory);manifest,segments=load_release(root)
    web=(root/manifest['web']['file']).read_bytes();verify(root/manifest['web']['file'])
    assert sha(web)==manifest['web']['sha256'] and len(web)==manifest['web']['size']
    for name,(address,data) in segments.items():assert web[address:address+len(data)]==data,name
    assert sha((root/manifest['source']['file']).read_bytes())==manifest['source']['sha256']
    with zipfile.ZipFile(root/manifest['source']['file']) as z:
        assert set(z.namelist())==set(manifest['source_files'])
        for name,digest in manifest['source_files'].items():assert sha(z.read(name))==digest,name
    archive=next(root.glob('*-firmware.zip'))
    with zipfile.ZipFile(archive) as z:
        for name in z.namelist():assert z.read(name)==(root/name).read_bytes(),name
    for line in (root/'SHA256SUMS.txt').read_text().splitlines():
        digest,name=line.split('  ',1);assert sha((root/name).read_bytes())==digest,name
    print('PASS release: offsets, erase sectors, partitions, web/ZIP/source byte equality, all hashes')
if __name__=='__main__':main(sys.argv[1])
