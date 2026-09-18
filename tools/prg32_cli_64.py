#!/usr/bin/env python3
"""Run the PRG32 CLI with the announced 64 KiB portable RAM fallback.

PRG32 commit 596bcf9 raises the firmware profile to 64 KiB, but its portable
builder fallback remains 32 KiB.  Keep this tiny adapter until that fallback is
updated upstream; it changes no firmware or package-format behavior.
"""

import importlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path.cwd()))

from prg32.utilities import env_variables


RAM_SIZE = 64 * 1024
env_variables.FALLBACK_CART_RAM_SIZE = RAM_SIZE
builder = importlib.import_module("prg32.cartridge.build_cartridge")
builder.FALLBACK_CART_RAM_SIZE = RAM_SIZE
runtime = importlib.import_module("prg32.utilities.runtime_handler")
runtime.FALLBACK_CART_RAM_SIZE = RAM_SIZE

from prg32 import prg32  # noqa: E402

prg32.main(sys.argv[1:])
