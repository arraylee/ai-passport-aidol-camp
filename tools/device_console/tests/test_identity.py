import sys,unittest,tempfile,subprocess,importlib.util
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from nvs_identity import public_sn
class IdentityTests(unittest.TestCase):
 def test_invalid_identity_matches_firmware_public_fallback(self):
  self.assertEqual(public_sn(b'\xff'*0x4000,'aa:bb:cc:dd:ee:ff'),{'sn':'aabbccddeeff','sn_source':'firmware MAC fallback'})
 @unittest.skipUnless(importlib.util.find_spec('esp_idf_nvs_partition_gen'),'NVS generator not installed in this test interpreter')
 def test_reads_real_nvs_devicekey_without_returning_secret(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td);(p/'test.csv').write_text('key,type,encoding,value\nfolotoy-key,namespace,,\nDeviceKey,data,string,SN-TEST-001\nDeviceSecret,data,string,FAKE-TEST-ONLY\n')
   subprocess.run([sys.executable,'-m','esp_idf_nvs_partition_gen','generate',str(p/'test.csv'),str(p/'identity.bin'),'0x4000'],stdout=subprocess.DEVNULL,check=True)
   raw=(p/'identity.bin').read_bytes();r=public_sn(raw,'aa:bb:cc:dd:ee:ff');self.assertEqual(r,{'sn':'SN-TEST-001','sn_source':'DeviceKey'});self.assertNotIn('FAKE',str(r))
   b=bytearray(raw);at=b.index(b'SN-TEST-001');b[at]^=1
   self.assertEqual(public_sn(b,'aa:bb:cc:dd:ee:ff')['sn_source'],'firmware MAC fallback')
if __name__=='__main__':unittest.main()
