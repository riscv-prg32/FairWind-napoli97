# Technical notes

## World, units and time

The race field is a course-aligned plane in metres: world +y points into the mean south-westerly breeze, +x lies to starboard when facing upwind. The field spans x ±900 m and y −450…+1250 m. The start line runs ±110 m across y = 0, with the pre-start box 200 m deep below it. The finish line runs ±90 m across y = −80. Yacht positions are stored in millimetres, speeds in mm/s ×256, and headings as 16-bit binary angles (65 536 = 360°, clockwise from +y).

Time is simulated. Before the two-minute box entry the simulation runs at 20× real time; afterwards, and for the whole race, at 4×. The firmware paces cartridges at 33 ms per frame. The game measures each frame's real duration with `prg32_ticks_ms` (clamped to 10–66 ms) and integrates `time scale × frame time` simulated milliseconds. Every rate is expressed per simulated second, so the yachts sail identically at 30 fps or at a lower sustained rate (the harness checks 15 vs 30 fps agree within about a metre after 53 s). Whole simulated seconds (race clock, signals, wind) come from a millisecond accumulator.

## Fixed-point maths

`src/fixmath.h` provides everything the engine needs without floating point or a C library:
- `fsin`/`fcos`: a 65-entry quarter-wave Q14 sine table with linear interpolation.
- `bearing(dx, dy)`: a 14-iteration CORDIC that returns the compass heading of a vector.
- `isqrt`: an integer square root.

## Sailing model

- **Wind.** `update_wind` is a pure function of the course seed and elapsed simulated seconds: two sine shifts (±7° over ~7 min, ±4° over ~3 min), a persistent veer or back of up to ±5° over the race, and pressure pulses around a base of 8–16 kt. Every console in a multiplayer room therefore sails the same breeze.
- **Apparent wind.** For each yacht the true wind angle and the yacht's velocity give apparent angle and speed. The tack is the sign of the true wind angle; Rule 13 "tacking" runs from passing head to wind until 38° off it.
- **Boom and trim.** The wind carries the boom to leeward until it reaches the eased sheet, or streams with the apparent wind if that is closer. It walks over in tacks and slams across in gybes. The best boom angle keeps about 18° angle of attack. Easing past it loses drive quadratically to zero, the luffing point. Over-trimming stalls, to a floor of 30%.
- **Speed.** Target = ORC polar (TWA, TWS) × sail-plan efficiency (jib, spinnaker or gennaker, blended while a kite is hoisting) × trim efficiency × syndicate performance. The yacht approaches it with a 7–11 s acceleration and a 14 s coast time constant. Rudder angle costs up to ~2% of speed per second. Yaw rate grows from 3°/s at rest to 12°/s at 8 kt. A small leeway drift carries a stopped yacht downwind.
- **Kites.** A state machine hoists over 16–30 s and drops over 12–19 s, depending on crew level. A request for the other kite drops the set one first, so spinnaker and gennaker are never set together.
- **Heel.** Heel grows with apparent wind speed upwind, scaled by trim efficiency and capped at 24°. It leans the rig to leeward in the renderer.

## Renderer

The race view is a small fixed-point 3D pipeline:

1. **Camera space.** The chase camera sits 30 m behind and 6.5 m above the yacht, smoothed towards its heading. World points are rotated and translated into decimetre camera space (x right, h up, z forward).
2. **Clipping.** Polygons and lines are clipped in camera space against a near plane and two side planes (chase), or four orthographic planes (top view), with Sutherland–Hodgman and an overflow-safe interpolation. Projected coordinates are therefore always bounded.
3. **Projection.** Chase: 150 px/rad perspective with the horizon at row 76. Top view: 1.5 px/m, heading-up, with a slight height tilt so masts and sails keep their shape.
4. **Rasterisation.** Even-odd scanline fill emitted as one-row `prg32_gfx_rect_indexed` spans, so concave sail outlines work; each edge's 16.16 slope is computed once, so a scanline costs no divisions. Lines use a 16.16 DDA with `prg32_gfx_pixel_indexed`.
5. **Ordering.** Painter's order by camera depth for yachts, buoys and committee boats. Within a yacht, the farther of mainsail and headsail is drawn first.

Scene layers:
- **Sky.** Three sky bands.
- **Coastal hills.** Procedural hills on bearings 61–128° from dead downwind.
- **Panorama.** The venue panorama spans ±61° about dead downwind at 150 px/rad, so it blits 1:1 from the row-compressed RLE with only a horizontal offset.
- **Sea.** A dithered depth gradient, and world-anchored ripples that drift downwind.
- **Shore.** Walls in tufa, city and green along the three land edges.
- **Lines.** Start line, box and gates, or the finish line.
- **Objects.** Buoys, committee boats with their flag hoists, and the yachts.

Yachts are modelled in decimetres:
- **Hull.** A seven-point waterline in deep red, a team-coloured sheer and a pale deck inset.
- **Rig.** A 25 m mast and boom.
- **Mainsail.** A five-point outline whose belly moves to leeward and flutters when luffing.
- **Jib.** Lowered as a kite goes up.
- **Spinnaker.** Eight points on a windward pole.
- **Gennaker.** Eight points, tacked at the bow.
- **Wake.** A foam trail that lengthens with speed.

Kites grow from the bow with hoist progress.

All race colours are exact levels of the ILI9341 driver's 6×6×6 palette cube (`C6(r,g,b)`), so the indexed hardware display and the RGB565 QEMU display match. All drawing goes through the palette-indexed firmware calls with an index computed once per call (`ci()`, the firmware's own conversion rule). See `docs/PERFORMANCE.md` for why this matters. The HUD overlays sit above the clipped world region (rows 18–179):
- a wind instrument
- a course-up map
- start-signal messages
- a bottom bar with speed, TWA, sail state, next mark, trim gauge and status

## AI

AI yachts use the same `update_rig`/`sail` physics as the player, setting rudder, sheet and kite requests directly:

- **Pre-start.** Hold outside the line, return above it if they cross outside their gate, enter through the gate, then kill time deep in the box. A time-distance rule decides when to leave: distance divided by 90% of the close-hauled polar, plus an acceleration allowance. On the way in they meter speed by easing sheets towards the luffing point and hold short of the line until the gun.
- **Racing.** Aim 18 m to the right of each mark so it is left to port. Direct legs are sailed straight. Beats (42°) and runs (158°) are sailed at best VMG, changing tack at the team's layline (5–11° overstand), on a 5° header, when the other tack clearly gains, or before an edge of the field. Choose kites by leg angle (≥118° spinnaker, 68–118° gennaker); drop before a mark whose next leg needs none; hoist only once the yacht has borne away.
- **Traffic.** Predict the closest approach against every rival. Give way as Rules 10–12 require: bear away 30° to duck, or head up 20° when windward. Always avoid imminent contact (Rule 14). Sail into clear water before a penalty turn.

## Rules

The RRS engine activates at the preparatory signal:
- **Contact.** Five sample points along each hull detect contact within 4 m. Contact bounces the yachts apart and slows any that are still moving.
- **Rule 31.** Touching a buoy within 2.5 m is a penalty, counted once per touch.
- **Contact ordering.** Rules are checked in a fixed order: Rule 13 tacking, then Rule 18 mark-room inside the 63 m zone, Rule 10 port/starboard, Rule 11 windward, and Rule 12 clear astern.
- **Penalty turns.** A penalty is cleared by 360° of accumulated heading change with the helm hard over towards the wind. The player triggers it with A+B.

## Courses and finishing

Course navigation is table driven in metres. The windward/run, Olympic triangle and 1992 IACC Z layouts contain three, five and six ordered roundings respectively, fitting the three-bit multiplayer leg field. A mark counts when the yacht passes within 40 m of it. After the final mark, the only valid completion is a downwind crossing of the finish line.

## Multiplayer

Multiplayer follows the firmware snapshot API: `init`, `available`, `join`, `tick`, `set_local_state`, `set_input`, `get_peer_count`, `get_peer`, and `leave`. The firmware relays `x`, `y`, `sprite` and `flags` as 16-bit fields but keeps only 7 bits of `input`. The v8 snapshot therefore packs:
- `x`, `y`: position in decimetres
- `sprite`: 9-bit heading and 7-bit sheet
- `flags`: leg, team, spinnaker, started, finished, racing (0x100), gennaker, 3-bit hoist progress, and penalty
- `input`: speed in 0.2 kt steps

Lobby snapshots lack the racing bit, which both the ready handshake and the snapshot filter rely on. Peers keep their fleet slot by player id, remote penalties belong to their own console, and AI takes over a disconnected slot. Course-specific `fairwind-napoli97:v8-*` rooms keep incompatible layouts and protocol versions apart.

## Package

The package targets the portable 64 KiB cartridge profile used by PRG32 main. The builder receives both `--portable` and `--multiplayer`; post-build checks reject any architecture variant over 65,536 bytes. `tools/prg32_cli_64.py` temporarily corrects the Python builder/uploader's stale 32 KiB fallback while preserving the normal CLI.

Procedural yachts replaced the 16 KB of pre-rotated yacht sprite banks. The 4.1 engine, including the 4.6 KB title-screen logo sprite, fits in about 50 KB of code and data, and each store variant stays under the 64 KiB package limit including icon, real-frame screenshot and metadata.

Five authored 320×48 panorama sources share a Mediterranean palette. Runtime versions retain 8-bit palette indices and are resized to 320×24, then compressed as row-safe `(run length, palette index)` byte pairs and expanded directly as horizontal runs.

Audio declares eight instruments with explicit left/right pan pairs and five tracks. Tracks 3 and 4 provide stereo short and long horn signals before returning to the race score. The packed block is generated by the firmware's `tools/prg32audio_pack.py`.
