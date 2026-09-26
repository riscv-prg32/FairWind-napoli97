#!/usr/bin/env python3
"""Measure FairWind's per-frame cost inside the real PRG32 firmware.

Builds tools/profile/profile_cart.c (the game plus an autopilot and timers)
as a portable QEMU cartridge and runs it in the ESP32 QEMU machine with
``-icount shift=0``: every guest instruction advances virtual time by 1 ns,
so the reported microseconds are thousands of RV32 instructions executed by
the cartridge *and* the firmware calls it makes. An SDL window opens while it
runs. usage: PRG32_ROOT=... qemu_profile.py [SECONDS]
"""
import importlib.util, os, re, shutil, subprocess, sys, tempfile, threading, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRG32 = Path(os.environ.get('PRG32_ROOT', ROOT.parent / 'PRG32')).resolve()
seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 240
source = ROOT / 'tools/profile' / ('profile_null.c' if os.environ.get('NULL_GFX') else 'profile_cart.c')
# shift=4: one instruction = 16 ns of virtual time (shift=0 does not boot this machine).
ICOUNT = [] if os.environ.get('ICOUNT') == 'off' else ['-icount', 'shift=4']
NS_PER_INSN = 16
spec = importlib.util.spec_from_file_location('rec', ROOT / 'tools/prg32_capture_runtime.py')
rec = importlib.util.module_from_spec(spec); spec.loader.exec_module(rec)
adapter = ROOT / 'tools/prg32_cli_64.py'
# Default ESP-IDF tool locations, so the script runs from a plain shell.
for tools in (Path.home() / '.espressif/tools/riscv32-esp-elf', Path.home() / '.espressif/tools/qemu-riscv32'):
    for bin_dir in sorted(tools.glob('*/*/bin')) + sorted(tools.glob('*/*/*/bin')):
        os.environ['PATH'] = f"{bin_dir}:{os.environ['PATH']}"
with tempfile.TemporaryDirectory(prefix='fairwind-profile-') as tmp:
    tmp = Path(tmp)
    cart, flash, efuse = tmp / 'profile.prg32', tmp / 'flash.bin', tmp / 'efuse.bin'
    audio = tmp / 'audio.block'   # the game's score, packed as build.sh does
    subprocess.run(['python3', 'tools/prg32audio_pack.py', str(ROOT / 'audio.json'), '--out', str(audio)], cwd=PRG32, check=True, stdout=subprocess.DEVNULL)
    subprocess.run(['python3', str(adapter), 'cartridge', 'build', str(source), '--portable', '--multiplayer',
                    '--entry-prefix', 'fairwind', '--name', 'FairWind-profile', '--audio-block', str(audio), '--out', str(cart)],
                   cwd=PRG32, check=True, stdout=subprocess.DEVNULL)
    shutil.copy2(PRG32 / 'build-qemu/qemu_flash.bin', flash); shutil.copy2(PRG32 / 'build-qemu/qemu_efuse.bin', efuse)
    subprocess.run(['python3', str(adapter), 'qemu', 'upload', str(cart), '--flash', str(flash)], cwd=PRG32, check=True, stdout=subprocess.DEVNULL)
    cmd = ['qemu-system-riscv32', '-M', 'esp32c3', '-m', '4M', *ICOUNT,
           '-drive', f'file={flash},if=mtd,format=raw', '-drive', f'file={efuse},if=none,format=raw,id=efuse',
           '-global', 'driver=nvram.esp32c3.efuse,property=drive,value=efuse',
           '-global', 'driver=timer.esp32c3.timg,property=wdt_disable,value=true',
           '-nic', 'user,model=open_eth', '-display', 'sdl', '-monitor', 'none',
           '-serial', f'tcp::{rec.CONSOLE_PORT},server=on,wait=off,nodelay=on',
           '-serial', f'tcp::{rec.AUDIO_PORT},server=on,wait=on,nodelay=on']
    samples, transcript = bytearray(), bytearray()
    recording, complete, stopping = threading.Event(), threading.Event(), threading.Event()
    proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    threading.Thread(target=rec.audio_worker, args=(proc, recording, complete, samples, seconds * 4), daemon=True).start()
    console = rec.connect_tcp(rec.CONSOLE_PORT, proc)
    threading.Thread(target=rec.console_worker, args=(console, transcript, stopping), daemon=True).start()
    recording.set()
    time.sleep(seconds)
    stopping.set(); proc.terminate(); proc.wait(timeout=10)
if os.environ.get('AUDIO_OUT'):
    import wave
    with wave.open(os.environ['AUDIO_OUT'], 'wb') as w:
        w.setparams((1, 2, rec.RATE, len(samples) // 2, 'NONE', 'not compressed')); w.writeframes(samples)
lines = [l for l in transcript.decode('utf-8', 'replace').splitlines() if l.startswith('PROF ')]
rows = [dict((k, int(v)) for k, v in re.findall(r'(\w+)=(\d+)', l)) for l in lines]
rows = [r for r in rows if {'upd_avg', 'upd_max', 'drw_avg', 'drw_max', 'top'} <= r.keys()]   # skip lines cut by other output
for l in lines: print(l)
for l in transcript.decode('utf-8', 'replace').splitlines():
    if l.startswith(('CAL ', 'RESULT ')): print(l)
if rows:
    ki = lambda us: us * 1000 / NS_PER_INSN / 1000   # virtual us -> thousand instructions
    for key in ('upd_avg', 'drw_avg'):
        vals = [r[key] for r in rows]
        print(f'{key}: mean {ki(sum(vals) / len(vals)):.0f}  max {ki(max(vals)):.0f}  thousand instructions per frame')
    print(f"peak single frame: update {ki(max(r['upd_max'] for r in rows)):.0f}, draw {ki(max(r['drw_max'] for r in rows)):.0f} thousand instructions")
    chase = [r for r in rows if r['top'] == 0]; top = [r for r in rows if r['top'] == 64]
    for name, group in (('chase view', chase), ('top view', top)):
        if group: print(f"{name}: {len(group)} windows, draw mean {ki(sum(r['drw_avg'] for r in group) / len(group)):.0f} thousand instructions")
    if any('rects' in r for r in rows):
        for key in ('rects', 'rows', 'px', 'pixels', 'chars', 'rgb'):
            vals = [r[key] for r in rows if key in r]
            print(f'{key}/frame: mean {sum(vals) / len(vals):.0f}  max {max(vals)}')
else:
    print('no PROF lines: did the cartridge reach a race? console tail:')
    print('\n'.join(transcript.decode('utf-8', 'replace').splitlines()[-25:]))
