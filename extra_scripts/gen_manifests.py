#!/usr/bin/env python3
"""
Generates per-device esp-web-tools manifest JSON files for GitHub Releases.
Device list and chip config is read from devices.json in the repo root.
Usage: python gen_manifests.py <tag> <output_dir>
"""
import json
import os
import sys

tag        = sys.argv[1]
output_dir = sys.argv[2]
base_url   = f"https://github.com/DN9KGB/rMesh/releases/download/{tag}"

with open('devices.json', encoding='utf-8') as f:
    devices = json.load(f)

os.makedirs(output_dir, exist_ok=True)

for dev in devices:
    device      = dev['name']
    chip        = dev['chip']

    # esp-web-tools manifests only apply to ESP32 chips
    if not chip.startswith('ESP32'):
        print(f'  SKIP {device} (not ESP32)')
        continue

    lfs_offset  = dev['lfs_offset']
    is_s3       = chip == 'ESP32-S3'
    boot_offset = 0x0000 if is_s3 else 0x1000

    manifest = {
        'name':    f'rMesh \u2013 {device}',
        'version': tag,
        'builds': [{
            'chipFamily': chip,
            'parts': [
                {'path': f'{base_url}/{device}_bootloader.bin', 'offset': boot_offset},
                {'path': f'{base_url}/{device}_partitions.bin',  'offset': 0x8000},
                {'path': f'{base_url}/{device}_firmware.bin',    'offset': 0x10000},
                {'path': f'{base_url}/{device}_littlefs.bin',    'offset': lfs_offset},
            ]
        }]
    }

    out_path = os.path.join(output_dir, f'{device}_manifest.json')
    with open(out_path, 'w') as f:
        json.dump(manifest, f, indent=2)
    print(f'  {out_path}')
