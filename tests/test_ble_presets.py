"""Every delivered portrait must decode on-device to its advertised RGB565 CRC."""
from pathlib import Path
import json,subprocess,sys,re
root=Path(__file__).resolve().parents[1];presets=root/'tools/device_console/presets'
for role in json.loads((presets/'manifest.json').read_text())['roles']:
 p=subprocess.run([sys.argv[1],str(presets/role['avatar'])],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
 assert int(re.search(r'PRESET_IMAGE_CRC=(\d+)',p.stdout)[1])==role['image_crc'],role['id']
print('PASS six delivered portraits through production PNG/profile decoder and exact pixel CRC')
