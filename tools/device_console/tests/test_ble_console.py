import unittest,sys,asyncio,struct,json,tempfile,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]));sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_ble_protocol import status
from ble_protocol import STATUS,COMMAND,NOTICE
from ble_console import BLEConsole
class FakeDevice:
 address='11111111-1111-1111-1111-111111111111'
 name='98c377f41e1c'
class Radio:
 def __init__(self):self.b=bytearray(status());self.wire=bytearray();self.payload=bytearray();self.notify=None;self.fail=False;self.drop=False;self.connected=False;self.max_active=0;self.active=0
 def client(self,dev,**kwargs):return Link(self)
 async def scan(self,**kwargs):return {FakeDevice.address:(FakeDevice(),type('Adv',(),{'local_name':FakeDevice.name,'rssi':-35})())}
class Link:
 mtu_size=100
 def __init__(self,r):self.r=r
 async def __aenter__(self):self.r.connected=True;return self
 async def __aexit__(self,*a):self.r.connected=False
 async def read_gatt_char(self,uuid):return bytes(self.r.b)
 async def start_notify(self,uuid,cb):self.r.notify=cb
 async def write_gatt_char(self,uuid,b,response=True):
  r=self.r
  if r.drop and b[:2]==b"\x01\x02":raise ConnectionError("device disconnected")
  r.wire+=b
  if len(r.wire)<4:return
  wanted=4+int.from_bytes(r.wire[2:4],'little')
  if len(r.wire)<wanted:return
  kind=r.wire[1];data=bytes(r.wire[4:]);r.wire.clear();code=0
  if r.fail:code=0x16
  elif kind==0x30:
   r.b[5]=data[2];r.b[6:8]=bytes([90,0]);r.b[213]=3;r.b[214]=100;r.b[209:213]=data[8:12];struct.pack_into('<I',r.b,197,int.from_bytes(r.b[197:201],'little')+1)
  elif kind==1:
   d=json.loads(data);r.b[55:104]=d['nickname'].encode().ljust(49,b'\0');r.b[104:189]=d['intro'].encode().ljust(85,b'\0');code=1
  elif kind==2:
   if data[0]==0:r.payload.clear()
   elif data[0]==1:r.payload+=data[1:]
   elif data[0]==2:struct.pack_into('<I',r.b,205,1234);code=1
  r.notify(None,bytes([kind,code]))
class BLETests(unittest.IsolatedAsyncioTestCase):
 async def asyncSetUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name);(self.root/'presets').mkdir();(self.root/'presets/2-avatar.png').write_bytes(b'PNGtest')
  self.roles=[{'id':2,'name':'Bob','signature':'bio','avatar':'2-avatar.png','image_crc':1234}];(self.root/'presets/manifest.json').write_text(json.dumps({'roles':self.roles}));self.radio=Radio();self.console=BLEConsole(self.root,self.radio.client,self.radio.scan)
 async def test_scan_accepts_sn_advertisements_without_service_response_and_ignores_unrelated(self):
  unrelated=type('Other',(),{'address':'unrelated','name':'Headphones'})()
  unrelated_ad=type('Adv',(),{'local_name':'Headphones','rssi':-50,'service_uuids':[]})()
  async def broad(**kwargs):
   if kwargs.get('service_uuids'):return {}
   rows=await self.radio.scan();rows[unrelated.address]=(unrelated,unrelated_ad);return rows
  self.console.scanner=broad
  rows=await self.console.scan_async()
  self.assertEqual([x['port'] for x in rows],[FakeDevice.address])
 async def test_scan_merges_rounds_and_keeps_missing_devices_explicitly_offline(self):
  other=type('Other',(),{'address':'22222222-2222-2222-2222-222222222222','name':'98c377f6d274'})()
  adv=type('Adv',(),{'local_name':'98c377f6d274','rssi':-50})()
  calls=0;empty=False
  async def alternating(**kwargs):
   nonlocal calls
   calls+=1
   if empty:return {}
   return await self.radio.scan() if calls%2 else {other.address:(other,adv)}
  self.console.scanner=alternating
  devices=await self.console.scan_async()
  self.assertEqual(len(devices),2)
  self.assertTrue(all(d['seen_this_scan'] for d in devices))
  empty=True;missing=await self.console.scan_async()
  self.assertEqual(len(missing),2)
  self.assertTrue(all(not d['seen_this_scan'] for d in missing))
  with self.assertRaises(ValueError):self.console._port(FakeDevice.address)
 async def test_pairing_information_error_is_explained_per_device(self):
  class BadLink(Link):
   async def __aenter__(self):raise RuntimeError('CBErrorDomain Code=14 Peer removed pairing information')
  self.console.client_factory=lambda *a,**k:BadLink(self.radio)
  devices=await self.console.scan_async()
  self.assertEqual(len(devices),1)
  self.assertIn('配对',devices[0]['read_error'])
  self.assertIsNone(devices[0]['info'])
 async def test_scan_reads_actual_profile_instead_of_guessing_from_default_role(self):
  devices=await self.console.scan_async()
  self.assertIsNotNone(devices[0]['info'])
  self.assertEqual(devices[0]['info']['name'],'Bob')
  self.assertEqual(devices[0]['info']['avatar_url'],'/presets/2-avatar.png')
 async def test_unknown_avatar_is_not_replaced_with_a_preset(self):
  struct.pack_into('<I',self.radio.b,205,999)
  await self.console.scan_async();info=await self.console.inspect_async(FakeDevice.address)
  self.assertEqual(info.get('avatar_url'),'')
 async def test_builtin_avatar_requires_matching_firmware_and_no_custom_image(self):
  meta={'roles':self.roles,'builtin_avatars':[{'role':2,'versions':['0.7.0'],'avatar':'2-avatar.png','sha256':hashlib.sha256(b'PNGtest').hexdigest()}]}
  (self.root/'presets/manifest.json').write_text(json.dumps(meta))
  console=BLEConsole(self.root,self.radio.client,self.radio.scan)
  struct.pack_into('<I',self.radio.b,205,0)
  rows=await console.scan_async()
  self.assertEqual(rows[0]['info']['avatar_url'],'/presets/2-avatar.png')
  self.assertEqual(rows[0]['info']['avatar_source'],'builtin')
  self.radio.b[39:44]=b'9.9.9'
  rows=await console.scan_async()
  self.assertEqual(rows[0]['info']['avatar_url'],'')
  self.radio.b[39:44]=b'0.7.0';struct.pack_into('<I',self.radio.b,205,999)
  rows=await console.scan_async()
  self.assertEqual(rows[0]['info']['avatar_url'],'')
 async def test_builtin_avatar_with_changed_asset_stays_unavailable(self):
  meta={'roles':self.roles,'builtin_avatars':[{'role':2,'versions':['0.7.0'],'avatar':'2-avatar.png','sha256':'0'*64}]}
  (self.root/'presets/manifest.json').write_text(json.dumps(meta))
  console=BLEConsole(self.root,self.radio.client,self.radio.scan)
  struct.pack_into('<I',self.radio.b,205,0)
  rows=await console.scan_async()
  self.assertEqual(rows[0]['info']['avatar_url'],'')
 async def test_scan_inspect_restore_and_readback_keep_boot(self):
  await self.console.scan_async();info=await self.console.inspect_async(FakeDevice.address);before=info['boot'];result=await self.console.execute_async('restore',FakeDevice.address,info['mac'],2)
  self.assertTrue(result['verified']);self.assertEqual(result['boot'],before);self.assertEqual(self.radio.payload,b'PNGtest');self.assertFalse(self.radio.connected)
 async def test_clear_does_not_replace_current_portrait(self):
  await self.console.scan_async();info=await self.console.inspect_async(FakeDevice.address);result=await self.console.execute_async('clear',FakeDevice.address,info['mac'],2)
  self.assertTrue(result['verified']);self.assertFalse(self.radio.payload)
 async def test_wrong_identity_and_device_errors_do_not_succeed(self):
  await self.console.scan_async();info=await self.console.inspect_async(FakeDevice.address)
  with self.assertRaises(ValueError):await self.console.execute_async('restore',FakeDevice.address,'00:00:00:00:00:00',2)
  self.radio.fail=True
  with self.assertRaises(ValueError):await self.console.execute_async('restore',FakeDevice.address,info['mac'],2)
  self.assertFalse(self.radio.connected)
 async def test_disconnect_during_avatar_cannot_report_success(self):
  await self.console.scan_async();info=await self.console.inspect_async(FakeDevice.address);self.radio.drop=True
  with self.assertRaises(ConnectionError):await self.console.execute_async('restore',FakeDevice.address,info['mac'],2)
  self.assertFalse(self.radio.connected)
if __name__=='__main__':unittest.main()
