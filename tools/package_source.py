#!/usr/bin/env python3
from pathlib import Path
import hashlib, zipfile
r=Path(__file__).resolve().parents[1]; import json; out=r/'dist'/f"FairWind-napoli97-{json.loads((r/'metadata/metadata.json').read_text())['version']}-github-ready.zip"; out.parent.mkdir(exist_ok=True)
skip={'.git','build','dist'}
files=[p for p in r.rglob('*') if p.is_file() and not any(x in skip for x in p.parts) and p!=out and '__pycache__' not in p.parts and not ('release-artifacts' in p.parts and not p.name.startswith('FairWind-napoli97'))]
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for p in sorted(files): z.write(p,Path(r.name)/p.relative_to(r))
h=hashlib.sha256(out.read_bytes()).hexdigest();(r/'dist'/'SOURCE_SHA256SUMS').write_text(f'{h}  {out.name}\n')
print(out)
