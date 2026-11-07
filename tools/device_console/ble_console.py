"""Native BLE worker behind the existing loopback web interface."""
import asyncio,concurrent.futures,hashlib,json,re,secrets,threading,time
from pathlib import Path
from contextlib import asynccontextmanager
from ble_protocol import SERVICE,COMMAND,NOTICE,IDENTITY,STATUS,parse_status,reset_command,frame,verify_configuration

class BLEConsole:
 def __init__(self,root,client_factory=None,scanner=None):
  self.root=Path(root);meta=json.loads((self.root/'presets/manifest.json').read_text());self.roles={r['id']:r for r in meta['roles']}
  self.builtin_avatars=[]
  for item in meta.get('builtin_avatars',[]):
   if not re.fullmatch(r'[1-6]-avatar\.png',item.get('avatar','')):continue
   asset=self.root/'presets'/item['avatar']
   if asset.is_file() and hashlib.sha256(asset.read_bytes()).hexdigest()==item.get('sha256'):self.builtin_avatars.append(item)
  self.registry={};self.inspected={};self.devices={};self.advertisements={};self.lock=threading.RLock();self.loop=None
  if client_factory is None:
   from bleak import BleakClient,BleakScanner
   client_factory=BleakClient;scanner=BleakScanner.discover
  self.client_factory=client_factory;self.scanner=scanner;self.slots=asyncio.Semaphore(2);self.device_locks={}
 def start_worker(self):
  ready=threading.Event()
  def worker():
   self.loop=asyncio.new_event_loop();asyncio.set_event_loop(self.loop);ready.set();self.loop.run_forever()
  self.thread=threading.Thread(target=worker,daemon=True);self.thread.start();ready.wait(5)
 def call(self,coro,timeout=240):
  if self.loop is None:coro.close();raise RuntimeError('蓝牙服务尚未启动')
  f=asyncio.run_coroutine_threadsafe(coro,self.loop)
  try:return f.result(timeout)
  except concurrent.futures.TimeoutError:f.cancel();raise TimeoutError('蓝牙操作超时，请唤醒设备后重新扫描')
 def close(self):
  if self.loop:self.loop.call_soon_threadsafe(self.loop.stop);self.thread.join(5)
 def _port(self,port):
  with self.lock:
   if not isinstance(port,str) or port not in self.devices:raise ValueError('请先扫描并选择附近设备')
   if not self.advertisements[port].get('seen_this_scan',False):raise ValueError('本轮没有发现这台设备，请唤醒并重新扫描后操作')
 def list_ports(self):
  with self.lock:
   return [{'port':p,'ble_name':self.advertisements[p]['name'],'rssi':self.advertisements[p]['rssi'],'info':self.inspected.get(p),'assigned_role':self.inspected.get(p,{}).get('role'),'read_error':self.advertisements[p].get('read_error',''),'seen_this_scan':self.advertisements[p].get('seen_this_scan',False)} for p in self.devices]
 def scan(self):return self.call(self.scan_async(),180)
 async def scan_async(self):
  with self.lock:
   for ad in self.advertisements.values():ad['seen_this_scan']=False
  found={}
  # Advertising and scan responses may be missed in a short macOS scan.
  # Merge three fresh rounds; retain previous rows as explicitly unavailable.
  for _ in range(3):
   rows=await self.scanner(timeout=7.0,return_adv=True,service_uuids=None)
   # The SN is in advertising data; the service UUID is in a separate scan
   # response. macOS/Bleak service filtering can discard SN-only events.
   # Candidates are verified through our exact GATT identity/status before use.
   result={}
   for port,(device,ad) in rows.items():
    name=(ad.local_name or device.name or '').lower()
    uuids=[u.lower() for u in getattr(ad,'service_uuids',[])]
    if SERVICE in uuids or re.fullmatch(r'[0-9a-f]{12}',name) or port in self.devices:result[port]=(device,ad)
   found.update(result)
   with self.lock:
    for port,(device,ad) in result.items():
     self.devices[port]=device
     self.advertisements[port]={'name':ad.local_name or device.name or 'AI PASSPORT','rssi':ad.rssi,'seen_this_scan':True}
     self.inspected.pop(port,None)
  async def read_current(port):
   try:await self.inspect_async(port)
   except Exception as e:
    message=str(e) or '无法连接或读取当前资料'
    if 'Peer removed pairing information' in message or ('CBErrorDomain' in message and 'Code=14' in message):
     message='macOS报告配对信息已失效，请在系统蓝牙设置中移除此设备的旧配对记录后重新扫描；设备资料不会被清空'
    with self.lock:self.advertisements[port]['read_error']=message
  await asyncio.gather(*(read_current(port) for port in found))
  return self.list_ports()
 def inspect(self,port,progress=lambda s:None):return self.call(self.inspect_async(port,progress))
 async def _read(self,c):return parse_status(await asyncio.wait_for(c.read_gatt_char(STATUS),10))
 def _cache(self,port,info):
  # Custom portraits require a matching pixel CRC. A zero CRC means the
  # firmware uses its compiled portrait, verified separately for each version.
  info['avatar_url']='';info['avatar_source']=''
  if info.get('supported') and info.get('image_crc'):
   match=next((r for r in self.roles.values() if r.get('image_crc')==info['image_crc']),None)
   if match:info['avatar_url']='/presets/'+match['avatar'];info['avatar_source']='matched_crc'
  elif info.get('supported'):
   match=next((r for r in self.builtin_avatars if r['role']==info.get('role') and info.get('version') in r['versions']),None)
   if match:info['avatar_url']='/presets/'+match['avatar'];info['avatar_source']='builtin'
  with self.lock:
   self.inspected[port]=info
   self.advertisements.get(port,{}).pop('read_error',None)
 def _device(self,port):
  self._port(port)
  with self.lock:return self.devices[port]
 def _device_lock(self,port):return self.device_locks.setdefault(port,asyncio.Lock())
 @asynccontextmanager
 async def _session(self,device):
  c=self.client_factory(device,timeout=20)
  try:
   await asyncio.wait_for(c.__aenter__(),25)
   yield c
  finally:
   try:await asyncio.wait_for(c.__aexit__(None,None,None),5)
   except asyncio.TimeoutError:raise TimeoutError('蓝牙断开超时，请关闭该台设备后重新扫描')
 async def inspect_async(self,port,progress=lambda s:None):
  device=self._device(port)
  async with self._device_lock(port),self.slots:
   progress('正在通过蓝牙识别，不会重启设备')
   async with self._session(device) as c:
    try:info=await self._read(c)
    except Exception as error:
     # Old firmware retains its identity characteristic but has no management API.
     try:b=bytes(await asyncio.wait_for(c.read_gatt_char(IDENTITY),8))
     except Exception:raise error
     if len(b)!=72 or b[0]!=1:raise error
     sn=b[2:26].split(b'\0',1)[0].decode('utf8','replace')
     info={'sn':sn,'sn_source':'设备蓝牙身份','mac':'','role':0,'name':sn,'version':'旧版','supported':False,'shared_roles':True}
    self._cache(port,info);return info
 def execute(self,action,port,mac,role,progress=lambda s:None,replace=False):return self.call(self.execute_async(action,port,mac,role,progress))
 async def execute_async(self,action,port,mac,role,progress=lambda s:None):
  if action not in ('restore','clear') or type(role)!=int or role not in self.roles:raise ValueError('操作或人物不正确')
  with self.lock:seen=dict(self.inspected.get(port,{}))
  if not seen.get('supported') or seen.get('mac')!=mac:raise ValueError('请先识别设备，并升级支持蓝牙管理的固件')
  device=self._device(port)
  async with self._device_lock(port),self.slots:
   progress('正在连接蓝牙并核对 SN')
   async with self._session(device) as c:
    before=await self._read(c)
    if before['mac']!=mac or before['sn']!=seen['sn']:raise ValueError('设备身份发生变化，已停止')
    if before['state'] in (1,2):raise ValueError('设备还在完成上次清理，请稍后重新识别')
    target=self.roles[role] if action=='restore' else {'id':before['role'],'name':before['name'],'signature':before['signature'],'image_crc':before['image_crc']}
    if action=='clear' and role!=before['role']:raise ValueError('清空数据请使用当前人物')
    if action=='restore':
     image=(self.root/'presets'/target['avatar']).read_bytes()
     if target.get('avatar_sha256') and hashlib.sha256(image).hexdigest()!=target['avatar_sha256']:raise ValueError('预设头像校验失败')
     if not 0<len(image)<=65536 or 'image_crc' not in target:raise ValueError('预设头像不完整')
    q=asyncio.Queue()
    await c.start_notify(NOTICE,lambda _,b:q.put_nowait(bytes(b)))
    async def send(kind,payload,expected):
     wire=frame(kind,payload);chunk=min(244,max(20,c.mtu_size-3))
     for at in range(0,len(wire),chunk):await asyncio.wait_for(c.write_gatt_char(COMMAND,wire[at:at+chunk],response=True),10)
     reply=await asyncio.wait_for(q.get(),12)
     if len(reply)!=2 or reply[0]!=kind or reply[1]!=expected:raise ValueError('设备拒绝操作：'+reply.hex()+'，未确认完成')
    txn=secrets.randbelow(0xffffffff)+1
    await send(0x30,reset_command(before,role,txn,action),0)
    end=time.monotonic()+60
    while True:
     await asyncio.sleep(.2);current=await self._read(c)
     if current['mac']!=mac or current['boot']!=before['boot']:raise ValueError('设备断线或重启，未确认完成')
     if current['txn']==txn:
      progress(f"正在清空测试数据 · {current['progress']}%")
      if current['state']==4:raise ValueError('设备清理失败，重新连接后可重试')
      if current['state']==3:break
     if time.monotonic()>end:raise TimeoutError('设备清理超时，未确认完成')
    if action=='restore':
     progress('正在设置昵称和签名')
     data=json.dumps({'nickname':target['name'],'intro':target['signature'],'img_mode':'avatar'},ensure_ascii=False,separators=(',',':')).encode()
     await send(1,data,1);await send(2,b'\0'+len(image).to_bytes(4,'little'),0)
     for at in range(0,len(image),239):
      await send(2,b'\1'+image[at:at+239],0)
      if at%2390==0:progress(f'正在传送头像 · {at*100//len(image)}%')
     await send(2,b'\2',1)
    progress('正在回读人物、清理结果和开机标记')
    # Firmware publishes snapshots at 100ms intervals; allow the new image commit to appear.
    for _ in range(12):
     after=await self._read(c)
     try:verify_configuration(before,after,target,txn);break
     except ValueError:
      if _==11:raise
      await asyncio.sleep(.15)
    self._cache(port,after)
    return {'verified':True,'action':action,'mac':mac,'sn':after['sn'],'role':after['role'],'name':after['name'],'version':after['version'],'boot':after['boot'],'device_restarted':False,'firmware_written':False}
