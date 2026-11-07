"""Read only the public DeviceKey (SN); never expose other identity fields."""
import struct,zlib

def public_sn(raw,mac):
 namespaces=set();strings=[]
 for page_at in range(0,len(raw),4096):
  p=raw[page_at:page_at+4096]
  if len(p)!=4096 or struct.unpack_from('<I',p)[0] not in (0xfffffffe,0xfffffffc):continue
  if zlib.crc32(p[4:28],0xffffffff)!=struct.unpack_from('<I',p,28)[0]:continue
  bitmap=int.from_bytes(p[32:64],'little');seq=struct.unpack_from('<I',p,4)[0]
  for i in range(126):
   if (bitmap>>(i*2))&3!=2:continue
   e=p[64+32*i:96+32*i]
   if zlib.crc32(e[:4]+e[8:32],0xffffffff)!=struct.unpack_from('<I',e,4)[0]:continue
   key=e[8:24].split(b'\0')[0]
   if e[0]==0 and e[1]==1 and e[2]==1 and key==b'folotoy-key':namespaces.add(e[24])
   if e[1]!=0x21 or key!=b'DeviceKey' or e[2]<2 or i+e[2]>126:continue
   size=struct.unpack_from('<H',e,24)[0]
   if not 1<size<=25 or size>(e[2]-1)*32:continue
   if any((bitmap>>(j*2))&3!=2 for j in range(i+1,i+e[2])):continue
   value=p[96+32*i:96+32*i+size]
   if not value.endswith(b'\0') or zlib.crc32(value,0xffffffff)!=struct.unpack_from('<I',e,28)[0]:continue
   try:sn=value[:-1].decode('utf-8')
   except UnicodeDecodeError:continue
   if any(ord(c)<32 for c in sn):continue
   strings.append((seq,i,e[0],sn))
 valid=[v for v in strings if v[2] in namespaces]
 return {'sn':max(valid)[3],'sn_source':'DeviceKey'} if valid else {'sn':mac.replace(':',''),'sn_source':'firmware MAC fallback'}
