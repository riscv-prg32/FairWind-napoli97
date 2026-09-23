#!/usr/bin/env python3
"""Write the Cartridge Store bundle manifest from the cartridge metadata.

The Store requires manifest.abi == prg32-metadata-1.0 and rebuilds every
cartridge from the manifest itself: metadata fields come from the manifest,
the screenshot from assets.splash, and the colophon from an inline object.
Deriving it here keeps the bundle in step with metadata/*.json.
"""
import json, sys
from pathlib import Path

r = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]) if len(sys.argv) > 1 else r / 'dist/store/manifest.json'
meta = json.loads((r / 'metadata/metadata.json').read_text())
colophon = json.loads((r / 'metadata/colophon.json').read_text())
assert meta['abi'] == 'prg32-metadata-1.0'
assert colophon['version'] == meta['version'] and colophon['title'] == meta['title']
manifest = dict(meta)
manifest['architectures'] = [{'id': a, 'file': f"{meta['name']}-{a}.prg32"} for a in meta['runtime']['architectures']]
manifest['assets'] = {'icon': 'icon.png', 'splash': 'screenshot.png'}
manifest['colophon'] = colophon
out.write_text(json.dumps(manifest, ensure_ascii=False, separators=(',', ':')) + '\n')
print(f'wrote {out}')
