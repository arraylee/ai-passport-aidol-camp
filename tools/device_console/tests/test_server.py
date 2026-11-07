import sys,unittest,tempfile,threading,json,urllib.request,urllib.error
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT))
from ble_console import BLEConsole
from server import make_server
class ServerTests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
  self.console=BLEConsole(ROOT,lambda p:None,lambda:[])
  self.server=make_server(self.console);self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start();self.url=f'http://127.0.0.1:{self.server.server_port}'
  self.addCleanup(self.close)
 def close(self):self.server.shutdown();self.server.server_close();self.thread.join(2)
 def request(self,path,body=None,headers=None):
  data=None if body is None else json.dumps(body).encode();r=urllib.request.Request(self.url+path,data=data,headers=headers or {})
  try:
   with urllib.request.urlopen(r) as res:return res.status,res.read()
  except urllib.error.HTTPError as e:return e.code,e.read()
 def test_page_and_readonly_status(self):
  code,body=self.request('/');self.assertEqual(code,200);self.assertIn(self.server.token.encode(),body)
  code,body=self.request('/api/status');self.assertEqual(code,200);self.assertEqual(len(json.loads(body)['roles']),6)
 def test_no_token_and_cross_origin_cannot_start(self):
  data={'action':'restore','targets':[]}
  self.assertEqual(self.request('/api/start',data,{'Content-Type':'application/json'})[0],403)
  self.assertEqual(self.request('/api/start',data,{'Content-Type':'application/json','X-Passport-Token':self.server.token,'Origin':'https://example.com'})[0],403)
  self.assertIsNone(self.server.queue.snapshot())
 def test_unknown_path_and_bad_action_fail(self):
  self.assertEqual(self.request('/../console_core.py')[0],404)
  self.assertEqual(self.request('/api/start',{'action':'erase_flash','targets':[]},{'Content-Type':'application/json','X-Passport-Token':self.server.token})[0],400)
  self.assertIsNone(self.server.queue.snapshot())
 def test_host_rebinding_fails(self):
  self.assertEqual(self.request('/api/status',headers={'Host':'evil.example'})[0],403)
if __name__=='__main__':unittest.main()
