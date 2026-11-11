#!/usr/bin/env python3
"""Store icon and title art from real host-rendered frames (tools/host_capture.py
--store runs this after capturing): the title screen as title.png, and a
256x256 icon cropped from it around the yacht under the logo."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw
sys.path.insert(0, str(Path(__file__).resolve().parent))
from pngutil import save_compact

r = Path(__file__).resolve().parents[1]
frames = Path(sys.argv[1]) if len(sys.argv) > 1 else r / 'build/host-frames'
o = r / 'assets/generated'; o.mkdir(parents=True, exist_ok=True)
title = Image.open(frames / '01-title.png').convert('RGB')
title.save(o / 'title.png')
# The logo spans x 72..248; the yacht sails below it. Crop a square that
# keeps the whole logo, scale it up crisply and frame it in gold.
crop = title.crop((68, 6, 252, 190)).resize((240, 240), Image.Resampling.LANCZOS)
icon = Image.new('RGB', (256, 256), (255, 204, 64))
d = ImageDraw.Draw(icon); d.rectangle((3, 3, 252, 252), fill=(14, 26, 62))
icon.paste(crop, (8, 8))
# The icon rides in every package: a 64-colour palette keeps it near 10 KB.
icon = icon.quantize(64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert('RGB')
save_compact(icon, o / 'icon.png')
print('rendered title.png and icon.png')
