"""One serial connection for identity check, bounded reads and data-only writes."""
import contextlib,io,types
from serial.tools import list_ports
from esptool.targets.esp32c3 import ESP32C3ROM
import esptool
from console_core import PARTS

def ports():
 out=[]
 for p in list_ports.comports():
  if p.vid==0x303a and p.pid==0x1001 and p.device.startswith('/dev/cu.usbmodem'):
   serial=(p.serial_number or '').lower();out.append({'port':p.device,'usb_mac':serial if serial.count(':')==5 else ''})
 return sorted(out,key=lambda p:p['port'])
class FlashDevice:
 def __init__(self,port):
  self.esp=None;self.mac=''
  try:
   with contextlib.redirect_stdout(io.StringIO()):
    esp=ESP32C3ROM(port,115200);self.esp=esp;esp.connect(attempts=3)
    self.mac=':'.join(f'{b:02x}' for b in esp.read_mac());esp=esp.run_stub();self.esp=esp
    esp.flash_spi_attach(0);esp.change_baud(460800);esp._port.timeout=5
    if ((esp.flash_id()>>16)&0xff)!=23:raise ValueError('不是8MB设备，已停止')
  except Exception:self.close();raise
 def read(self,address,size):
  if address<0 or size<1 or address+size>0x800000:raise ValueError('读取范围错误')
  b=b''.join(self.esp.read_flash(address+n,min(65536,size-n)) for n in range(0,size,65536))
  if len(b)!=size:raise ValueError('串口读取不完整')
  return b
 def write(self,images,progress):
  if any((a,len(b)) not in PARTS.values() for a,b in images.items()):raise ValueError('禁止写入非演示数据区')
  if ':'.join(f'{b:02x}' for b in self.esp.read_mac())!=self.mac:raise ValueError('设备身份变化')
  files=[]
  for a,b in images.items():
   f=io.BytesIO(b);f.name=f'demo-data-{a:x}.bin';files.append((a,f))
  args=types.SimpleNamespace(addr_filename=files,compress=True,no_compress=False,no_stub=False,force=False,encrypt=False,encrypt_files=None,ignore_flash_encryption_efuse_setting=False,erase_all=False,flash_size='8MB',flash_mode='dio',flash_freq='80m')
  try:
   with contextlib.redirect_stdout(io.StringIO()):esptool.write_flash(self.esp,args)
  finally:
   for _,f in files:f.close()
 def boot(self):
  if self.esp:
   with contextlib.redirect_stdout(io.StringIO()):self.esp.hard_reset()
 def close(self):
  if self.esp:
   self.esp._port.close();self.esp=None
 def __enter__(self):return self
 def __exit__(self,*args):self.close()
