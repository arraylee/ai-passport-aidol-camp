#!/usr/bin/env python3
"""User-invoked explicit split flashing; identity/NVS/settings and segment readback."""
import argparse,contextlib,datetime,json,os,re,subprocess,sys,tempfile
from pathlib import Path
from release_layout import load_release,sha

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--release',type=Path,required=True);p.add_argument('--port',required=True)
 p.add_argument('--expected-mac',help='Required for execution: exact device MAC')
 p.add_argument('--backup-dir',type=Path);p.add_argument('--skip-backup',action='store_true',help='Explicitly skip full backup; temporary protected-data comparison still mandatory')
 p.add_argument('--execute',action='store_true');a=p.parse_args()
 manifest,segments=load_release(a.release)
 if any(c in a.port for c in '*?[]'):p.error('Use one resolved port')
 print('Validated: only 0x0 bootloader, 0x8000 partitions, 0x10000 app. SN/NVS/demo_cfg/profile_store excluded.')
 if not a.execute:print('Validation only; no device connected.');return
 if not a.expected_mac or not re.fullmatch(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}',a.expected_mac):p.error('Execution requires --expected-mac')
 if not a.skip_backup and not a.backup_dir:p.error('Use --backup-dir or explicitly --skip-backup')
 def run(*args):
  r=subprocess.run([sys.executable,'-m','esptool','--chip','esp32c3','--port',a.port,'--baud','460800','--after','no_reset',*map(str,args)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  if r.returncode:raise RuntimeError(r.stdout)
  return r.stdout
 info=run('flash_id')
 if not re.search(r'Detected flash size:\s*8MB',info):raise RuntimeError('Expected 8 MB flash; no writes performed')
 mac=re.search(r'MAC:\s*([0-9a-fA-F:]{17})',info)
 if not mac or mac[1].lower()!=a.expected_mac.lower():raise RuntimeError('MAC mismatch; no writes performed')
 if a.skip_backup:
  ctx=tempfile.TemporaryDirectory(prefix='hr-protected-compare-')
 else:
  a.backup_dir.mkdir(parents=True,exist_ok=True)
  path=Path(tempfile.mkdtemp(prefix=datetime.datetime.now().strftime('%Y%m%d-%H%M%S-'),dir=a.backup_dir));os.chmod(path,0o700)
  ctx=contextlib.nullcontext(str(path))
 with ctx as folder:
  root=Path(folder)
  def read(name,address,size):
   f=root/name;run('read_flash',hex(address),hex(size),f);data=f.read_bytes()
   if len(data)!=size:raise RuntimeError('Incomplete read: '+name)
   return data
  protected={'identity':(0x356000,0x4000),'original-nvs':(0x9000,0x6000),'demo-cfg':(0x360000,0x6000),'profile-store':(0x380000,0x30000),'friend-store':(0x3b0000,0x30000),'gallery-store':(0x400000,0xc0000)}
  before={name:read(name+'-before.bin',address,size) for name,(address,size) in protected.items()}
  if not a.skip_backup:
   full=read('full-8mb.bin',0,0x800000)
   for name,(address,size) in protected.items():
    if full[address:address+size]!=before[name]:raise RuntimeError('Backup mismatch; no writes performed')
  args=['write_flash','--flash_mode','dio','--flash_size','8MB','--flash_freq','80m']
  for name,(address,data) in segments.items():
   staged=root/('verified-'+name);staged.write_bytes(data);args.extend([hex(address),staged])
  run(*args)
  for name,(address,size) in protected.items():
   if read(name+'-after.bin',address,size)!=before[name]:raise RuntimeError('PROTECTED DATA MISMATCH: '+name+'; stop and investigate')
  for name,(address,expected) in segments.items():
   if read(name+'.readback',address,len(expected))!=expected:raise RuntimeError('Firmware readback mismatch: '+name)
  report={'version':manifest['version'],'mac':mac[1].lower(),'full_backup':not a.skip_backup,'protected_sha256':{name:sha(data) for name,data in before.items()},'segments_readback_equal':True,'physical_acceptance':'pending'}
  if not a.skip_backup:(root/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
  print(json.dumps(report,indent=2))
 print('Flash/readback verified. Reconnect power to boot. Button/audio/radio/UI physical acceptance remains pending.')
if __name__=='__main__':main()
