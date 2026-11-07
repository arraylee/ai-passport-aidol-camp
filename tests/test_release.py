import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from release_layout import validate_segments

class LayoutTests(unittest.TestCase):
    def segments(self):
        return {'bootloader.bin':(0,(ROOT/'build/bootloader/bootloader.bin').read_bytes()),
                'partition-table.bin':(0x8000,(ROOT/'build/partition_table/partition-table.bin').read_bytes()),
                'app.bin':(0x10000,(ROOT/'build/passport_hideseek_radio.bin').read_bytes())}
    def test_actual_build(self):
        self.assertEqual(validate_segments(self.segments())['cardid'][2:],(0x356000,0x4000))
    def test_reject_overflow(self):
        s=self.segments();s['app.bin']=(0x10000,b'\xe9'+bytes(0x350000))
        with self.assertRaises(ValueError):validate_segments(s)
    def test_reject_identity_image(self):
        s=self.segments();s['identity.bin']=(0x356000,b'x')
        with self.assertRaises(ValueError):validate_segments(s)
    def test_reject_shifted_app(self):
        s=self.segments();s['app.bin']=(0,s['app.bin'][1])
        with self.assertRaises(ValueError):validate_segments(s)
    def test_reject_modified_partition(self):
        s=self.segments();b=bytearray(s['partition-table.bin'][1]);b[4]^=1;s['partition-table.bin']=(0x8000,bytes(b))
        with self.assertRaises(ValueError):validate_segments(s)
if __name__=='__main__':unittest.main()
