# Changelog

## 5.0.0 — 2026-11-11 — 128 KiB profile, indexed colour, sound and playability

### Platform

- Moved to PRG32 1.8's portable **128 KiB profile** (`--cart-ram-kib 128`), built separately for `esp32c6` and `qemu`: about 74 KB of code and data and about 99 KB per package (limit 131,072 bytes). Removed the 64 KiB builder adapter.
- The image is position independent: static pointer tables are gone (they crashed QEMU, which loads cartridges at a different address) and `tools/check_relocatable.py` links the image at two addresses and requires identical bytes.
- Uses the latest firmware features: indexed drawing and `prg32_palette_set`, `prg32_audio_note_on_pan` with live channel volume and pan, `prg32_multiplayer_local_player_id`, the hall-of-fame score API, and themed status bands.

### Graphics

- **Indexed colour.** A fixed palette for yachts, sails, shore, HUD and menus, plus a per-race palette — sky, sea, haze, clouds, sun, glitter and the panorama — graded to that race's time of day: morning, late morning, midday, afternoon and sunset.
- New horizon: graded sky with world-anchored drifting clouds, the sun and its halo at their true bearing, the far coast of Ischia, Procida, Posillipo, Monte Faito and Capri, a perspective-graded sea and a sun-glitter path. The glitter and ripples sparkle by palette cycling.
- Re-authored panoramas: only land connected to the waterline is kept, so they sit cleanly on the new sky.
- Yachts: sun-lit and shaded hull sides with back-face culling, team-coloured sheer line, cockpit, two-tone sails by the side in view, team kites with light/dark faces, battens, wakes and bow waves. Improved buoys and committee boat.
- New title screen with a pixel logo over a live 3D Bay scene and a sailing yacht; syndicate screen with the chosen yacht sailing in a window onto the Bay; briefing with the venue, its time of day and a course map; bevelled panels and gradient menu backdrops; results with team swatches and podium bars; season finale with the trophy and the hall of fame.

### Sound

- Twenty synth instruments and six songs: title (6/8, A minor), team HQ, lobby, podium fanfare, result cadence and season finale.
- Race audio driven live by the game: wind in the rig and water along the hull (level and pan follow apparent wind and speed), surf, winch clicks, luffing, gybe slams, contact, halyard whoosh, start horns and gun, mark chimes and a penalty buzzer.

### Controls and playability

- Only the D-pad, A and B. **Tap A** spinnaker, **tap B** gennaker, **hold A** penalty turn, **hold B** race menu (resume, auto trim, retire, restart; single-player races pause). The A+B chord now does nothing (the firmware reserves A+B+Down).
- **Auto trim** (default on): the crew trims to the best angle, and yields to Up/Down for 1.5 s.
- Guidance panel after the gun: arrow to the next mark, distance and live position. Toasts for signals, roundings, penalties and the finish; HUD hints for kite hoists and drops; name tags over rival yachts.
- B goes back on the mode, syndicate and briefing screens; the mode screen holds single player, network multiplayer and auto trim.

### Multiplayer (protocol v9)

- Rooms `fairwind-napoli97:v9-*`; the snapshot adds retired and old-race flags.
- Lobby lists players with P1–P4 tags, syndicate colours and states. Duplicate syndicates are resolved by player id (points still go to the campaign syndicate). Late joiners wait for a race in progress.
- Only racing consoles bind fleet slots (a lobby snapshot no longer freezes a slot); remote yachts are dead-reckoned and smoothed between snapshots.

### Tooling and release

- `scripts/qemu_preview.py` records the Store preview from QEMU (headless, stereo, frames on the audio clock) and cuts it to 60 s; `make preview`.
- `build.sh` builds both architectures, attaches metadata, and packs the Store bundle with the icon, splash and `preview.mp4`.
- Host renderer emulates the palettes; new harness scenarios cover hold/tap separation, the race menu, auto trim, lobby arbitration, late joiners and off-screen drawing.

## 4.0.0 — 2026-09-23 — FairWind: stern view and sailing physics

### Renamed

- Renamed the game and cartridge from `NaCup-napoli97` to `FairWind-napoli97`, with package id `org.riscv-prg32.fairwind-napoli97`.
- Renamed the `fairwind_` entry points, asset symbols, build products, store bundle, title, information band, lobby and release artifacts.

### Graphics engine

- Replaced the top-down sprite view with a fixed-point perspective renderer: a stern chase camera, camera-space clipping, polygon and line rasterisation, and painter's ordering.
- Procedural 12-Metre yachts: deep-red hull, team sheer, deck, mast and boom. The mainsail and jib follow the wind, the sheet trim and the heading: the boom swings to leeward, walks over in tacks, slams in gybes, and the sails flutter when luffing. The hull heels.
- Added a spinnaker and a gennaker in each team's colour, with animated hoists and drops.
- The race field (1.8 × 1.7 km) is larger than the view, with a course-up map at the top right. The map shows shore, marks, lines, fleet, wind and, from Strategy 2, laylines.
- Added a horizon with the venue panorama dead downwind, coastal hills and an open SW horizon.
- Added shore walls for Naples, Sorrento and Posillipo, a dithered sea, drifting ripples and wakes.
- Added a wind instrument showing true and apparent wind, true wind speed, and the shift as a lift or header.
- Automatic heading-up top view within 70 m of a mark or rival (100 m to return), with the three-length zone drawn at the next mark.
- Race colours sit exactly on the ILI9341 6×6×6 palette cube.

### Physics

- Realistic speeds: ORC polar × sail plan (jib, spinnaker, gennaker) × trim efficiency × syndicate, reached through keelboat momentum. Typical speeds are about 7 kt close-hauled and 8–9 kt reaching in 12 kt of breeze.
- Rudder-dependent turn rate and drag, a no-go zone in which the yacht coasts to a stop, and leeway drift.
- Deterministic SW wind with oscillating and persistent shifts and pressure pulses, identical on every console.
- Yachts cannot sail onto the shore or past the race-area limit.
- Simulated time now runs at 20× before the two-minute box entry and 4× afterwards, instead of 20× throughout. The race time limit is 45 simulated minutes.

### Controls

- Left/Right helm to port/starboard.
- Up eases and Down trims the sheets.
- A toggles the spinnaker and B the gennaker; they are mutually exclusive, and changing kite drops one before hoisting the other.
- A+B takes a penalty turn.

### AI

- Time-distance starts from the box, with speed metered by the sheets.
- VMG beats and runs with layline, header and edge tacking.
- Port roundings, crewed kite choice and timing.
- Rule 10–12 give-way and Rule 14 contact avoidance.
- Penalty turns taken in clear water.

### Multiplayer and tooling

- Multiplayer protocol v8 (`fairwind-napoli97:v8-*` rooms), carrying decimetre position, 9-bit heading, sheet, kite state and speed. The lobby is recognised by the absence of the racing flag.
- Removed 16 KB of pre-rotated yacht sprite banks. Each store variant is now about 54 KB, down from 64.7 KB.
- New host renderer (`tools/host_capture.py`, `make screenshots`) produces real-frame store screenshots and traceable autopiloted races.
- New harness scenarios cover:
  - polar speeds and momentum
  - trim
  - helm
  - kites
  - A+B penalty turns
  - grounding
  - view hysteresis
  - wind shifts
  - full AI races on every course

## 3.1.0 — 2026-09-22 — 64 KiB profile refresh

- Migrated the cartridge and package guards to PRG32's 64 KiB limits.
- Restored five 8-bit indexed Bay landscapes with row-safe RLE compression.
- Rebuilt yachts as four-bitplane top-down silhouettes with syndicate-specific hull and sail palettes.
- Added an automatic overhead tactical view within five yacht lengths of rivals or buoys.
- Preserved distinct mainsail, jib, and spinnaker geometry in precomposed heading frames.
- Added five authored one-plane Bay of Naples skyline silhouettes to the live race renderer.
- Added a temporary build/upload adapter for the upstream Python fallback that still reports 32 KiB.
- Fixed a multiplayer lobby deadlock: a peer that has already started racing now counts as ready.
- Bound remote peers to fleet slots by player id, so a disconnect hands only that yacht to the AI.
- AI takeover of a remote yacht that already started now continues round the course.
- Remote yachts' penalties are owned by their console and no longer suppress local rule checks.
- AI yachts take the syndicates no human picked in multiplayer fleets.
- Time-limit DNFs are ranked by course progress instead of fleet slot.
- Store icon and screenshot are saved as lossless compact PNGs, widening the 64 KiB package headroom.
- Added behavioural harness scenarios for each fix; the harness temp file now works with GNU mktemp.
- Store bundle manifest is now generated from `metadata/*.json` with `abi: prg32-metadata-1.0`, the splash screenshot, and an inline colophon; the previous hand-written manifest was rejected by Cartridge Store intake.
- `build.sh` validates the bundle with the Cartridge Store's own intake code when `CARTRIDGE_STORE_ROOT` is set.

## 3.0.0 — 2026-09-18

- Renamed the game and cartridge to `NaCup-napoli97`.
- Changed the package identifier to `org.riscv-prg32.nacup-napoli97`.
- Renamed the PRG32 entry points, generated asset symbols, build products, store bundle, and source archive to the `nacup` namespace.
- Moved multiplayer into course-specific `nacup-napoli97:v7` rooms, preventing cross-version collisions with AC12 releases.
- Updated the title screen, firmware information band, lobby, icon, metadata, documentation, and release assets.

## 2.8.0 — 2026-09-18

- Adopted a near-orthographic, course-aligned race presentation with constant yacht scale.
- Added five authored late-1990s console-style panoramas covering Santa Lucia, Vesuvius, Capri, Sorrento, and Castel Nuovo.
- Reworked the shared 16-colour environment palette for Mediterranean stone, terracotta, vegetation, haze, and layered cobalt water.
- Redesigned the mainsail and jib as distinct fore-and-aft layers on the same leeward side, improving top-down readability and wind logic.
- Added animated short water glints without perspective convergence and a heading-aware team pennant at each stern.
- Aligned the course-up camera with the Gulf's common daytime south-westerly sea breeze and constrained wind shifts around SW.
- Advanced multiplayer rooms to course-specific v7 signatures for the revised wind model.

## 2.7.0 — 2026-09-18

- Reworked the yacht silhouette around Il Moro di Venezia V's long, narrow 1992 IACC proportions.
- Added deep Venetian-red topsides, a brighter sheer stripe, pale inset deck, fine bow, and compact working stern.
- Changed mainsails and jibs to pale cream/white cloth with restrained grey panel seams and battens.
- Preserved fictional syndicate identity through team trim and four uniquely coloured radial spinnakers.
- Added explicit historical-reference and non-affiliation documentation; no real sponsor marks are reproduced.

## 2.6.0 — 2026-09-18

- Added deterministic enforcement of RRS 10–14, 18, and 31 from the preparatory signal.
- Added tack-transition, overlap, ahead/astern, windward, and three-length mark-zone calculations.
- Added rule-specific HUD calls and complete 360-degree penalty turns for players and AI.
- Added immediate hull separation and speed loss after contact.
- Refined hulls with waterlines, deck structure, cockpit crew, winches, rails, wake, and bow fittings.
- Refined mainsails, jibs, and team-coloured spinnakers with panels, seams, battens, and radial shading.
- Advanced multiplayer rooms to course-specific v6 signatures.

## 2.5.0 — 2026-09-18

- Added player selection among windward/run, Olympic triangle, and 1992 IACC Z paths.
- Replaced hard-coded leg logic with shared data-driven course tables for players and AI.
- Added a flashing gold highlight around the next required buoy.
- Added a real finish line between the final buoy and a blue-flag committee boat.
- Require every ordered rounding before a finish-line crossing is accepted.
- Split multiplayer v5 rooms by course to keep every client synchronized.

## 2.4.0 — 2026-09-18

- Added a playable, accelerated ten-minute starting operation before every race.
- Added the committee boat, pin buoy, temporary starting line, and race-signal display.
- Implemented class/warning, International Code P, two-minute entry, one-minute, and start signals.
- Added alternating port/starboard gates and mandatory entry into the pre-start box.
- Added short stereo warning horns and a longer start horn.
- Added course-side detection: early yachts must return before starting correctly.
- Synchronized each yacht's valid start state in four-player multiplayer.
- Remove the complete line assembly only after the final yacht crosses.
- Converted countdown, race limit, wind changes, penalties, and results to simulated seconds.

## 2.3.0 — 2026-09-18

- Split every yacht into hull, mainsail, jib, and spinnaker sprite layers.
- Added port/starboard sail-frame selection from signed true-wind angle.
- Added A-button spinnaker toggling and moved penalty service to B.
- Added four team-coloured spinnaker palettes without duplicating pixels.
- Replaced the heuristic speed curve with an interpolated 12‑Metre ORC polar table.
- Added realistic performance losses for missing or incorrectly carried spinnakers.

## 2.2.0 — 2026-09-18

- Upgraded yachts from 24×24 sprites to detailed 32×32 four-plane sprites that fit the portable execution-RAM profile.
- Added a dedicated 256-entry RGB565 yacht palette.
- Added detailed sails, hull shading, deck, cockpit, rigging, fittings, and wake.
- Retained four-plane panoramas to remain inside the portable cartridge budget.

## 2.1.0 — 2026-09-18

- Expanded PRG32 multiplayer from two to four human players.
- Added distributed lobby readiness and automatic synchronized start.
- Added team identity and remote finish-state packing to snapshots.
- Added AI takeover for empty or disconnected peer slots.
- Added live `NET n/4` status to the race HUD.

## 2.0.0 — 2026-09-17

- Renamed the cartridge to `AC12-napoli97`.
- Added PRG32 Wi-Fi/WebSocket snapshot multiplayer and an AI fallback.
- Reworked team management around sponsors, income, prizes, and development spending.
- Added sponsor progression driven by race wins.
- Added five venue-specific Bay of Naples landscapes.
- Enforced the portable execution-RAM and packaged-cartridge limits current at release time.
- Replaced schematic skyline primitives with five detailed planar Bay of Naples panoramas.

## 1.0.0 — 2026-09-17

- Initial campaign with four fictional syndicates and five races.
- Added manager/helmsman roles, variable-wind sailing, AI boats, scoring, and penalties.
- Added deterministic 4-bitplane graphics and eight-voice stereo music.
- Added PRG32 Store metadata and ESP32-C6/QEMU packaging workflow.
