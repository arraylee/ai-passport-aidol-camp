"""Data-only badge operations. Every write is bound to an inspected MAC/layout."""
from pathlib import Path
import hashlib,json,re,struct,threading
PARTS={'demo_cfg':(0x360000,0x6000),'profile_store':(0x380000,0x30000),'friend_store':(0x3b0000,0x30000),'gallery_store':(0x400000,0xc0000)}
PROTECTED={'identity':(0x356000,0x4000),'original_nvs':(0x9000,0x6000),'partition_table':(0x8000,4096),'app_header':(0x10000,4096)}
KNOWN={'4c:11:ae:2f:6d:10':1,'4c:11:ae:2f:38:cc':2,'4c:11:ae:32:ed:54':3}
IDENTITIES={'4c:11:ae:2f:6d:10':'e27911a63e89645c3d8491a623639861d428b9a013f9dbbede98b506d7e7b63b','4c:11:ae:2f:38:cc':'3421e0aa9a364a098c8c6fc80b4c2a1aaa86b94b03c814b6037b52a30ef74257','4c:11:ae:32:ed:54':'ee16815b7da3b5e29dc1598344cab72f01ede832ecbc5e8f5f1f36e44efc782a'}
def sha(data):return hashlib.sha256(data).hexdigest()
def checked_layout(table,app):
 entries={}
 for at in range(0,len(table)-31,32):
  magic,kind,sub,offset,size,label,flags=struct.unpack_from('<HBBII16sI',table,at)
  if magic!=0x50aa:break
  entries[label.split(b'\0')[0].decode()] = (kind,sub,offset,size,flags)
 wanted={'factory':(0,0,0x10000,0x300000),'nvs':(1,2,0x9000,0x6000),'cardid':(1,2,0x356000,0x4000)}
 wanted.update({name:(1,sub,*PARTS[name]) for name,sub in [('demo_cfg',2),('profile_store',0x41),('friend_store',0x42),('gallery_store',0x43)]})
 if any(entries.get(k,())[:4]!=v for k,v in wanted.items()) or not entries['cardid'][4]&2:raise ValueError('分区布局不匹配，未写入任何数据')
 if len(app)<112 or struct.unpack_from('<I',app,32)[0]!=0xabcd5432 or app[80:112].split(b'\0')[0]!=b'passport_hideseek_radio':raise ValueError('不是本项目演示固件，不能执行重置')
 version=app[48:80].split(b'\0')[0].decode();m=re.fullmatch(r'(\d+)\.(\d+)\.(\d+)',version)
 if not m or not ( (0,4,15)<=tuple(map(int,m.groups()))<=(0,6,0) ):raise ValueError('当前固件版本未经本工具验证：'+version)
 return version
class Console:
 def __init__(self,root,state,device_factory,ports):
  self.root=Path(root);self.state=Path(state);self.state.mkdir(parents=True,exist_ok=True)
  self.factory=device_factory;self.ports=ports;self.lock=threading.RLock();self.inspected={}
  self.registry=dict(KNOWN);self.known_identities=dict(IDENTITIES)
  p=self.state/'bindings.json'
  if p.exists():
   saved=json.loads(p.read_text());self.registry=dict(saved['roles']);self.known_identities.update(saved.get('identity_hashes',{}))
  if any(not re.fullmatch(r'(?:[0-9a-f]{2}:){5}[0-9a-f]{2}',m) or type(i)!=int or not 1<=i<=6 for m,i in self.registry.items()):raise ValueError('设备绑定文件损坏')
  meta=json.loads((self.root/'presets/manifest.json').read_text());self.roles={r['id']:r for r in meta['roles']}
 def list_ports(self):
  out=[]
  for p in self.ports():
   cached=self.inspected.get(p['port'])
   if cached and p.get('usb_mac') and cached['mac']!=p['usb_mac']:self.inspected.pop(p['port'],None);cached=None
   out.append({**p,'assigned_role':self.registry.get(p.get('usb_mac','')),'info':cached})
  return out
 def _port(self,port):
  if not re.fullmatch(r'/dev/cu\.usbmodem[A-Za-z0-9._-]+',port) or port not in {p['port'] for p in self.ports()}:raise ValueError('设备已断开或串口不正确，请刷新设备列表')
 def _read_info(self,d):
  table=d.read(0x8000,4096);app=d.read(0x10000,4096);version=checked_layout(table,app)
  identity=d.read(0x356000,0x4000);identity_hash=sha(identity)
  if d.mac in self.known_identities and identity_hash!=self.known_identities[d.mac]:raise ValueError('原身份校验不匹配，已停止')
  cfg=d.read(0x360000,0x6000);role=self.registry.get(d.mac,0)
  if not role:
   import zlib
   for at in range(len(cfg)-11):
    if cfg[at:at+4]==b'HRC1' and 1<=cfg[at+4]<=6 and zlib.crc32(cfg[at:at+8])==struct.unpack_from('<I',cfg,at+8)[0]:role=cfg[at+4];break
  name=self.roles.get(role,{}).get('name','未分配角色')
  valid=[]
  import zlib
  for bank in [0,1]:
   h=d.read(0x380000+bank*65536,224)
   if h[:4]==b'HRE1' and h[8]==role and zlib.crc32(h[:220])==struct.unpack_from('<I',h,220)[0]:
    valid.append((struct.unpack_from('<I',h,4)[0],h[16:65].split(b'\0')[0].decode('utf-8','replace')))
  if valid:name=max(valid)[1]
  from nvs_identity import public_sn
  return {**public_sn(identity,d.mac),'mac':d.mac,'role':role,'name':name,'version':version,'shared_roles':tuple(map(int,version.split('.')))>=(0,6,0),'identity_sha256':identity_hash,'partition_sha256':sha(table),'app_header_sha256':sha(app)}
 def inspect(self,port,progress=lambda text:None):
  with self.lock:
   self._port(port);progress('正在连接并识别，设备会短暂重启')
   with self.factory(port) as d:
    try:info=self._read_info(d)
    finally:d.boot()
   self.inspected[port]=info;return info
 def preset(self,role):
  r=self.roles.get(role)
  if not r:raise ValueError('请选择1～6号预设角色')
  out={}
  for kind in ['profile','config']:
   name=r[kind+'_file']
   if name!=f'{role}-{kind}.bin':raise ValueError('预设文件名不正确')
   b=(self.root/'presets'/name).read_bytes()
   if sha(b)!=r[kind+'_sha256'] or len(b)!=(0x30000 if kind=='profile' else 0x6000):raise ValueError('预设资料校验失败，未执行写入')
   out[kind]=b
  return out
 def execute(self,action,port,mac,role,progress=lambda text:None,replace=False):
  with self.lock:
   self._port(port)
   if action not in ['clear','restore'] or type(role)!=int or role not in self.roles:raise ValueError('操作或角色不正确')
   seen=self.inspected.get(port)
   if not seen or seen['mac']!=mac:raise ValueError('请先识别这台设备，再执行操作')
   if action=='clear' and role!=seen['role']:raise ValueError('清空记录不会更改原编号，请选择当前编号')
   occupied=None if seen.get('shared_roles') else next((m for m,i in self.registry.items() if i==role and m!=mac),None)
   if occupied and not replace:raise ValueError('该编号已绑定另一台设备，请确认旧设备不参与后勾选替换绑定')
   if occupied and any(p.get('usb_mac')==occupied for p in self.ports()):raise ValueError('原编号设备仍连接着，不能产生重复编号')
   preset=self.preset(role);images={}
   if action=='restore':images[0x380000]=preset['profile']
   images[0x3b0000]=b'\xff'*0x30000
   if action=='restore':images[0x400000]=b'\xff'*0xc0000
   images[0x360000]=preset['config']
   if any((a,len(b)) not in PARTS.values() for a,b in images.items()):raise ValueError('写入范围不在演示数据区')
   progress('正在核对真实设备和原身份')
   with self.factory(port) as d:
    if d.mac!=mac:raise ValueError('USB设备已更换，停止操作')
    current=self._read_info(d)
    if any(current[k]!=seen[k] for k in ['mac','identity_sha256','partition_sha256','app_header_sha256']):raise ValueError('设备或固件在识别后发生变化，停止写入')
    preserved={k:d.read(a,n) for k,(a,n) in PROTECTED.items()}
    if action=='clear':preserved.update({k:d.read(*PARTS[k]) for k in ['profile_store','gallery_store']})
    progress('正在写入演示数据，固件保持原样');d.write(images,progress)
    progress('正在完整回读演示数据')
    for a,b in images.items():
     if d.read(a,len(b))!=b:raise ValueError('演示数据回读不一致，请重新执行操作')
    progress('正在核对原身份、原NVS和固件')
    for k,b in preserved.items():
     a,n=PROTECTED.get(k,PARTS.get(k))
     if d.read(a,n)!=b:raise ValueError('受保护或应保留的数据校验不一致，已停止：'+k)
    d.boot()
   registry=dict(self.registry)
   if occupied:registry.pop(occupied)
   registry[mac]=role;identities=dict(self.known_identities);identities[mac]=current['identity_sha256']
   report={'action':action,'mac':mac,'role':role,'name':self.roles[role]['name'],'sn':current['sn'],'sn_source':current['sn_source'],'version':current['version'],'verified':True,'original_identity_unchanged':True,'original_nvs_unchanged':True,'app_header_unchanged':True,'firmware_written':False,'written_regions':[{'address':hex(a),'size':len(b),'sha256':sha(b)} for a,b in images.items()]}
   p=self.state/'bindings.json';tmp=p.with_suffix('.tmp');tmp.write_text(json.dumps({'roles':registry,'identity_hashes':identities},indent=2)+'\n');tmp.replace(p)
   self.registry=registry;self.known_identities=identities;self.inspected[port]={**current,'role':role,'name':self.roles[role]['name'] if action=='restore' else current['name']}
   import time
   (self.state/f'result-{time.time_ns()}.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
   progress('已完成，回读一致；请短按下键重新激活');return report
