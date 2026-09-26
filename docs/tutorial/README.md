# Building FairWind: a sailing game in C for a RISC-V console

*A step-by-step tutorial for first-year BSc students of Computer Science and Computer Engineering, written to accompany a course on computer programming in C.*

## Why a sailing game?

Programming courses often ask students to write small, disconnected exercises: sort an array, parse a string, simulate a bank account. They are useful, but they rarely show how the pieces of a real program fit together, or why a professional programmer makes the choices they make.

This tutorial follows the opposite route. It takes one complete, working program, **FairWind-napoli97**, and rebuilds it step by step. FairWind is a sports-management and sailing game for the PRG32, a small RISC-V console built around the Espressif ESP32-C6 microcontroller. The player manages a fictional 12-Metre America's Cup syndicate and helms the yacht, from behind the stern, against computer-controlled rivals or friends on other consoles.

A sailing game makes a good teaching vehicle because every part of it rests on ideas you meet in your first year:

| Game feature | Programming and computing concepts |
|---|---|
| Menus, races, results | State machines, `enum`, `switch`-like control flow |
| Yachts, teams, sponsors | `struct`, arrays of structs, constant tables |
| Wind, speed, heading | Integer and fixed-point arithmetic, units, overflow |
| Boat physics | Numerical integration, first-order systems, interpolation |
| Racing rules | Geometry with integers, predicates, priority rules |
| Computer rivals | Control loops, decision rules, hysteresis |
| 3D view | Coordinate transforms, projection, clipping, rasterisation |
| Running on a microcontroller | Memory budgets, cost models, measurement |
| Multiplayer | Bit fields, packing and unpacking, protocols |
| Quality | Testing, sanitizers, regression checks |

## How to read this tutorial

Each chapter follows the same pattern:

1. **Learning objectives**: what you should be able to do at the end.
2. **Background**: the idea explained without code.
3. **Design**: how FairWind applies the idea, and which alternatives were rejected.
4. **Implementation**: the relevant C code, quoted from `src/game.c` and `src/fixmath.h`. The shipped source is written compactly, sometimes several statements to a line, because every byte of a 64 KiB cartridge counts. The excerpts here are **reformatted for readability**; the logic is identical.
5. **Check your understanding**: short questions to test yourself.
6. **Exercises**: graded ★ (short), ★★ (an afternoon) and ★★★ (a small project).

Read the chapters in order the first time. Later, use them as a reference while you modify the game.

## Chapters

1. [Setting up: the console, the tools and the cartridge model](01-setup.md)
2. [The game loop and the screen state machine](02-game-loop.md)
3. [Data: structs, tables and the campaign](03-data.md)
4. [Numbers without floating point: fixed-point arithmetic](04-fixed-point.md)
5. [Time and wind: simulated time, frame time and deterministic randomness](05-time-and-wind.md)
6. [Sailing physics: apparent wind, polars, trim and momentum](06-sailing-physics.md)
7. [Racing: the start, the marks, the finish and the rules](07-racing-rules.md)
8. [Artificial intelligence: rivals that sail like sailors](08-ai.md)
9. [3D graphics from scratch: cameras, projection, clipping and polygons](09-3d-graphics.md)
10. [Pixels on a microcontroller: palettes, sprites, text and the HUD](10-pixels-hud.md)
11. [Performance engineering on the ESP32-C6](11-performance.md)
12. [Multiplayer and testing](12-multiplayer-testing.md)

## Prerequisites

You should already know, or be learning in parallel:

- C syntax: variables, expressions, `if`, `for`, `while`, functions;
- arrays and strings;
- `struct` and `enum`;
- pointers at the level of "pass a struct by address";
- the binary representation of integers (two's complement, bit shifts).

No previous knowledge of sailing, physics beyond secondary school, or computer graphics is assumed. A short sailing glossary is given at the end of this page.

## The source files you will read

| File | Content |
|---|---|
| `src/game.c` | The whole game: about 750 dense lines of C11 |
| `src/fixmath.h` | Integer trigonometry, arctangent and square root |
| `src/platform.h` | Thin wrappers around the PRG32 firmware API |
| `src/assets_bitplanes.h` | Generated image data (panoramas, logo, trophy) |
| `tests/harness/run_harness.c` | The behavioural test harness |
| `tools/host_render.c` | Runs the game on a PC and saves real frames |
| `tools/profile/*` | Measures the game's cost inside the real firmware |

## A sailing glossary for programmers

| Term | Meaning |
|---|---|
| **Wind direction** | The direction the wind comes *from* (a "south-westerly" blows from the SW). |
| **Heading** | The direction the bow points. |
| **True wind angle (TWA)** | Angle between the heading and the wind direction, as measured by a stationary observer. |
| **Apparent wind** | The wind felt on the moving boat: true wind minus the boat's own velocity. |
| **Port / starboard** | Left / right side of the boat, looking forward. |
| **Port / starboard tack** | Sailing with the wind coming over the port / starboard side. |
| **Tack / gybe** | Turning so that the wind changes side: through the wind's eye (tack) or with the wind behind (gybe). |
| **Beat, reach, run** | Sailing upwind, across the wind, downwind. |
| **Luffing** | A sail eased too far flaps and stops driving the boat. |
| **Sheet** | The rope that controls a sail's angle; *easing* lets it out, *trimming* pulls it in. |
| **Boom** | The spar at the foot of the mainsail. |
| **Spinnaker / gennaker** | Large light sails for downwind (symmetric) and reaching (asymmetric) courses. |
| **Mark** | A buoy that must be rounded. |
| **Layline** | The line from which a boat can just reach an upwind mark without another tack. |
| **VMG** | Velocity made good: the component of speed towards where you want to go. |
| **Polar** | A table giving a boat's speed for each wind speed and angle. |
| **Header / lift** | A wind shift that forces you to steer away from / lets you steer closer to your target. |
