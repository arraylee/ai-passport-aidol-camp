"""Loopback-only desktop panel; USB mutations require a per-launch browser token."""
from pathlib import Path
from http.server import ThreadingHTTPServer,BaseHTTPRequestHandler
from urllib.parse import urlsplit
import argparse,copy,json,secrets,threading,time,webbrowser,sys
from ble_console import BLEConsole
from concurrent.futures import ThreadPoolExecutor

class AlreadyRunning(Exception):
 def __init__(self,url):self.url=url

class InstanceLease:
 def __init__(self,state):
  import fcntl
  self.state=Path(state);self.state.mkdir(parents=True,exist_ok=True);self.file=(self.state/'console.lock').open('a+')
  try:fcntl.flock(self.file,fcntl.LOCK_EX|fcntl.LOCK_NB)
  except BlockingIOError:
   self.file.close();url=None
   try:
    import re
    candidate=json.loads((self.state/'service.json').read_text())['url']
    if re.fullmatch(r'http://127\.0\.0\.1:[0-9]{1,5}',candidate):url=candidate
   except (OSError,ValueError,KeyError,TypeError):pass
   raise AlreadyRunning(url)
 def publish(self,url):(self.state/'service.json').write_text(json.dumps({'url':url})+'\n')
 def close(self):self.file.close()

class WorkQueue:
 def __init__(self,console):self.console=console;self.lock=threading.Lock();self.job=None
 def snapshot(self):
  with self.lock:return copy.deepcopy(self.job)
 def start(self,action,targets):
  if action not in ['scan','inspect','restore','clear'] or not isinstance(targets,list) or not ((action=='scan' and not targets) or (action!='scan' and 1<=len(targets)<=6)):raise ValueError('操作参数不正确')
  if len({t.get('port') for t in targets})!=len(targets):raise ValueError('不能重复选择同一串口')
  if action=='restore' and len({t.get('role') for t in targets})!=len(targets) and not all(getattr(self.console,'inspected',{}).get(t.get('port'),{}).get('shared_roles') for t in targets):raise ValueError('旧版固件同一批设备不能选择重复角色，请先升级到v0.6.0')
  for t in targets:
   if not isinstance(t,dict) or not isinstance(t.get('port'),str):raise ValueError('设备参数错误')
   self.console._port(t['port'])
   if action!='inspect' and (type(t.get('role'))!=int or t['role'] not in self.console.roles or not isinstance(t.get('mac'),str)):raise ValueError('先识别设备并选择角色')
  with self.lock:
   if self.job and not self.job['done']:raise RuntimeError('已有任务正在执行，请稍候')
   self.job={'id':secrets.token_hex(8),'action':action,'done':False,'items':[{**t,'status':'queued','stage':'等待处理'} for t in targets] if action!='scan' else [{'port':'scan','status':'queued','stage':'正在扫描附近设备'}]}
  threading.Thread(target=self._run,daemon=True).start();return self.snapshot()
 def _run(self):
  def run(item):
   def stage(text):
    with self.lock:item['stage']=text
   with self.lock:item['status']='running'
   try:
    if self.job['action']=='scan':result={'devices':self.console.scan()}
    elif self.job['action']=='inspect':result=self.console.inspect(item['port'],stage)
    else:result=self.console.execute(self.job['action'],item['port'],item['mac'],item['role'],stage,bool(item.get('replace')))
    label='扫描完成' if self.job['action']=='scan' else '已识别' if self.job['action']=='inspect' else '已完成，蓝牙回读一致，设备未重启'
    with self.lock:item.update(status='success',stage=label,result=result)
   except Exception as e:
    with self.lock:item.update(status='error',stage=str(e))
  with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(run,self.job['items']))
  with self.lock:self.job['done']=True

def make_server(console,host='127.0.0.1',port=0):
 root=Path(__file__).resolve().parent;token=secrets.token_urlsafe(32);queue=WorkQueue(console)
 class Handler(BaseHTTPRequestHandler):
  def log_message(self,*a):pass
  def send(self,code,body,kind='application/json; charset=utf-8'):
   if not isinstance(body,bytes):body=json.dumps(body,ensure_ascii=False).encode()
   self.send_response(code);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(body)));self.send_header('Cache-Control','no-store');self.send_header('X-Content-Type-Options','nosniff');self.send_header('Referrer-Policy','no-referrer');self.send_header('Content-Security-Policy',"default-src 'self'; style-src 'self'; script-src 'self'; img-src 'self'; frame-ancestors 'none'");self.end_headers();self.wfile.write(body)
  def valid_host(self):return self.headers.get('Host') in [f'127.0.0.1:{self.server.server_port}',f'localhost:{self.server.server_port}']
  def do_GET(self):
   if not self.valid_host():return self.send(403,{'error':'仅允许本机访问'})
   path=urlsplit(self.path).path
   if path=='/api/status':
    roles=[{k:r[k] for k in ['id','name','signature','avatar']} for r in console.roles.values()]
    return self.send(200,{'devices':console.list_ports(),'roles':roles,'bindings':dict(console.registry),'job':queue.snapshot()})
   if path=='/':data=(root/'web/index.html').read_bytes().replace(b'{{TOKEN}}',token.encode());return self.send(200,data,'text/html; charset=utf-8')
   if path in ['/app.js','/app.css']:
    return self.send(200,(root/'web'/path[1:]).read_bytes(),'text/javascript; charset=utf-8' if path.endswith('.js') else 'text/css; charset=utf-8')
   import re
   if re.fullmatch(r'/presets/[1-6]-avatar\.png',path):return self.send(200,(root/path[1:]).read_bytes(),'image/png')
   return self.send(404,{'error':'页面不存在'})
  def do_POST(self):
   if not self.valid_host():return self.send(403,{'error':'仅允许本机访问'})
   origins=[f'http://127.0.0.1:{self.server.server_port}',f'http://localhost:{self.server.server_port}']
   if self.headers.get('Origin') not in [None,*origins] or not secrets.compare_digest(self.headers.get('X-Passport-Token',''),token):return self.send(403,{'error':'操作校验失败，请从工具首页重试'})
   if self.headers.get_content_type()!='application/json':return self.send(415,{'error':'请求格式错误'})
   try:n=int(self.headers.get('Content-Length','0'))
   except ValueError:return self.send(400,{'error':'请求长度错误'})
   if not 1<=n<=16384:return self.send(413,{'error':'请求过大'})
   try:
    data=json.loads(self.rfile.read(n));path=urlsplit(self.path).path
    if path!='/api/start':return self.send(404,{'error':'操作不存在'})
    result=queue.start(data.get('action'),data.get('targets'));return self.send(202,{'job':result})
   except (ValueError,TypeError,AttributeError) as e:return self.send(400,{'error':str(e)})
   except RuntimeError as e:return self.send(409,{'error':str(e)})
 server=ThreadingHTTPServer((host,port),Handler);server.token=token;server.queue=queue;return server

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--no-browser',action='store_true');ap.add_argument('--port',type=int,default=0);a=ap.parse_args()
 root=Path(__file__).resolve().parent;state=Path.home()/'Library/Application Support/AI Passport BLE Console'
 try:lease=InstanceLease(state)
 except AlreadyRunning as existing:
  print('助手已在运行：'+(existing.url or '请回到已打开的助手页面'),flush=True)
  if existing.url and not a.no_browser:webbrowser.open(existing.url)
  return
 try:console=BLEConsole(root);console.start_worker();server=make_server(console,port=a.port)
 except Exception:lease.close();raise
 url=f'http://127.0.0.1:{server.server_port}';lease.publish(url)
 print('AI PASSPORT 测试助手：'+url,flush=True);print('扫描附近设备后识别 SN，选择角色，再设置并重置。无需 USB，不会重启设备。',flush=True)
 if not a.no_browser:webbrowser.open(url)
 try:server.serve_forever(poll_interval=.2)
 except KeyboardInterrupt:
  job=server.queue.snapshot()
  if job and not job['done']:
   print('正在执行设备操作，请等待校验完成。',flush=True)
   while not server.queue.snapshot()['done']:time.sleep(.2)
 finally:server.server_close();console.close();lease.close()
if __name__=='__main__':main()
