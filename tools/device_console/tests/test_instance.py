import sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from server import InstanceLease,AlreadyRunning
class InstanceTests(unittest.TestCase):
 def test_second_launch_reuses_existing_url_and_release_allows_restart(self):
  with tempfile.TemporaryDirectory() as td:
   first=InstanceLease(td)
   try:
    first.publish('http://127.0.0.1:12345')
    with self.assertRaises(AlreadyRunning) as e:InstanceLease(td)
    self.assertEqual(e.exception.url,'http://127.0.0.1:12345')
   finally:first.close()
   restarted=InstanceLease(td);restarted.close()
 def test_existing_url_cannot_redirect_to_external_page(self):
  with tempfile.TemporaryDirectory() as td:
   first=InstanceLease(td)
   try:
    first.publish('https://example.com')
    with self.assertRaises(AlreadyRunning) as e:InstanceLease(td)
    self.assertIsNone(e.exception.url)
   finally:first.close()
if __name__=='__main__':unittest.main()
