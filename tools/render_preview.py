#!/usr/bin/env python3
"""Render the Cartridge Store icon from the FairWind logo.

The title and in-race store images are real game frames captured by
tools/host_capture.py --store."""
from pathlib import Path
from PIL import Image
from pngutil import save_compact

ICON = 128
r = Path(__file__).resolve().parents[1]
o = r / 'assets/generated'
o.mkdir(parents=True, exist_ok=True)
# The store icon counts toward the 64 KiB package, so the logo is reduced to
# an exact 48-colour palette.
icon = Image.open(r / 'assets/source/fairwind-logo.png').convert('RGB').resize((ICON, ICON), Image.Resampling.LANCZOS)
icon = icon.quantize(48, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert('RGB')
save_compact(icon, o / 'icon.png')
print('rendered icon.png')
