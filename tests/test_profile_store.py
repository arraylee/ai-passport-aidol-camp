import struct,zlib,subprocess,tempfile,sys
from pathlib import Path
def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d)&0xffffffff)
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'avatar.png'
 for channels,color in [(3,2),(4,6),(1,0)]:
  raw=b''.join(b'\0'+bytes((x*37+y*19)%256 for x in range(96*channels)) for y in range(156))
  compressed=zlib.compress(raw,0)
  p.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',96,156,8,color,0,0,0))+chunk(b'IDAT',compressed)+chunk(b'IEND',b''))
  subprocess.run([sys.argv[1],str(p)],check=True)
