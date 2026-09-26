# 10. Pixels on a microcontroller: palettes, sprites, text and the HUD

## Learning objectives

- Explain **indexed colour** and convert RGB565 colours to palette indices.
- Decode **run-length encoded** images and **bitplane** sprites.
- Map world coordinates onto a small **map display** and draw instruments with trigonometry.

## 10.1 How the PRG32 stores the screen

A colour on the PRG32 is written in **RGB565**: 16 bits holding 5 bits of red, 6 of green and 5 of blue:

```
 15      11 10        5 4       0
 r r r r r  g g g g g g  b b b b b
```

A full 320×200 screen in RGB565 would need 128,000 bytes, too much RAM for the firmware to keep alongside everything else. The ILI9341 back end of the firmware therefore stores **one byte per pixel**: an *index* into a 256-entry **palette**. Most of the palette is a 6×6×6 **colour cube**, six levels each of red, green and blue:

```
index = 16 + 36·r + 6·g + b,        r, g, b ∈ {0, …, 5}
```

Indices 0–7 are eight "named" colours (black, white, red, green, blue, yellow, cyan, magenta), and 232–255 are greys. When the palette buffer is sent to the LCD, each index is looked up and expanded back to RGB565.

The palette is **runtime state**, not a constant. The firmware only loads its default at boot, the PRG32 emulators start from a different (3-3-2) palette, and any cartridge can change entries with `prg32_palette_set`, which survive into the next cartridge. A game that relies on the default palette therefore shows different colours on different devices and even from run to run. FairWind loads all 256 entries itself at start-up:

```c
static void set_palette(void) {
    /* 0-15 named and system colours, 16-231 the cube, 232-247 the panoramas'
       own 16 colours exactly, 248-255 a grey ramp */
    ...
    for (i = 0; i < 216; i++) prg32_palette_set(16 + i, C6(i / 36, i / 6 % 6, i % 6));
    for (i = 0; i < 16; i++)  prg32_palette_set(BG_BASE + i, fairwind_background_palette[i]);
    ...
}
```

This has two consequences for a game programmer.

**Only cube colours are shown exactly.** Any RGB565 colour you ask for is rounded to the nearest cube level. FairWind defines all its race colours directly as cube levels, so what you design is what you see:

```c
/* level (0..5) -> 5- or 6-bit channel, rounded up so the firmware maps it back to the same level */
#define C6(r,g,b) ((uint16_t)((((r)*31+4)/5)<<11 | (((g)*63+4)/5)<<5 | (((b)*31+4)/5)))
#define SEA2  C6(0, 2, 3)
#define DECK  C6(5, 5, 4)
```

**Drawing with an RGB565 colour costs a conversion per pixel.** Chapter 11 measures this and shows why FairWind draws through `prg32_gfx_rect_indexed` with a palette index it computes **once**:

```c
static uint8_t ci(uint16_t c) {                          /* RGB565 -> palette index */
    static const uint16_t named[8] = {0x0000,0xffff,0xf800,0x07e0,0x001f,0xffe0,0x07ff,0xf81f};
    uint8_t i;
    for (i = 0; i < 8; i++) if (c == named[i]) return i;
    return (uint8_t)(16u + ((c >> 11) & 31u) * 5u / 31u * 36u
                         + ((c >> 5) & 63u) * 5u / 63u * 6u
                         +  (c & 31u) * 5u / 31u);
}
static void fill(int x, int y, int w, int h, uint16_t c) { prg32_gfx_rect_indexed(x, y, w, h, ci(c)); }
```

`ci` repeats, byte for byte, the rule the firmware uses, so the result is identical on screen. It is simply computed once per rectangle instead of once per pixel.

## 10.2 Run-length encoded panoramas

Each of the five venue panoramas is 320×24 pixels in a 16-colour palette: 7,680 pixels. Stored raw, one byte per pixel, all five would take 38 KB. Scenery has long horizontal runs of the same colour (sky, sea, a cliff face), so FairWind stores it **run-length encoded (RLE)** as `(count, colour)` byte pairs. Rows never share a run, which keeps decoding simple. The five panoramas take 15.7 KB together.

Decoding is also cheap for us: every run becomes **one** horizontal rectangle, so there is no need to expand it to pixels first:

```c
while (i < end) {
    int count = fairwind_background_rle8[i++];
    uint8_t colour = bg_idx[fairwind_background_rle8[i++]];     /* palette index, precomputed */
    int a = x + off, b = x + off + count;                       /* horizontal scroll          */
    if (a < 0) a = 0;
    if (b > W) b = W;
    if (b > a) prg32_gfx_rect_indexed(a, y, b - a, 1, colour);
    x += count;
    if (x >= 320) { x = 0; y++; }
}
```

`bg_idx` holds the palette index of each of the 16 panorama colours. Because `set_palette()` loads those colours exactly into entries 232–247, `bg_idx[i]` is simply `232 + i`, so the decoder never converts a colour and the skyline keeps its authored shades.

## 10.3 Bitplane sprites: the logo

The FairWind logo on the title screen is a 96×96 **sprite** with 16 colours. Storing 4 bits per pixel, it needs 96 × 96 × 4 / 8 = 4,608 bytes. The data is stored as four **bitplanes**. Plane *p* holds bit *p* of every pixel's colour index, eight pixels per byte, row after row. A pixel's colour is reassembled by taking one bit from each plane:

```
index(x, y) = Σ  bit(plane p, x, y) · 2^p       for p = 0..3
```

Bitplanes were the native format of the Amiga and of many 1990s consoles. They make it easy to use fewer colours (fewer planes) for simple images: the trophy uses 4 planes, while a 2-colour icon would use 1.

The asset generator (`tools/generate_assets.py`) prepares the logo in three steps:

1. **Scale.** Resize the 512×512 source image to 96×96.
2. **Snap to the cube.** Map every pixel to its nearest level of the 6×6×6 cube.
3. **Keep 16 colours.** Keep the 16 most used colours and map every other pixel to its nearest kept colour.

Because the palette consists of cube colours only, the console shows exactly the colours of the preview image. This is a small instance of **colour quantisation**, a classic problem in image processing.

## 10.4 Text

The firmware draws text in an 8×8 pixel font with `prg32_gfx_text8(x, y, string, fg, bg)`. The cartridge has no `printf`, so numbers are formatted by hand. `small_num` writes an integer, optionally with one decimal (the input is in tenths):

```c
static void small_num(int x, int y, int n, int tenths, uint16_t fg, uint16_t bg) {
    char b[8];
    int i = 0, d = 1;
    if (n < 0) n = 0;
    if (n > 9999) n = 9999;
    while (n / d >= 10) d *= 10;              /* largest power of ten ≤ n */
    if (tenths && d < 10) d = 10;             /* at least "0.x"           */
    for (; d; d /= 10) {
        if (tenths && d == 1) b[i++] = '.';
        b[i++] = (char)('0' + n / d % 10);    /* next digit               */
    }
    b[i] = 0;
    prg32_gfx_text8(x, y, b, fg, bg);
}
```

Text is surprisingly expensive on this console: the firmware converts every text pixel's colour, which is why race text uses the "named" colours (Chapter 11).

## 10.5 Instruments: the wind gauge and the map

The **wind gauge** in the top-left corner is a circle of 16 dots with the yacht at the centre, bow up. The true wind is drawn as an arrow coming from its direction *relative to the bow*, (`twd − heading`), which is just a binary-angle subtraction. A point on a circle of radius *r* around (cx, cy) at angle θ is (cx + r·sin θ, cy − r·cos θ), with the minus sign because screen *y* grows downwards:

```c
uint16_t rel = (uint16_t)(twd - b->heading);
line2(31 + (int)((22 * fsin(rel)) >> 14), 46 - (int)((22 * fcos(rel)) >> 14),     /* tail on the rim  */
      31 + (int)(( 8 * fsin(rel)) >> 14), 46 - (int)(( 8 * fcos(rel)) >> 14),     /* head near centre */
      GOLD);
```

Below the gauge, `L` or `H` and a number show the current **lift** or **header** in degrees. Its sign is the wind shift multiplied by the tack: a positive product means the shift lets you point closer to the mark.

The **map** in the top-right corner is a linear mapping of the 1.8 × 1.7 km field onto 60×57 pixels, 30 m per pixel, drawn with the upwind direction at the top:

```c
static int map_x(int32_t x) { return 257 + (int)((x + FIELD_X) / 30); }
static int map_y(int32_t y) { return 24  + (int)((FIELD_Y1 - y) / 30); }
```

It shows:
- the shore on three sides, and a dotted race-area limit;
- the marks, with the next one blinking;
- the start or finish line;
- the rivals as coloured dots;
- the player's yacht with a heading tick;
- a small wind arrow;
- from Strategy level 2, the **laylines** to a windward mark: two lines leaving the mark at the wind direction ± 42°.

## 10.6 The HUD

The bottom bar shows:
- the clock (countdown `T-` or race time `R`);
- the speed in knots and the true wind angle;
- the sail plan (`JIB`, `SPI`, `GEN`), with the hoist percentage while a kite goes up or down;
- the next mark;
- the trim bar: the green optimum and the white boom, with the words `LUFF`, `GOOD` or `STALL`;
- one line of messages, such as `ENTER PORT`, `CROSS LINE`, the rule of a penalty, or `AGROUND`.

The whole screen layout follows one simple rule: the 3D world lives in rows 18–179, and every overlay is drawn **after** the world, so the painter's algorithm keeps them on top.

## Check your understanding

1. How many bytes would a 96×96 logo take at 8 bits per pixel? At 2 bits per pixel?
2. Which RGB565 value does `C6(5,5,5)` produce, and which palette index does `ci()` return for it? And for `C6(3,3,3)`?
3. Why is `sin` used for *x* and `cos` for *y* when drawing the wind arrow?

## Exercises

- ★ Add the distance to the next mark (in metres) to the HUD. Keep the text short (see Chapter 11).
- ★★ Write an RLE encoder in Python that also merges identical consecutive rows ("row repeat"), and measure how much it saves on the five panoramas.
- ★★★ Replace the 4-plane logo with a 3-plane (8-colour) version. Compare the size and the visual quality, and argue which you would ship.
