"""Public GATT protocol; no serial dependency or raw flash write capability."""
import struct
SERVICE='54524145-4341-5244-0000-000000000000'
COMMAND='54524145-4341-5244-0000-000000000010'
NOTICE='54524145-4341-5244-0000-000000000011'
IDENTITY='54524145-4341-5244-0000-000000000012'
STATUS='54524145-4341-5244-0000-000000000020'

def parse_status(raw):
 b=bytes(raw)
 if len(b)!=228 or b[:5]!=b'HBC1\x01':raise ValueError('设备未提供兼容的蓝牙管理协议，请先升级匹配固件')
 def text(at,n):
  x=b[at:at+n]
  if b'\0' not in x:raise ValueError('设备资料格式错误')
  return x.split(b'\0',1)[0].decode('utf8')
 def u(at):return struct.unpack_from('<I',b,at)[0]
 if b[5]>6 or b[6]>100 or b[213]>4 or b[214]>100:raise ValueError('设备状态无效')
 return dict(mac=':'.join(f'{x:02x}' for x in b[8:14]),sn=text(14,25),sn_source='设备蓝牙身份',version=text(39,16),role=b[5],volume=b[6],activated=bool(b[7]),name=text(55,49),signature=text(104,85),uptime=u(189),boot=u(193),reset_count=u(197),revision=u(201),image_crc=u(205),txn=u(209),state=b[213],progress=b[214],error=b[215],nonce=u(216),friends=b[220],best_valid=bool(b[221]),best_score=u(222),gallery=bool(b[226]),welcomed=bool(b[227]),supported=True,shared_roles=True)

def reset_command(info,role,txn,action):
 if type(role)!=int or not 1<=role<=6 or action not in ('restore','clear') or not 0<txn<=0xffffffff or not info['nonce']:raise ValueError('蓝牙配置参数无效')
 if action=='clear' and role!=info['role']:raise ValueError('清空数据不能更换当前人物')
 return struct.pack('<BBBBII',1,1 if action=='restore' else 2,role,0,info['nonce'],txn)+bytes.fromhex(info['mac'].replace(':',''))

def frame(kind,payload):
 if not 0<=kind<=255 or len(payload)>256:raise ValueError('蓝牙数据包过大')
 return struct.pack('<BBH',1,kind,len(payload))+payload

def verify_configuration(before,after,role,txn):
 wanted={'mac':before['mac'],'sn':before['sn'],'boot':before['boot'],'role':role['id'],'name':role['name'],'signature':role['signature'],'image_crc':role['image_crc'],'volume':90,'friends':0,'best_valid':False,'best_score':0,'gallery':False,'activated':False,'welcomed':False,'state':3,'error':0,'txn':txn,'reset_count':(before['reset_count']+1)&0xffffffff}
 for key,value in wanted.items():
  if after.get(key)!=value:raise ValueError('设备回读不一致：'+key+'，未确认完成，请重新识别后重试')
