# 1. Setting up: the console, the tools and the cartridge model

## Learning objectives

By the end of this chapter you will be able to:

- describe the hardware a PRG32 cartridge runs on and the limits it imposes;
- explain the three entry points every cartridge exports and when the firmware calls them;
- build FairWind, run its tests, and render real frames on your own computer.

## 1.1 The machine

The PRG32 is a hand-held console built around an **Espressif ESP32-C6** microcontroller:

| Resource | Value | Why it matters to us |
|---|---|---|
| CPU | 32-bit RISC-V (RV32IMAC) at 160 MHz | No floating-point unit: every `float` operation would be emulated in software. |
| Memory for a cartridge | 64 KiB, code and data together | The whole game, including images, must fit. |
| Display | ILI9341 LCD, 320×240, driven over a 32 MHz SPI bus | The game draws in a 320×200 window; the firmware owns two status bands. |
| Frame pacing | One frame every 33 ms (30 frames per second) | Our code must finish its work well inside this time. |
| Controls | D-pad, A, B | Six buttons for everything. |

Compare this with the laptop you are reading on: roughly ten thousand times more memory and a hundred times more raw speed, plus a hardware floating-point unit. Many of the design choices in this tutorial exist only because of the column on the right. They are good habits nonetheless: code that respects its budget on a microcontroller is fast everywhere.

## 1.2 What a cartridge is

A **cartridge** is a small binary loaded by the PRG32 firmware into a reserved 64 KiB region of RAM. It is *not* a complete program: there is no `main`. Instead the firmware calls three functions that the cartridge exports:

```c
void fairwind_init(void);    /* once, after loading                  */
void fairwind_update(void);  /* once per frame: read input, simulate */
void fairwind_draw(void);    /* once per frame: paint the screen     */
```

(The `fairwind_` prefix is chosen at build time with `--entry-prefix fairwind`.) The firmware's main loop is, in essence:

```c
while (1) {
    input = prg32_controller_read();
    prg32_gfx_lock();
    fairwind_update();
    fairwind_draw();
    prg32_gfx_present();          /* send the changed pixels to the LCD */
    prg32_gfx_unlock();
    wait_for_next_33ms_tick();
}
```

This is the classic **game loop**: *read input → update the world → draw the world → present*, repeated forever. Separating *update* from *draw* is the first design principle of the whole game. Update may change the state but never draws; draw may read the state but never changes it. We return to this in Chapter 2.

The cartridge calls back into the firmware through a table of function pointers, the **portable ABI** (Application Binary Interface). To the C programmer it looks like ordinary function calls declared in `prg32.h`:

```c
uint32_t prg32_input_read(void);                        /* button bit mask */
uint32_t prg32_ticks_ms(void);                          /* milliseconds    */
void prg32_gfx_rect_indexed(int x, int y, int w, int h, uint8_t colour);
void prg32_gfx_text8(int x, int y, const char *s, uint16_t fg, uint16_t bg);
void prg32_audio_play_track(uint16_t track);
/* ... multiplayer, sprites, pixels, ... */
```

`src/platform.h` wraps the few calls whose names or types might change between firmware versions, so a change touches one file only.

> **No C library.** A cartridge is compiled with `-ffreestanding` and is not linked with the standard library. There is no `printf`, no `malloc`, no `sin`, no `memcpy`. FairWind even defines its own `memcpy` and `memset`, because the compiler may silently generate calls to them when it copies a `struct`. Everything else (trigonometry, square roots, number formatting) the game implements itself, in integers. Chapter 4 shows how.

## 1.3 Getting the tools

You need:

1. a C compiler for your own computer (`cc`/`clang` on macOS, `gcc` on Linux, or WSL on Windows);
2. Python 3 with Pillow (`python3 -m pip install -r requirements-dev.txt`);
3. for cartridge builds, a checkout of the PRG32 firmware repository and the ESP-IDF RISC-V toolchain (`riscv32-esp-elf-gcc`).

The first two are enough for most of this tutorial: the game's logic and its graphics can be run and tested on your own machine.

## 1.4 Three ways to run the game

**On your computer, as tests.** `tests/harness/run_harness.c` includes `src/game.c` directly and replaces every firmware function with a small fake: input comes from variables, drawing only counts pixels. It then plays whole seasons and checks hundreds of properties.

```sh
make test
```

`make test` runs three things:
- `tests/source_checks.py`, static checks on the source and metadata;
- a strict compile of the game with `-Wall -Wextra -Werror`;
- the harness, compiled with the AddressSanitizer and UndefinedBehaviorSanitizer, which stop the program at the first out-of-bounds access or signed overflow.

**On your computer, with pictures.** `tools/host_render.c` fakes the firmware with a real 320×200 framebuffer and saves PNG images. It needs the PRG32 checkout for the 8×8 font:

```sh
PRG32_ROOT=../PRG32 python3 tools/host_capture.py /tmp/frames 0 1
```

The arguments are: output folder, course (0–2), venue (0–4). The player's yacht is sailed by an *autopilot* (Chapter 8), so you get a whole race of frames.

**On the console, or in the QEMU emulator.** With the RISC-V toolchain installed:

```sh
PRG32_ROOT=/path/to/PRG32 ./build.sh
```

This produces:
- two cartridge files, `dist/store/FairWind-napoli97-esp32c6.prg32` (hardware) and `...-qemu.prg32` (emulator);
- the Cartridge Store bundle `dist/FairWind-napoli97-<version>-store.zip`.

The build script refuses any cartridge larger than 65,536 bytes.

## 1.5 A first look at `game.c`

Open `src/game.c`. It is organised in sections, marked by comments such as `/* ---- physics ---- */`:

| Section | Chapter |
|---|---|
| Constants, types, tables | 3 |
| Wind and time | 5 |
| Polar and trim, physics | 6 |
| AI | 8 |
| Rules, race state | 7 |
| Menus | 2 |
| 3D rasteriser, scenery, yachts | 9 |
| HUD | 10 |

The three entry points are near the middle and the end of the file. `fairwind_update` is short enough to read now, in its reformatted version:

```c
void fairwind_update(void) {
    uint32_t in  = prg32_input_read();
    uint32_t p   = in & ~last_input;      /* buttons pressed this frame  */
    uint32_t r   = last_input & ~in;      /* buttons released this frame */
    uint32_t now = prg32_ticks_ms();
    frame++;
    frame_ms = last_ticks ? clamp((int)(now - last_ticks), 10, 66) : FRAME_MS;
    last_ticks = now;
    if (p) ui_dirty = 1;
    if (screen == ST_TITLE && (p & PRG32_BTN_A)) screen = ST_MODE;
    else if (screen == ST_MODE) { /* ... */ }
    /* ... one branch per screen ... */
    else if (screen == ST_RACE) update_race(in, r);
    /* ... */
    last_input = in;
}
```

Three ideas are visible already, and each gets a chapter:
- **Edge detection.** Comparing this frame's buttons with last frame's finds *presses* and *releases* (Chapter 2).
- **Frame time.** The firmware clock measures how long the last frame really took (Chapter 5).
- **Screen state machine.** One branch runs per screen (Chapter 2).

## Check your understanding

1. Why does a cartridge have no `main` function? Who owns the loop?
2. Name two things you cannot do in a cartridge that you do daily in a normal C program.
3. Why is it useful that the test harness *includes* `game.c` rather than linking with it? (Hint: `static` functions.)

## Exercises

- ★ Build and run the harness with `make test`. Find the line that reports the polar speed on a beam reach. What speed does the yacht reach in 12 knots of wind?
- ★ Render a race with `tools/host_capture.py` for course 2 (1992 IACC Z) at venue 3 (Sorrento). Look at `05-prestart.png` and `store-run.png`.
- ★★ Write a minimal cartridge `hello.c` with `hello_init`, `hello_update` and `hello_draw` that draws a filled rectangle whose colour changes when A is pressed. Test it by writing a ten-line fake of `prg32_input_read` and `prg32_gfx_rect_indexed` that prints what it is asked to draw.
