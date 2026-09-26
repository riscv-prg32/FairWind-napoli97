# 11. Performance engineering on the ESP32-C6

## Learning objectives

- Build a **cost model** of a program from the hardware and the software it calls.
- **Measure** instead of guessing: instruction counting, isolating one component, calibration.
- Apply the classic optimisations: do less, do it once, move work out of inner loops, avoid expensive operations.
- Protect the result with **regression tests**.

## 11.1 "It runs smoothly on my laptop"

Version 4.0.0 of FairWind looked perfect in the host renderer and in the emulator. It would have run at about **5 frames per second** on the real console. Nothing in its code was *wrong*. It simply spent its time in places that are cheap on a laptop and expensive on a microcontroller.

This chapter retraces, as an experiment you can repeat, how that was discovered and fixed. The full data are in `docs/PERFORMANCE.md`.

## 11.2 Step 1: understand where time can go

Before measuring, read the code that runs around yours. The PRG32 firmware calls, every frame:

```
fairwind_update();  fairwind_draw();  prg32_gfx_present();  wait until 33 ms have passed;
```

Reading the display driver reveals the facts that matter:

- **The screen is a 1-byte-per-pixel palette buffer.** Drawing with an RGB565 colour (`prg32_gfx_rect`) converts the colour **for every pixel**. The firmware is compiled at `-Og`, where that conversion runs a small loop and three integer divisions.
- **Indexed drawing (`prg32_gfx_rect_indexed`) is a `memset` per row.**
- **Text** converts every pixel's colour too.
- **`present` sends the changed area over a 32 MHz SPI bus and waits for it.** A full frame is 128,000 bytes, which takes 32 ms. That alone is almost the whole 33 ms budget.

From these facts alone you can write down a **cost model**, a formula for the time of one frame:

```
T_frame ≈ T_cartridge + (pixels filled × cost per pixel) + (text chars × cost per char)
                      + (pixels presented × conversion cost) + (bytes presented × 8 / 32 MHz)
```

A model tells you *which numbers to measure*. Here: instructions spent by the cartridge, pixels and characters drawn, and pixels presented.

## 11.3 Step 2: measure instructions, exactly

Wall-clock time in an emulator says nothing about a real chip. But the number of **instructions executed** is a property of the program, and QEMU can count it. With `-icount shift=4`, each executed instruction advances QEMU's virtual clock by exactly 16 ns. The firmware's microsecond timer then measures instructions:

```
instructions = measured µs × 1000 / 16
```

The profiling cartridge (`tools/profile/profile_cart.c`) wraps the game's entry points:

```c
void fairwind_draw(void) {
    uint32_t t0 = (uint32_t)prg32_perf_now_us(), dt;
    game_draw();                                   /* the real fairwind_draw, renamed */
    dt = (uint32_t)prg32_perf_now_us() - t0;
    /* accumulate mean and maximum over 64 race frames, then print them */
}
```

Two tricks make the experiment clean:

1. **Isolate one component.** `profile_null.c` replaces every drawing call with a counter:

   ```c
   #define prg32_gfx_rect_indexed(x, y, w, h, c) null_rect((x), (y), (w), (h))
   ```

   The timers then measure *only* the cartridge's own work, and the counters record *what it asked the firmware to draw*.
2. **Calibrate what you cannot run.** The ILI9341 driver does not run in QEMU, so its hot loops were copied into the profiling cartridge and compiled at `-Og`, like the firmware, with `__attribute__((optimize("Og")))`. Then they were timed. For example, one 8×8 character in white on navy costs about **7,000 instructions**, and in white on black about **2,400**.

## 11.4 Step 3: find the bottleneck

Put the measurements into the model for version 4.0.0, per race frame:

| Work | Quantity | Cost | Instructions |
|---|---|---|---|
| Cartridge (physics, AI, 3D) | – | measured | ≈ 0.3 M |
| RGB565 fills | ≈ 82,000 px | ≈ 100 / px | ≈ 8 M |
| Full-screen clear (RGB565) | 64,000 px | ≈ 100 / px | ≈ 6.4 M |
| Text on navy | ≈ 80 chars | ≈ 7,000 / char | ≈ 0.6 M |
| `present` conversion | 64,000 px | ≈ 5 / px | ≈ 0.3 M |

About 15 million instructions, of which the game's *own* work, the part a programmer instinctively optimises, is only 2%. **The bottleneck was not where intuition put it.** Always measure before optimising.

## 11.5 Step 4: optimise

Each change below attacks a specific term of the model.

**1. Convert colours once, not per pixel.** Every fill goes through `prg32_gfx_rect_indexed`, with the palette index computed once per call by `ci()`, the firmware's own rule (Chapter 10). The per-pixel conversion disappears; a row becomes a `memset`.

**2. Do not draw what you will overwrite.** The race view paints every pixel, so the full-screen clear was pure waste and was removed.

**3. Do not send what has not changed.** The race header never changes, so it is drawn once. The HUD numbers do not need 30 updates per second, so they refresh one frame in four (7.5 Hz). On three frames out of four, `present` sends 162 rows instead of 200, saving 6 ms of SPI per frame. Idle menus send nothing at all (Chapter 2's dirty flag).

**4. Make the remaining expensive work cheap.** Text now uses black backgrounds and the firmware's "named" colours, which skip the divisions. The start banners were shortened. Text fell from about 80 characters per frame to 24, each three times cheaper.

**5. Move divisions out of inner loops.** The polygon filler used to divide once per edge per scanline. It now computes each edge's slope once, in 16.16 fixed point (Chapter 9). Integer division is one of the slowest instructions on a small RISC-V core.

**6. Make the result independent of speed.** The simulation now integrates the real frame time (Chapter 5). The occasional slow frame therefore makes the animation slightly less smooth but never makes the boats slower.

The result, per average race frame:

| Work | Instructions |
|---|---|
| Cartridge | ≈ 0.31 M |
| Indexed fills, pixels, text | ≈ 0.2 M |
| `present` conversion | ≈ 0.3 M |
| **Total CPU** | **≈ 0.8 M ≈ 5–8 ms at 160 MHz** |
| SPI transfer | ≈ 27 ms |

About **30 fps**, limited now by the SPI bus, which no cartridge can go faster than. The CPU work fell by a factor of about twenty.

## 11.6 Step 5: keep it fast

Optimisations decay: the next feature adds "just one" `prg32_gfx_rect` and the frame rate quietly halves. So the design is encoded as **tests** that run on every `make test`:

```c
CHECK(g_rgb_px == 0,           "budget: race frames draw with palette indices only");
CHECK(top_frames == 0,         "budget: the static race header is never redrawn");
CHECK(hud_frames * 3 < frames, "budget: the HUD refreshes at most one frame in four");
CHECK(max_chars <= 64,         "budget: at most 64 text characters in any race frame");
```

These are **performance regression tests**. They do not measure time, which would be fragile. They check the *properties* that make the program fast.

## 11.7 Lessons

1. **Read the platform** before writing code for it: here, a 30 fps pacing, an indexed frame buffer and a blocking SPI bus.
2. **Model, then measure.** The model says what to measure; the measurement corrects the model.
3. **Isolate and calibrate.** Null back ends and copied inner loops turn an emulator into a measuring instrument.
4. **The biggest win is usually "do less"**: fewer pixels converted, fewer bytes sent.
5. **Encode the budget in tests.**

## Check your understanding

1. Why is counting instructions in QEMU more meaningful than timing QEMU with a stopwatch?
2. Why does the HUD refresh at 7.5 Hz save SPI time on three frames out of four, and not on all four?
3. A colleague proposes to draw the sea as one big rectangle and then draw the ripples on top. Using the model, is this faster or slower than drawing the dithered rows directly?

## Exercises

- ★ Run `NULL_GFX=1 python3 tools/profile/qemu_profile.py 300` and compare the stern-view and top-view draw costs. Explain the difference.
- ★★ Add a third calibration: the cost of `prg32_gfx_pixel_indexed`, measured by calling a copy of it 10,000 times. Update the model.
- ★★★ Reduce the SPI transfer further: the wind gauge and the map change slowly. Design a scheme that redraws them less often without leaving stale pixels behind when the world beneath them is repainted. Measure the gain, and write the regression test.
