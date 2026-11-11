#!/usr/bin/env python3
"""Generate src/assets.h: the indexed palette, the five graded Bay of Naples
venues, the far-coast horizon profile, the logotype and the cup.

The PRG32 ESP32-C6 framebuffer holds one 8-bit palette index per pixel.
FairWind draws every fill with prg32_gfx_rect_indexed/pixel_indexed, so a fill
colour can sit in any of the 256 entries. Text is the exception: the firmware
font call takes RGB565 colours and quantises them to a cell of the 6x6x6 system
cube (prg32_gfx_index_for_rgb565_unlocked). Each text colour is therefore
written into its own cube cell, so the QEMU (RGB565) and ESP32-C6 (indexed)
displays show the same shade; no two text colours may share a cell.

Each venue reloads a block of palette entries: its sky, sea, haze, sun, glint
and panorama colours, graded to the light of that race day (morning at Santa
Lucia to sunset for the Naples final). Four glint entries are rotated every
few frames by the game, a classic indexed-colour shimmer on the water.
"""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "src" / "assets.h"
GEN = ROOT / "assets" / "generated"
SRC = ROOT / "assets" / "source"

NAMED = (0x0000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F)


def rgb565(c):
    r, g, b = c
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def rgb888(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return ((r * 527 + 23) >> 6, (g * 259 + 33) >> 6, (b * 527 + 23) >> 6)


def cube_index(v):
    """Exact port of prg32_gfx_index_for_rgb565_unlocked() (ESP32-C6)."""
    if v in NAMED:
        return NAMED.index(v)
    r = ((v >> 11) & 31) * 5 // 31
    g = ((v >> 5) & 63) * 5 // 63
    b = (v & 31) * 5 // 31
    return 16 + r * 36 + g * 6 + b


def mix(a, b, t):
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


def ramp(stops, n):
    """n colours interpolated through (position, colour) stops."""
    out = []
    for i in range(n):
        p = i / (n - 1) if n > 1 else 0
        for (p0, c0), (p1, c1) in zip(stops, stops[1:]):
            if p0 <= p <= p1:
                out.append(mix(c0, c1, (p - p0) / (p1 - p0) if p1 > p0 else 0))
                break
    return out


# ------------------------------------------------------------- palette --
# Text colours: foreground and background of every prg32_gfx_text8 call.
TEXT = {
    "BLACK": (0, 0, 0),
    "WHITE": (255, 255, 255),
    "GOLD": (255, 204, 64),
    "CYAN": (100, 220, 246),
    "GREEN": (110, 228, 120),
    "RED": (246, 78, 70),
    "GREY": (150, 162, 182),
    "NAVY": (14, 26, 62),
    "HDR": (6, 12, 34),
    "SEL": (40, 70, 140),
    "INK": (60, 94, 124),
    # Team colours double as text (HUD kite name, lobby, results).
    "TEAM0": (40, 196, 220),
    "TEAM1": (232, 40, 48),
    "TEAM2": (142, 92, 232),
    "TEAM3": (248, 176, 20),
}

# Fill-only colours shared by every screen.
FIXED = {
    "PANEL_HI": (60, 92, 160), "PANEL_LO": (4, 8, 22), "PANEL": (20, 36, 82),
    "HULL_L": (196, 26, 32), "HULL_D": (118, 10, 20), "HULL_K": (60, 8, 12),
    "DECK_L": (232, 220, 186), "DECK_D": (170, 150, 116), "COCKPIT": (96, 82, 66),
    "CLOTH_L": (246, 244, 232), "CLOTH_D": (190, 194, 196), "CLOTH_LUFF": (150, 160, 184),
    "BATTEN": (160, 160, 150), "SPAR": (34, 34, 40),
    "TEAMD0": (16, 112, 140), "TEAMD1": (130, 22, 34), "TEAMD2": (80, 44, 150), "TEAMD3": (176, 104, 6),
    "BUOY_L": (255, 132, 20), "BUOY_D": (190, 70, 8),
    "FOAM": (236, 246, 250), "WAKE": (170, 214, 232),
    "TUFA_L": (214, 160, 96), "TUFA_D": (150, 98, 58), "CITY_L": (222, 204, 160), "CITY_D": (158, 138, 108),
    "GREEN_L": (70, 132, 62), "GREEN_D": (40, 86, 44), "SAND": (236, 214, 160),
    "GROUND": (98, 128, 70), "GROUND_D": (72, 96, 54), "ROOF": (178, 82, 50),
    "CB_GREY": (176, 184, 192), "CB_DARK": (80, 88, 98), "FLAG_BLUE": (24, 60, 196),
    "MAP_SEA": (22, 84, 150), "MAP_DEEP": (12, 50, 104), "LAYLINE": (150, 196, 246),
    "TRIM_BG": (20, 30, 60), "TRIM_OK": (30, 160, 80),
    "GOLD_D": (150, 92, 10), "GOLD_M": (218, 150, 30), "GOLD_L": (255, 218, 96), "GOLD_H": (255, 248, 210),
    "LOGO_INK": (8, 14, 40), "LOGO_SH": (2, 30, 70),
}
MENU = ramp([(0, (8, 22, 62)), (0.55, (8, 70, 128)), (1, (16, 120, 160))], 12)
for i, c in enumerate(MENU):
    FIXED[f"MENU{i}"] = c

# ---------------------------------------------------------------- venues --
VENUE_NAMES = ["SANTA LUCIA", "VESUVIUS", "CAPRI", "SORRENTO", "NAPLES"]
# Light of each race day: (sky stops, sea stops, haze, grade colour and amount,
# sun colour, sun world bearing in degrees from upwind (clockwise), elevation).
VENUE_LIGHT = [
    dict(sky=[(0, (70, 128, 214)), (0.6, (150, 196, 236)), (1, (226, 232, 236))],
         sea=[(0, (120, 168, 200)), (0.25, (40, 108, 166)), (1, (6, 46, 104))],
         haze=(170, 190, 214), grade=((210, 222, 244), 0.18), sun=(255, 246, 214), sun_at=(245, 46)),
    dict(sky=[(0, (52, 116, 210)), (0.6, (124, 182, 238)), (1, (210, 228, 244))],
         sea=[(0, (110, 170, 210)), (0.25, (30, 110, 176)), (1, (4, 44, 108))],
         haze=(150, 176, 210), grade=((255, 255, 255), 0.0), sun=(255, 252, 230), sun_at=(290, 62)),
    dict(sky=[(0, (36, 100, 206)), (0.55, (104, 172, 240)), (1, (196, 228, 250))],
         sea=[(0, (96, 176, 216)), (0.22, (16, 118, 186)), (1, (2, 48, 116))],
         haze=(140, 172, 212), grade=((120, 200, 255), 0.05), sun=(255, 255, 240), sun_at=(330, 66)),
    dict(sky=[(0, (60, 104, 190)), (0.55, (150, 172, 214)), (1, (250, 214, 168))],
         sea=[(0, (176, 172, 176)), (0.25, (44, 100, 150)), (1, (10, 42, 92))],
         haze=(186, 170, 176), grade=((255, 196, 130), 0.16), sun=(255, 236, 170), sun_at=(10, 34)),
    dict(sky=[(0, (40, 46, 120)), (0.45, (190, 96, 120)), (0.8, (250, 150, 80)), (1, (255, 214, 120))],
         sea=[(0, (236, 160, 96)), (0.2, (120, 80, 110)), (1, (20, 26, 70))],
         haze=(150, 96, 120), grade=((255, 130, 70), 0.22), sun=(255, 240, 150), sun_at=(40, 9)),
]
SKY_N, SEA_N = 14, 12


def grade(c, v):
    g, t = VENUE_LIGHT[v]["grade"]
    out = mix(c, g, t)
    if v == 4:  # sunset: darker, warmer shadows
        out = tuple(int(x * k) for x, k in zip(out, (0.92, 0.78, 0.80)))
    return out


def load_panorama(scene):
    """Authored late-1990s panorama: sky made transparent, cropped at the
    waterline. Returns (rows of palette slots or None, colour list)."""
    im = Image.open(SRC / f"panorama-{scene}.png").convert("RGB")
    w, h = im.size
    px = im.load()
    sky = px[0, 0]
    clouds = {(255, 247, 218)}
    sea_cols = {(4, 96, 174), (54, 174, 217), (2, 38, 99)}
    # Waterline: first row from the bottom half that is mostly open sea.
    water = next(y for y in range(24, h) if sum(px[x, y] in sea_cols for x in range(w)) > 250)
    clear = [[False] * w for _ in range(h)]
    stack = [(x, 0) for x in range(w)]
    while stack:
        x, y = stack.pop()
        if not (0 <= x < w and 0 <= y < water) or clear[y][x]:
            continue
        c = px[x, y]
        if c == sky or (c in clouds and y < 22) or (scene == 3 and y < 24 and c in ((54, 174, 217), (111, 181, 235), (131, 171, 210), (255, 247, 218))):
            clear[y][x] = True
            stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    # Keep only the land: pixels connected to the waterline. Floating cloud
    # fragments and haze speckles go, the game draws its own clouds.
    keep = [[False] * w for _ in range(h)]
    stack = [(x, water - 1) for x in range(w) if not clear[water - 1][x]]
    while stack:
        x, y = stack.pop()
        if not (0 <= x < w and 0 <= y < water) or keep[y][x] or clear[y][x]:
            continue
        keep[y][x] = True
        stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    for y in range(water):
        for x in range(w):
            clear[y][x] = not keep[y][x]
    rows = []
    top = next(y for y in range(water) if not all(clear[y]))
    for y in range(top, water):
        rows.append([None if clear[y][x] else px[x, y] for x in range(w)])
    return rows


panoramas = [load_panorama(i) for i in range(5)]
PANO_COLOURS = sorted({c for p in panoramas for row in p for c in row if c is not None})
assert len(PANO_COLOURS) <= 16, PANO_COLOURS


def venue_colours(v):
    L = VENUE_LIGHT[v]
    sky = ramp(L["sky"], SKY_N)
    sea = ramp(L["sea"], SEA_N)
    haze = L["haze"]
    far = [mix(haze, sky[-1], 0.35), haze, mix(haze, (40, 60, 80), 0.25)]
    cloud = [mix((255, 255, 255), sky[-1], 0.15), mix((222, 226, 236), sky[6], 0.25), mix((176, 186, 206), sky[3], 0.35)]
    if v == 4:
        cloud = [(255, 200, 150), (230, 140, 120), (130, 80, 110)]
    sun = L["sun"]
    halo = mix(sun, sky[-3], 0.5)
    glint = [sea[3], mix(sea[2], (255, 255, 255), 0.45), mix(sun, (255, 255, 255), 0.4), (255, 255, 255)]
    ripple = mix(sea[4], (255, 255, 255), 0.35)
    top_sea = [sea[6], sea[8]]
    pano = []
    for c in PANO_COLOURS:
        if c in ((4, 96, 174), (2, 38, 99)):
            pano.append(sea[1])
        elif c == (54, 174, 217):
            pano.append(mix(sea[0], (255, 255, 255), 0.2))
        else:
            pano.append(mix(grade(c, v), haze, 0.12))
    pano += [(0, 0, 0)] * (16 - len(pano))
    out = {}
    for i, c in enumerate(sky):
        out[f"SKY{i}"] = c
    for i, c in enumerate(sea):
        out[f"SEA{i}"] = c
    for i, c in enumerate(far):
        out[f"FAR{i}"] = c
    for i, c in enumerate(cloud):
        out[f"CLOUD{i}"] = c
    out["SUN"], out["HALO"] = sun, halo
    for i, c in enumerate(glint):
        out[f"GL{i}"] = c
    out["RIPPLE"] = ripple
    out["TOPSEA0"], out["TOPSEA1"] = top_sea
    for i, c in enumerate(pano):
        out[f"PANO{i}"] = c
    return out


VENUES = [venue_colours(v) for v in range(5)]
VENUE_KEYS = list(VENUES[0])

# -------------------------------------------------------------- allocation --
index = {}
used = set()
for name, c in TEXT.items():
    v = rgb565(c)
    if name == "BLACK":
        v = 0x0000
    if name == "WHITE":
        v = 0xFFFF
    cell = cube_index(v)
    assert cell not in used, f"text colour {name} shares cube cell {cell}"
    used.add(cell)
    index[name] = cell
free = [i for i in list(range(8, 16)) + list(range(232, 256)) + list(range(16, 232)) if i not in used]
for name in list(FIXED) + VENUE_KEYS:
    index[name] = free.pop(0)
SLOTS_LEFT = len(free)

# ------------------------------------------------------------ artwork --
def rle_rows(rows, w):
    """Rows of slot names (or None) -> (count, index) byte pairs; index 255 is
    transparent. Rows never span: each row restarts at x = 0."""
    data = []
    for row in rows:
        x = 0
        while x < w:
            v = row[x]
            n = 1
            while x + n < w and row[x + n] == v and n < 255:
                n += 1
            data += [n, 255 if v is None else index[v]]
            x += n
    return data


pano_rle, pano_off, pano_h = [], [0], []
for p in panoramas:
    rows = [[None if c is None else f"PANO{PANO_COLOURS.index(c)}" for c in row] for row in p]
    pano_rle += rle_rows(rows, 320)
    pano_off.append(len(pano_rle))
    pano_h.append(len(rows))

# Far coast round the Gulf, by world bearing (0 = upwind SW, clockwise).
FAR_FEATURES = [
    # (centre deg, half width deg, height px, shade, shape)
    (30, 9, 9, 0, "peak"),     # Ischia, Monte Epomeo
    (45, 3, 3, 0, "flat"),     # Procida
    (60, 4, 4, 1, "flat"),     # Capo Miseno
    (78, 12, 6, 1, "hills"),   # Campi Flegrei
    (104, 14, 8, 2, "hills"),  # Posillipo
    (262, 16, 9, 1, "ridge"),  # Monti Lattari
    (292, 16, 13, 1, "peak"),  # Monte Faito
    (316, 8, 6, 1, "ridge"),   # Punta Campanella
    (334, 6, 7, 0, "twin"),    # Capri
]
far = [0] * 256
for b in range(256):
    deg = b * 360 / 256
    best = (0, 0)
    for c, hw, hgt, shade, shape in FAR_FEATURES:
        d = (deg - c + 180) % 360 - 180
        if abs(d) > hw:
            continue
        t = abs(d) / hw
        if shape == "peak":
            v = hgt * (1 - t) ** 1.3
        elif shape == "flat":
            v = hgt * (1 - t ** 4)
        elif shape == "twin":
            v = hgt * (1 - t ** 2) * (0.75 + 0.25 * math.cos(d / hw * 3 * math.pi))
        elif shape == "ridge":
            v = hgt * (1 - t ** 3) * (0.8 + 0.2 * math.sin(deg * 0.7))
        else:
            v = hgt * (1 - t ** 2) * (0.7 + 0.3 * math.sin(deg * 0.9))
        if v > best[0]:
            best = (v, shade)
    h = int(round(best[0]))
    far[b] = (best[1] << 5) | min(h, 31) if h else 0

# Logotype: chunky pixel letters, gold ramp with ink outline and shadow.
FONT = {
    "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "I": ["###", ".#.", ".#.", ".#.", ".#.", ".#.", "###"],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "W": ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"],
    "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    "D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
}


def logotype(text, scale):
    gw = sum(len(FONT[ch][0]) + 1 for ch in text) - 1
    w, h = gw * scale + 4, 7 * scale + 4
    mask = [[0] * w for _ in range(h)]
    x0 = 1
    for ch in text:
        g = FONT[ch]
        for gy, line in enumerate(g):
            for gx, bit in enumerate(line):
                if bit == "#":
                    for yy in range(scale):
                        for xx in range(scale):
                            mask[1 + gy * scale + yy][x0 + gx * scale + xx] = 1
        x0 += (len(g[0]) + 1) * scale
    grad = ["GOLD_H", "GOLD_L", "GOLD_L", "GOLD_M", "GOLD_M", "GOLD_D"]
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            if mask[y][x]:
                fy = (y - 1) * len(grad) // (7 * scale)
                row.append(grad[max(0, min(len(grad) - 1, fy))])
            elif any(mask[y + dy][x + dx] for dy in (-1, 0, 1) for dx in (-1, 0, 1)
                     if 0 <= y + dy < h and 0 <= x + dx < w):
                row.append("LOGO_INK")
            elif y >= 2 and x >= 2 and mask[y - 2][x - 2]:
                row.append("LOGO_SH")
            else:
                row.append(None)
        rows.append(row)
    return rows, w, h


logo_rows, LOGO_W, LOGO_H = logotype("FAIRWIND", 4)
logo_rle = rle_rows(logo_rows, LOGO_W)


def cup_rows():
    im = Image.new("L", (32, 40), 0)
    d = ImageDraw.Draw(im)
    d.polygon([(8, 4), (24, 4), (21, 21), (17, 26), (17, 32), (25, 34), (25, 38), (7, 38), (7, 34), (15, 32), (15, 26), (11, 21)], fill=255)
    d.arc((1, 6, 13, 24), 80, 280, fill=255, width=3)
    d.arc((19, 6, 31, 24), 260, 100, fill=255, width=3)
    px = im.load()
    rows = []
    for y in range(40):
        row = []
        for x in range(32):
            if not px[x, y]:
                row.append(None)
                continue
            if y in (4, 5) and 9 <= x <= 23:
                row.append("GOLD_H")
            elif x < 12:
                row.append("GOLD_L" if (x + y) % 7 else "GOLD_H")
            elif x < 18:
                row.append("GOLD_M" if x != 13 else "GOLD_L")
            else:
                row.append("GOLD_D")
        rows.append(row)
    return rows


cup_rle = rle_rows(cup_rows(), 32)

# ------------------------------------------------------------- emit C --
def arr(ctype, name, data, per=16):
    lines = []
    for i in range(0, len(data), per):
        lines.append("  " + ",".join(str(x) for x in data[i:i + per]) + ",")
    return f"static const {ctype} {name}[{len(data)}]={{\n" + "\n".join(lines) + "\n};\n"


fixed_names = list(TEXT) + list(FIXED)
lines = [
    "/* Generated by tools/generate_assets.py: do not edit. */",
    "#ifndef FAIRWIND_ASSETS_H", "#define FAIRWIND_ASSETS_H", "#include <stdint.h>", "",
    "/* Palette indices. Text colours sit in their own 6x6x6 cube cell; T_* is",
    "   the RGB565 value to pass to prg32_gfx_text8 for the same colour. */",
]
for name in fixed_names + VENUE_KEYS:
    lines.append(f"#define C_{name} {index[name]}")
for name, c in TEXT.items():
    v = 0 if name == "BLACK" else 0xFFFF if name == "WHITE" else rgb565(c)
    lines.append(f"#define T_{name} 0x{v:04x}")
for i, name in enumerate(VENUE_KEYS):
    lines.append(f"#define VK_{name} {i}")
lines.append(f"#define FIXED_COLOURS {len(fixed_names)}")
lines.append(f"#define VENUE_COLOURS {len(VENUE_KEYS)}")
lines.append(f"#define SKY_N {SKY_N}")
lines.append(f"#define SEA_N {SEA_N}")
lines.append(f"#define LOGO_W {LOGO_W}")
lines.append(f"#define LOGO_H {LOGO_H}")
lines.append("")
fixed_rgb = []
for n in fixed_names:
    c = TEXT.get(n, FIXED.get(n))
    fixed_rgb.append(0 if n == "BLACK" else 0xFFFF if n == "WHITE" else rgb565(c))
body = "\n".join(lines) + "\n"
body += arr("uint8_t", "fixed_slot", [index[n] for n in fixed_names])
body += arr("uint16_t", "fixed_rgb", fixed_rgb, 12)
body += arr("uint8_t", "venue_slot", [index[n] for n in VENUE_KEYS])
body += "static const uint16_t venue_rgb[5][VENUE_COLOURS]={\n"
for v in VENUES:
    body += "  {" + ",".join(f"0x{rgb565(v[k]):04x}" for k in VENUE_KEYS) + "},\n"
body += "};\n"
body += "/* Sun per venue: world bearing (binary angle, 0 = upwind) and height in px. */\n"
body += arr("uint16_t", "venue_sun_bearing", [int(VENUE_LIGHT[v]["sun_at"][0] * 65536 / 360) & 0xFFFF for v in range(5)])
body += arr("uint8_t", "venue_sun_elev", [VENUE_LIGHT[v]["sun_at"][1] for v in range(5)])
body += "/* Bay panoramas: per row (count, index) runs, index 255 transparent. */\n"
body += arr("uint8_t", "pano_rle", pano_rle)
body += arr("uint16_t", "pano_off", pano_off)
body += arr("uint8_t", "pano_h", pano_h)
body += "/* Far coast: 256 bearings, shade<<5 | height in px. */\n"
body += arr("uint8_t", "far_coast", far)
body += arr("uint8_t", "logo_rle", logo_rle)
body += arr("uint8_t", "cup_rle", cup_rle)
body += "#endif\n"
OUT.write_text(body)

# Preview sheets for the README and release artwork.
GEN.mkdir(parents=True, exist_ok=True)
pal_by_slot = {}
sheet = Image.new("RGB", (320, 60 * 5))
for v in range(5):
    colours = dict(VENUES[v])
    sky = [colours[f"SKY{i}"] for i in range(SKY_N)]
    for y in range(60):
        c = sky[min(SKY_N - 1, y * SKY_N // 48)] if y < 48 else colours[f"SEA{min(SEA_N - 1, (y - 48) * 2)}"]
        for x in range(320):
            sheet.putpixel((x, v * 60 + y), c)
    p = panoramas[v]
    top = 48 - len(p)
    for y, row in enumerate(p):
        for x, c in enumerate(row):
            if c is not None:
                sheet.putpixel((x, v * 60 + top + y), colours[f"PANO{PANO_COLOURS.index(c)}"])
sheet.save(GEN / "bay_of_naples_backgrounds.png")
print(f"wrote {OUT} ({OUT.stat().st_size} bytes): {len(fixed_names)} fixed + {len(VENUE_KEYS)} venue colours, "
      f"{SLOTS_LEFT} palette slots free, panoramas {len(pano_rle)} B rows {pano_h}, logo {LOGO_W}x{LOGO_H}")
