import contextlib,io,sys,tempfile,types
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import safe_flash
segments={'bootloader.bin':(0,b'boot'),'partition-table.bin':(0x8000,b'part'),'app.bin':(0x10000,b'app')}
protected={(0x356000,0x4000):b'I'*0x4000,(0x9000,0x6000):b'N'*0x6000,(0x360000,0x6000):b'C'*0x6000,(0x380000,0x30000):b'P'*0x30000,(0x3b0000,0x30000):b'F'*0x30000,(0x400000,0xc0000):b'G'*0xc0000}

def case(mode):
 calls=[];wrote=False
 def fake_run(cmd,**kwargs):
  nonlocal wrote
  args=cmd[cmd.index('--after')+2:];calls.append(args)
  if args[0]=='flash_id':
   mac='11:22:33:44:55:66' if mode=='wrong_mac' else 'aa:bb:cc:dd:ee:ff'
   return types.SimpleNamespace(returncode=0,stdout=f'MAC: {mac}\nDetected flash size: 8MB\n')
  if args[0]=='write_flash':
   assert args[1:7]==['--flash_mode','dio','--flash_size','8MB','--flash_freq','80m']
   assert args[7::2]==['0x0','0x8000','0x10000'];
   for path,(_,data) in zip(args[8::2],segments.values()):assert Path(path).read_bytes()==data
   wrote=True
  if args[0]=='read_flash':
   addr,size=int(args[1],0),int(args[2],0)
   data=protected.get((addr,size))
   if data is None:data=next(data for _,(offset,data) in segments.items() if offset==addr and len(data)==size)
   if wrote and ((mode=='mismatch' and addr==0x360000) or (mode=='profile_mismatch' and addr==0x380000)):data=b'X'*size
   Path(args[3]).write_bytes(data)
  return types.SimpleNamespace(returncode=0,stdout='OK')
 argv=['safe_flash.py','--release','/tmp/review-dummy-release','--port','/dev/cu.REVIEW-NOT-A-DEVICE']
 if mode!='validate':argv+=['--execute','--skip-backup','--expected-mac','aa:bb:cc:dd:ee:ff']
 error=None
 with patch.object(safe_flash,'load_release',return_value=({'version':'0.2.0'},segments)),patch.object(safe_flash.subprocess,'run',side_effect=fake_run),patch.object(sys,'argv',argv),contextlib.redirect_stdout(io.StringIO()):
  try:safe_flash.main()
  except RuntimeError as e:error=str(e)
 if mode=='validate':assert not calls
 elif mode=='wrong_mac':assert not wrote and error=='MAC mismatch; no writes performed'
 elif mode in ('mismatch','profile_mismatch'):assert wrote and 'PROTECTED DATA MISMATCH: '+('demo-cfg' if mode=='mismatch' else 'profile-store') in error
 else:
  assert wrote and error is None
  reads=[a for a in calls if a[0]=='read_flash'];assert len(reads)==15
  assert not any(a[0] in ('erase_flash','erase_region') for a in calls)
 print('PASS actual safe_flash.py mocked:',mode,'subprocesses',len(calls))
for mode in ['validate','wrong_mac','success','mismatch','profile_mismatch']:case(mode)
