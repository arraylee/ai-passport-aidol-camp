"""Release boundary checks shared by packager and explicit opt-in flash tool."""
import hashlib
import json
import struct
from pathlib import Path

IDENTITY = (0x356000, 0x35A000)
SEGMENTS = {'bootloader.bin': 0, 'partition-table.bin': 0x8000, 'app.bin': 0x10000}

def sha(data):
    return hashlib.sha256(data).hexdigest()

def validate_segments(segments):
    if set(segments) != set(SEGMENTS):
        raise ValueError('Expected only bootloader, partition table and app')
    intervals = []
    for name, (offset, data) in segments.items():
        if offset != SEGMENTS[name] or not data:
            raise ValueError('Unexpected address or empty image')
        end = (offset + len(data) + 4095) // 4096 * 4096
        if offset < IDENTITY[1] and end > IDENTITY[0]:
            raise ValueError('Image erases identity sectors')
        if name == 'app.bin' and offset + len(data) > 0x310000:
            raise ValueError('Application exceeds factory partition')
        if name == 'bootloader.bin' and end > 0x8000:
            raise ValueError('Bootloader overlaps partition table')
        if name == 'partition-table.bin' and end > 0x9000:
            raise ValueError('Partition table overlaps original NVS')
        intervals.append((offset, end))
    for (start, end), (next_start, _) in zip(sorted(intervals), sorted(intervals)[1:]):
        if end > next_start:
            raise ValueError('Overlapping erase sectors')
    boot = segments['bootloader.bin'][1]
    app = segments['app.bin'][1]
    if boot[0] != 0xE9 or app[0] != 0xE9:
        raise ValueError('Not ESP images')
    # ESP32-C3 extended header chip id is 5, at byte offset 12.
    if struct.unpack_from('<H', boot, 12)[0] != 5 or struct.unpack_from('<H', app, 12)[0] != 5:
        raise ValueError('Image is not for ESP32-C3')
    if boot[2] != 2 or boot[3] >> 4 != 3:
        raise ValueError('Expected DIO / 8 MB boot image')
    table = segments['partition-table.bin'][1]
    entries = {}
    ranges = []
    partition_flags = {}
    for pos in range(0, len(table)-31, 32):
        magic, kind, sub, offset, size, label, flags = struct.unpack_from('<HBBII16sI', table, pos)
        if magic != 0x50AA:
            break
        name = label.split(b'\0')[0].decode()
        entries[name] = (kind, sub, offset, size)
        partition_flags[name]=flags
        ranges.append((offset, offset+size))
        if offset+size > 0x800000:
            raise ValueError('Partition outside 8 MB')
    required = {'nvs': (1,2,0x9000,0x6000), 'factory': (0,0,0x10000,0x300000),
                'cardid': (1,2,0x356000,0x4000), 'demo_cfg': (1,2,0x360000,0x6000),
                'recovery': (1,0x40,0x700000,0x100000)}
    if any(entries.get(k) != v for k,v in required.items()):
        raise ValueError('Unexpected partition layout')
    if "profile_store" in entries:
        if entries["profile_store"]!=(1,0x41,0x380000,0x30000) or not (partition_flags["cardid"] & 2):
            raise ValueError("Profile storage requires exact layout and hardware readonly identity")
    for name, expected in {'friend_store': (1,0x42,0x3b0000,0x30000), 'gallery_store': (1,0x43,0x400000,0xc0000)}.items():
        if name in entries and entries[name] != expected:
            raise ValueError('Unexpected '+name+' partition layout')
    for (_, end), (next_start, _) in zip(sorted(ranges), sorted(ranges)[1:]):
        if end > next_start:
            raise ValueError('Overlapping partitions')
    return entries

def load_release(directory):
    directory = Path(directory)
    manifest = json.loads((directory/'manifest.json').read_text())
    segments = {}
    for name, address in SEGMENTS.items():
        record = manifest['segments'][name]
        data = (directory/name).read_bytes()
        if record['address'] != address or record['size'] != len(data) or record['sha256'] != sha(data):
            raise ValueError('Segment manifest/hash mismatch: '+name)
        segments[name] = (address, data)
    validate_segments(segments)
    return manifest, segments
