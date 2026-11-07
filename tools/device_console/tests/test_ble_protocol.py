import unittest,sys,struct
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from ble_protocol import parse_status,reset_command,frame,verify_configuration

def status():
 b=bytearray(228);b[:4]=b'HBC1';b[4:8]=bytes([1,2,90,0]);b[8:14]=bytes.fromhex('98c377f41e1c');b[14:26]=b'98c377f41e1c';b[39:44]=b'0.7.0';b[55:58]=b'Bob';b[104:107]=b'bio';struct.pack_into('<IIIIII',b,189,1000,99,4,2,1234,77);b[213:216]=bytes([3,100,0]);struct.pack_into('<I',b,216,42);return bytes(b)
class ProtocolTests(unittest.TestCase):
 def test_status_fixed_vector(self):
  s=parse_status(status());self.assertEqual((s['mac'],s['sn'],s['role'],s['name'],s['image_crc'],s['boot']),("98:c3:77:f4:1e:1c","98c377f41e1c",2,'Bob',1234,99))
 def test_reject_malformed_status(self):
  for b in [status()[:-1],b'BAD!'+status()[4:],status()[:4]+b'\x02'+status()[5:]]:
   with self.assertRaises(ValueError):parse_status(b)
 def test_command_binds_nonce_transaction_action_mac(self):
  s=parse_status(status());self.assertEqual(reset_command(s,2,77,'clear'),bytes.fromhex('010202002a0000004d00000098c377f41e1c'))
  self.assertEqual(frame(0x30,b'abc'),bytes.fromhex('01300300616263'))
  with self.assertRaises(ValueError):reset_command(s,0,77,'restore')
 def test_readback_requires_same_boot_and_complete_reset_and_real_profile(self):
  before=parse_status(status());after={**before,'reset_count':5};role={'id':2,'name':'Bob','signature':'bio','image_crc':1234};verify_configuration(before,after,role,77)
  for k,v in [('boot',100),('mac','00:00:00:00:00:00'),('role',3),('name','wrong'),('image_crc',0),('friends',2),('activated',True),('best_valid',True),('gallery',True),('state',4),('txn',78),('reset_count',4),('volume',50)]:
   bad={**after,k:v}
   with self.subTest(field=k),self.assertRaises(ValueError):verify_configuration(before,bad,role,77)
if __name__=='__main__':unittest.main()
