#!/usr/bin/env python3
"""Render real game frames on the host: compile tools/host_render.c against
src/game.c with the PRG32 8x8 font, run an autopiloted race, save PNGs.

usage: host_capture.py OUTDIR [COURSE] [VENUE]   (needs PRG32_ROOT)
       host_capture.py --store   refresh the store screenshot and release frames
       host_capture.py --video OUT.mp4 [AUDIO.wav]   highlight montage at 30 fps
"""
import os, re, subprocess, sys, tempfile
from pathlib import Path
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parent))
from pngutil import save_compact

ROOT = Path(__file__).resolve().parents[1]
prg32 = Path(os.environ.get('PRG32_ROOT', ROOT.parent / 'PRG32'))
store = sys.argv[1:2] == ['--store']
video = sys.argv[1:2] == ['--video']
if video:
    video_out, video_audio = Path(sys.argv[2]).resolve(), (Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else None)
    os.environ['VIDEO'] = '1'; sys.argv[1:] = [str(ROOT / 'build' / 'host-video'), '0', '1']
if store: sys.argv[1:] = [str(ROOT / 'build' / 'host-frames'), '0', '1']
out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / 'build' / 'host-frames'); out.mkdir(parents=True, exist_ok=True)
src = (prg32 / 'components/prg32/prg32_display_qemu_rgb.c').read_text()
font = re.search(r'static const uint8_t g_font8\[96\]\[8\][^=]*=\s*\{.*?\n\};', src, re.S).group(0)
with tempfile.TemporaryDirectory() as tmp:
    Path(tmp, 'font8.h').write_text(font.replace('PRG32_FLASH_RODATA', '') + '\n')
    exe = Path(tmp, 'host_render')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-D_FORTIFY_SOURCE=0', '-Wall', '-Wextra', '-I', tmp, '-I', str(ROOT / 'tests/stub'),
                    '-I', str(ROOT / 'src'), '-I', str(ROOT / 'tools'), str(ROOT / 'tools/host_render.c'), '-o', str(exe)], check=True)
    for old in out.glob('*.p[np][gm]'): old.unlink()
    res = subprocess.run([str(exe), str(out)] + sys.argv[2:4], check=True, capture_output=True, text=True)
    print(res.stdout if os.environ.get("TRACE") else res.stdout.splitlines()[-1])
if video:
    cmd = ['ffmpeg', '-y', '-loglevel', 'error', '-framerate', '30', '-i', str(out / 'v%05d.ppm')]
    if video_audio: cmd += ['-i', str(video_audio), '-shortest', '-c:a', 'aac', '-b:a', '96k']
    cmd += ['-vf', 'scale=640:400:flags=neighbor', '-c:v', 'libx264', '-crf', '20', '-pix_fmt', 'yuv420p', '-movflags', '+faststart', str(video_out)]
    subprocess.run(cmd, check=True)
    for f in out.glob('*.ppm'): f.unlink()
    print(f'video: {video_out}'); sys.exit(0)
for ppm in sorted(out.glob('*.ppm')):
    Image.open(ppm).save(ppm.with_suffix('.png')); ppm.unlink()
print(f'frames in {out}')
if store:
    gen, shots = ROOT / 'assets/generated', ROOT / 'release-artifacts/store-screenshots'
    frame = lambda name: Image.open(out / f'{name}.png').convert('RGB')
    save_compact(frame('store-run'), gen / 'screenshot.png')
    save_compact(frame('01-title'), gen / 'title.png')
    save_compact(frame('06-entering-box'), gen / 'start_sequence.png')
    save_compact(frame('store-top'), gen / 'course_paths.png')
    for src, dst in [('01-title', '01-title'), ('02-mode', '02-mode'), ('03-team-hq', '03-team-hq'), ('04-race-briefing', '04-race-briefing'),
                     ('06-entering-box', '05-start-sequence'), ('store-run', '06-racing'), ('store-beat', '07-upwind'), ('store-top', '08-top-view')]:
        save_compact(frame(src), shots / f'{dst}.png')
    sheet = Image.new('RGB', (640, 400))
    for i, name in enumerate(['store-beat', 'store-run', '06-entering-box', 'store-top']):
        sheet.paste(frame(name), ((i % 2) * 320, (i // 2) * 200))
    save_compact(sheet, ROOT / 'release-artifacts/FairWind-napoli97-gameplay.png')
    print(f'store screenshot {(gen / "screenshot.png").stat().st_size} bytes')
