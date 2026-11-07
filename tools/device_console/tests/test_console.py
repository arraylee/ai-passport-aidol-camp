import sys,unittest,tempfile,json,hashlib,struct,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
try:import console_core as core
except ImportError:core=None
MAC='4c:11:ae:2f:6d:10'
class MemoryDevice:
 def __init__(self,mac=MAC):
  self.mac=mac;self.data=bytearray(b'\xff'*0x800000);self.writes=[];self.boots=0;self.bad_readback=False
  self.data[0x8000:0x9000]=(ROOT/'tests/fixtures/partition-table.bin').read_bytes().ljust(4096,b'\xff')
  self.data[0x10000:0x11000]=(ROOT/'tests/fixtures/app-header.bin').read_bytes()
  self.data[0x356000:0x35a000]=bytes(range(256))*64;self.data[0x9000:0xf000]=b'N'*0x6000
  self.data[0x380000:0x3b0000]=b'P'*0x30000;self.data[0x400000:0x4c0000]=b'G'*0xc0000
 def read(self,address,size):
  b=bytes(self.data[address:address+size])
  if self.bad_readback and self.writes and address==0x3b0000:return b'X'*size
  return b
 def write(self,images,progress):
  for address,data in images.items():self.writes.append((address,len(data)));self.data[address:address+len(data)]=data
 def boot(self):self.boots+=1
 def close(self):pass
 def __enter__(self):return self
 def __exit__(self,*args):pass
class ConsoleTests(unittest.TestCase):
 def setUp(self):
  self.assertIsNotNone(core,'Device console is not implemented')
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.dev=MemoryDevice()
  self.service=core.Console(ROOT,Path(self.tmp.name),lambda port:self.dev,lambda:[{'port':'/dev/cu.usbmodemTEST','usb_mac':MAC}])
  # Test fixture identity is intentionally synthetic, never a real SN dump.
  self.service.known_identities={};self.service.registry={MAC:1}
 def inspect(self):return self.service.inspect('/dev/cu.usbmodemTEST')
 def test_restore_writes_only_demo_regions_and_recovers_original_preset(self):
  self.inspect();before=bytes(self.dev.data);result=self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,1)
  self.assertTrue(result['verified']);self.assertEqual(self.dev.writes,[(0x380000,0x30000),(0x3b0000,0x30000),(0x400000,0xc0000),(0x360000,0x6000)])
  for a,n in [(0,0x360000),(0x366000,0x1a000),(0x4c0000,0x340000)]:self.assertEqual(self.dev.data[a:a+n],before[a:a+n])
  self.assertEqual(self.dev.data[0x380000:0x3b0000],(ROOT/'presets/1-profile.bin').read_bytes())
  self.assertEqual(self.dev.data[0x360000:0x366000],(ROOT/'presets/1-config.bin').read_bytes());self.assertGreater(self.dev.boots,0)
 def test_clear_preserves_edited_profile_and_gallery(self):
  self.inspect();profile=self.dev.read(0x380000,0x30000);gallery=self.dev.read(0x400000,0xc0000)
  self.assertTrue(self.service.execute('clear','/dev/cu.usbmodemTEST',MAC,1)['verified'])
  self.assertEqual(self.dev.read(0x380000,0x30000),profile);self.assertEqual(self.dev.read(0x400000,0xc0000),gallery)
  self.assertEqual(self.dev.writes,[(0x3b0000,0x30000),(0x360000,0x6000)])
 def test_swapped_device_is_rejected_before_write(self):
  self.inspect();self.dev.mac='4c:11:ae:2f:38:cc'
  with self.assertRaises(ValueError):self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,1)
  self.assertEqual(self.dev.writes,[])
 def test_foreign_firmware_and_partition_are_rejected(self):
  self.dev.data[0x10000:0x11000]=b'\x00'*4096
  with self.assertRaises(ValueError):self.inspect()
  self.assertEqual(self.dev.writes,[])
 def test_partition_change_after_inspection_is_rejected(self):
  self.inspect();self.dev.data[0x8004]^=1
  with self.assertRaises(ValueError):self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,1)
  self.assertEqual(self.dev.writes,[])
 def test_unlisted_port_and_invalid_role_are_rejected(self):
  for port,role in [('/tmp/device',1),('/dev/cu.usbmodemTEST',7)]:
   with self.assertRaises(ValueError):self.service.execute('restore',port,MAC,role)
  self.assertEqual(self.dev.writes,[])
 def test_readback_mismatch_is_not_reported_success(self):
  self.inspect();self.dev.bad_readback=True
  with self.assertRaises(ValueError):self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,1)
 def test_role_binding_prevents_duplicate_identity(self):
  self.inspect();self.service.registry['4c:11:ae:2f:38:cc']=2
  with self.assertRaises(ValueError):self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,2)
  self.assertEqual(self.dev.writes,[])
 def test_inspection_required_before_action(self):
  with self.assertRaises(ValueError):self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,1)
  self.assertEqual(self.dev.writes,[])
 def test_known_identity_mismatch_stops_before_write(self):
  self.service.known_identities={MAC:'0'*64}
  with self.assertRaises(ValueError):self.inspect()
  self.assertEqual(self.dev.writes,[])
 def test_corrupt_preset_stops_before_write(self):
  self.inspect();self.service.roles[1]['profile_sha256']='0'*64
  with self.assertRaises(ValueError):self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,1)
  self.assertEqual(self.dev.writes,[])
 def test_protected_mutation_after_write_cannot_report_success(self):
  self.inspect();original=self.dev.write
  def corrupt(images,progress):original(images,progress);self.dev.data[0x356000]^=1
  self.dev.write=corrupt
  with self.assertRaises(ValueError):self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,1)
  self.assertFalse((Path(self.tmp.name)/'bindings.json').exists())
 def test_role_switch_and_restore_keep_sn_and_firmware(self):
  original=self.inspect();before=self.dev.read(0,0x360000)
  switched=self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,5)
  self.assertEqual(self.service.inspected['/dev/cu.usbmodemTEST']['name'],'手工耿')
  self.assertEqual(self.inspect()['role'],5)
  restored=self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,1)
  self.assertEqual(original['sn'],switched['sn']);self.assertEqual(original['sn'],restored['sn'])
  self.assertEqual(self.dev.read(0,0x360000),before)
 def test_connected_role_owner_cannot_be_replaced(self):
  self.inspect();other='4c:11:ae:2f:38:cc';self.service.registry[other]=2
  self.service.ports=lambda:[{'port':'/dev/cu.usbmodemTEST','usb_mac':MAC},{'port':'/dev/cu.usbmodemOTHER','usb_mac':other}]
  with self.assertRaises(ValueError):self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,2,replace=True)
  self.assertEqual(self.dev.writes,[])
 def test_replacement_binding_survives_tool_restart(self):
  self.inspect();self.service.registry['4c:11:ae:2f:38:cc']=2
  self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,2,replace=True)
  restarted=core.Console(ROOT,Path(self.tmp.name),lambda port:self.dev,self.service.ports)
  self.assertEqual(restarted.registry,{MAC:2})
 def test_discovery_never_opens_serial_and_invalidates_changed_mac(self):
  self.inspect();self.service.factory=lambda p:self.fail('Discovery must not connect to the badge')
  self.assertTrue(self.service.list_ports()[0]['info'])
  self.service.ports=lambda:[{'port':'/dev/cu.usbmodemTEST','usb_mac':'4c:11:ae:2f:38:cc'}]
  self.assertIsNone(self.service.list_ports()[0]['info'])
 def test_new_identity_protocol_allows_shared_role_and_keeps_both_bindings(self):
  self.dev.data[0x10030:0x10050]=b'0.6.0'.ljust(32,b'\0')
  other='4c:11:ae:2f:38:cc';self.service.registry[other]=2
  self.assertTrue(self.inspect()['shared_roles'])
  result=self.service.execute('restore','/dev/cu.usbmodemTEST',MAC,2)
  self.assertTrue(result['verified']);self.assertEqual(self.service.registry[other],2)
  restarted=core.Console(ROOT,Path(self.tmp.name),lambda port:self.dev,self.service.ports)
  self.assertEqual(restarted.registry[MAC],2);self.assertEqual(restarted.registry[other],2)
 def test_preset_header_and_images_have_independent_crcs(self):
  meta=json.loads((ROOT/'presets/manifest.json').read_text())
  self.assertEqual(len(meta['roles']),6)
  for role in meta['roles']:
   b=(ROOT/'presets'/role['profile_file']).read_bytes();self.assertEqual(len(b),0x30000)
   self.assertEqual(hashlib.sha256(b).hexdigest(),role['profile_sha256'])
   if role['id']!=1:
    self.assertEqual(b[:4],b'HRE1');self.assertEqual(b[8:10],bytes([role['id'],1]))
    self.assertEqual(zlib.crc32(b[:220]),struct.unpack_from('<I',b,220)[0]);self.assertEqual(zlib.crc32(b[4096:61440]),struct.unpack_from('<I',b,216)[0])
    self.assertEqual(b[16:65].split(b'\0')[0].decode(),role['name'])
if __name__=='__main__':unittest.main()
