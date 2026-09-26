# Performance on the ESP32-C6

This document records how FairWind's per-frame cost was measured and budgeted for the physical PRG32 (ESP32-C6, 160 MHz RV32IMAC, ILI9341 over 32 MHz SPI). It also records what changed in 4.1.0 to make the race fluid on hardware.

The frame rate has **not yet been measured on a physical board**. The numbers below combine:
- exact instruction counts measured inside the real PRG32 firmware under QEMU;
- per-pixel costs of the firmware's own drawing loops, measured the same way;
- the SPI transfer time implied by the bus clock.

The last checklist step is a confirmation on hardware: build the firmware with `sdkconfig.defaults.metrics` and read the frame metrics.

## 1. How the firmware spends a frame

The PRG32 main loop calls, once per frame and while holding the graphics lock:

```
fairwind_update()  →  fairwind_draw()  →  prg32_gfx_present()  →  wait for the 33 ms frame target
```

Four facts about the ILI9341 back end (`components/prg32/prg32_display_ili9341.c`) dominate the budget:

1. **The frame buffer is palette-indexed** (1 byte per pixel). `prg32_gfx_rect(…, rgb565)` converts the colour to an index **for every pixel**, calling `prg32_gfx_index_for_rgb565_unlocked`. The firmware is built with ESP-IDF's default `-Og`, and that conversion runs a loop over eight named colours and then **three real `divu` instructions** for any other colour.
2. `prg32_gfx_rect_indexed` fills each row with **`memset`**; `prg32_gfx_pixel_indexed` writes one byte.
3. `prg32_gfx_text8` converts the colour of **every text pixel** the same way as in (1).
4. `prg32_gfx_present` sends the bounding box of everything drawn since the last frame. It converts each pixel back to RGB565 in 8-row strips and sends them with `spi_device_polling_transmit`, which **blocks**. At 32 MHz, one full 320×200 frame is 128,000 bytes = **32.0 ms**. Every full-screen game on the platform is therefore limited to roughly 25–30 fps by the bus alone.

## 2. Method

### 2.1 Instruction counting in the real firmware

`tools/profile/qemu_profile.py`:
- builds `tools/profile/profile_cart.c` (the game, plus an autopilot and timers) as a portable cartridge;
- runs it in the PRG32 firmware on QEMU's ESP32 machine with `-icount shift=4`, so every guest instruction advances virtual time by exactly 16 ns (`shift=0` does not boot this machine);
- times `fairwind_update` and `fairwind_draw` with `prg32_perf_now_us()` and prints averages and maxima over windows of 64 race frames.

A measured interval of *t* µs is *t* × 62.5 instructions. Neither function waits or sleeps, so the count is exact apart from the timer interrupts that happen to fall inside a window. Those interrupts add some noise; repeated calibration runs differ by up to about 15%.

With `NULL_GFX=1` the build `tools/profile/profile_null.c` replaces every drawing call by a counter. This measures **the cartridge's own work** (physics, AI, 3D transforms, clipping, polygon set-up) and records exactly what it asks the firmware to draw.

### 2.2 Calibrating the firmware's drawing loops

QEMU uses a different display back end, so the ILI9341 loops cannot be timed there directly. The profiling cartridge therefore contains **exact copies** of the three hot ILI9341 loops:
- the palette conversion;
- the per-character text loop;
- the `present` row conversion.

They are compiled with `__attribute__((optimize("Og")))` by the same GCC 14.2 as ESP-IDF 5.4, and timed once:

| Firmware operation (ILI9341, `-Og`) | Measured instructions |
|---|---|
| `present`: palette → RGB565 conversion | 5.0–6.4 per pixel |
| One 8×8 character, white on navy | 6,164–7,053 |
| One 8×8 character, white on black | 2,378–2,384 |
| One 8×8 character, gold on black | 5,011–5,901 |
| RGB565 fill with a non-named colour | ≈ 96–110 per pixel (from the navy text cost) |

A white-on-black character is three times cheaper than white-on-navy, because both colours are "named" and skip the divisions.

### 2.3 Converting instructions to time

Time = instructions × CPI / 160 MHz, with a cycles-per-instruction range of 1.1–1.5. The ESP32-C6 high-performance core is a short in-order pipeline; loads, taken branches and especially divisions cost extra cycles. The SPI time is bytes × 8 / 32 MHz.

## 3. Results

### 3.1 The cartridge's own work (4.1.0, null graphics)

Averages over 64-frame windows of an autopiloted race, pre-start included:

| Per race frame | Mean | Worst window | Worst single frame |
|---|---|---|---|
| `fairwind_update` | 18 k instructions | 21 k | 26–27 k |
| `fairwind_draw`, stern view | 297–364 k | | |
| `fairwind_draw`, top view | 245–251 k | | |
| `fairwind_draw`, overall | 263–293 k | 410–498 k | 416–574 k |

The ranges span two complete runs, each a pre-start plus one full race. The later run also played the soundtrack, as a real console does; the firmware's audio work then lands inside some timed windows, and it disturbed the text calibration of §2.2. That is why the firmware-loop costs above come from the quiet run.

What the game asks the firmware to draw, per race frame:

| Request | Mean | Max |
|---|---|---|
| Indexed rectangles | 663 (1,186 rows, 70,774 px) | 2,126 |
| Indexed single pixels | 326 | 981 |
| Text characters | 24 | 61 |
| RGB565 fills | **0** | 0 |

The text figures come from the behavioural harness's draw-budget scenario, after the text reductions of §4.

### 3.2 Estimated ESP32-C6 frame

| Component (mean race frame) | Instructions | Time |
|---|---|---|
| Cartridge (update + draw) | ≈ 0.31 M | |
| Indexed fills (memset + call overhead, modelled) | ≈ 0.11 M | |
| Single pixels (≈ 40 each) | ≈ 0.01 M | |
| Text (24 chars, mostly named colours) | ≈ 0.06–0.14 M | |
| `present` conversion (167 rows on average) | ≈ 0.27–0.34 M | |
| **CPU subtotal** | **≈ 0.76–0.91 M** | **≈ 5.2–8.5 ms** |
| SPI: 162 rows on 3 frames of 4, 182 on the 4th | | **≈ 26.7 ms** |
| **Frame** | | **≈ 32–35 ms → 28.5–31 fps** |

The runtime paces frames at 33 ms, so the expected steady rate is **about 30 fps**, SPI-bound. The worst frames (about 1.4 M instructions, 10–13 ms of CPU) take about 37–42 ms, a momentary 24–27 fps. Because the simulation integrates real frame time (§4.4), such frames do not change the yachts' speed.

### 3.3 Before: 4.0.0

4.0.0 drew everything through RGB565 fills and cleared the whole screen every frame. Per race frame that meant about 147,000 pixels of per-pixel conversion:
- about 82,000 pixels of scene;
- a 64,000-pixel clear;
- 75–88 characters of navy-backed text.

At about 100 instructions per pixel that is roughly **15 M instructions**, plus the multi-cycle divisions: some 100–200 ms per frame on the ESP32-C6, before the 32 ms SPI transfer. **About 4–7 fps.** The 3.x top-down version, which also cleared and redrew full-screen RGB565 every frame, sat in the same range.

## 4. What changed in 4.1.0

1. **Indexed drawing.** Every fill uses `prg32_gfx_rect_indexed`, `prg32_gfx_pixel_indexed` or `prg32_gfx_clear_indexed`. The palette index is computed once per call by `ci()`, which is the firmware's own conversion rule, so the image is identical. The five panorama palettes are converted once at start-up (`bg_idx`).
2. **No redundant clear in races.** The race view paints every pixel of rows 18–199, so the 64,000-pixel clear is skipped.
3. **Less SPI traffic.**
   - The race header (rows 0–17) is drawn once per race and never becomes dirty again.
   - The bottom HUD refreshes at 7.5 Hz (one frame in four), so three frames in four push only rows 18–179: 103,680 bytes instead of 128,000.
   - Menus redraw only after input, a screen change, or twice a second in the lobby. An idle menu sends nothing.
4. **Frame-time integration.** The simulation advances by the real frame time (`prg32_ticks_ms`, clamped to 10–66 ms). Speeds, turning, hoists and the race clock are therefore identical at any sustained frame rate, and consoles in a room agree. The harness checks that 15 fps and 30 fps give the same yacht within about a metre after 53 simulated seconds. 4.0.0 assumed 60 updates per second, while the runtime delivers at most 30, so it would have run at half speed on hardware.
5. **Cheaper text.**
   - Race text uses the named colours (white, yellow, cyan, green, red) on black.
   - Start-signal banners were shortened to the flag in force.
   - Two HUD labels were removed.

   That brings text down from about 80 to 24 characters per frame, and cheaper ones.
6. **Cheaper polygons.** Each polygon edge's 16.16 slope is computed once, so a scanline costs multiplications, not divisions.

## 5. Guarding the budget

The behavioural harness (`tests/harness/run_harness.c`) enforces the design on every `make test`:

| Scenario | Checks |
|---|---|
| `run_draw_budget_scenario` | No RGB565 fill in any race frame; the header is never redrawn; the HUD refreshes on at most one frame in four; at most 96 text characters in any frame and 56 on average; an idle menu draws nothing. |
| `run_frame_rate_scenario` | 15 and 30 fps produce the same yacht. |

`tests/source_checks.py` also rejects any `prg32_gfx_rect(` or `prg32_gfx_pixel(` call in `src/game.c`.

## 6. Confirming on hardware

1. Build the PRG32 firmware with the metrics profile:
   `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.metrics" build`
2. Install `dist/store/FairWind-napoli97-esp32c6.prg32`, start a race, and read `update_us`, `draw_us`, `present_us` and `fps_mean` from the metrics server. The PRG32 performance-test guide explains the fields.
3. Expect `present_us` ≈ 27–31 ms, `update_us + draw_us` of a few milliseconds, and `fps_mean` ≈ 28–30 in the stern view.

## 6. Navy race panels (after 4.1.0)

The race instruments, HUD and start banners went back to the 4.0.0 look: navy panels, orange and grey labels, the full start-sequence banners and the `TOP VIEW` label. Navy is not a named colour, so each of these characters costs about 7,000 instructions instead of about 2,400. The harness's worst case, a pre-start the player never leaves, averages 46 characters per race frame (maximum 81) instead of 24 (maximum 61). That adds about 0.25 M instructions, roughly 1.7–2.4 ms, per frame. The frame stays SPI-bound at about 26.7 ms of transfer, so the estimate is 27–30 fps in the pre-start and about 30 fps racing. Confirm on hardware with the frame metrics.
