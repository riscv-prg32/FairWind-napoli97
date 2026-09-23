#!/usr/bin/env python3
"""Capture a deterministic full-race FairWind demo with the PRG32 QEMU recorder."""

from pathlib import Path
import importlib.util
import os


PRG32_ROOT = Path("/Users/raffaelemontella/devel/riscv-prg32/PRG32")
PROJECT_ROOT = Path(__file__).resolve().parents[1]
RECORDER = PROJECT_ROOT / "tools" / "prg32_capture_runtime.py"
QEMU_BIN = Path("/Users/raffaelemontella/.espressif/tools/qemu-riscv32/esp_develop_9.0.0_20240606/qemu/bin")
os.environ["PATH"] = f"{QEMU_BIN}:{os.environ['PATH']}"

spec = importlib.util.spec_from_file_location("prg32_preview_recorder", RECORDER)
recorder = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(recorder)

# Navigate title -> single player -> team -> HQ -> briefing -> race, then
# demonstrate sail and helm controls while the simulated clock reaches results.
events = (
    (0.8, "j"), (1.6, "j"), (2.4, "d"), (2.8, "j"),
    (3.5, "s"), (3.8, "s"), (4.1, "s"), (4.4, "s"), (4.7, "s"),
    (5.3, "j"), (6.2, "d"), (7.0, "j"),
    (12.0, "j"), (16.0, "a"), (20.0, "d"), (24.0, "a"),
    (31.0, "d"), (39.0, "a"), (47.0, "d"), (55.0, "j"),
    (63.0, "a"), (72.0, "d"), (82.0, "a"), (92.0, "j"),
    (102.0, "d"), (112.0, "a"),
)

recorder.ROOT = PRG32_ROOT
recorder.CARTRIDGES["fairwind"] = (
    str(PROJECT_ROOT / "dist" / "store" / "FairWind-napoli97-qemu.prg32"),
    str(PROJECT_ROOT / "release-artifacts" / "FairWind-napoli97-qemu-long.mp4"),
    events,
)
recorder.capture("fairwind", float(os.environ.get("FAIRWIND_DEMO_DURATION", "135")),
                 30, 7.0, "/opt/homebrew/bin/ffmpeg", True)
