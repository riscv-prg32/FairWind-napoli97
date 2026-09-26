# Release report: FairWind-napoli97 4.1.0

Status of every item of `RELEASE_CHECKLIST.md` for this release, with the evidence and the tool that produced it. **Verified** means checked by an automated test, a real build, or a run of the real PRG32 firmware under QEMU. **Needs hardware** marks the items that can only be closed on a physical PRG32 board or with more than one board. No ESP32-C6 board was connected while this release was prepared.

| # | Checklist item | Status | Evidence |
|---|---|---|---|
| 1 | Run `make test` | Verified | Source checks, `-Werror` host compile, and the ASan/UBSan behavioural harness all pass. |
| 2 | `./build.sh` with `PRG32_ROOT` and `CARTRIDGE_STORE_ROOT`; Store intake passes | Verified | "store accepts esp32c6/qemu", both 4.1.0 (see §Build). |
| 3 | Play all five races on QEMU and ESP32-C6 | QEMU verified; needs hardware | QEMU: the autopiloted profiling cartridge (the release `game.c` plus timers) played complete races inside the PRG32 firmware with full drawing and audio: two consecutive races in one 25-minute real-time run (finishing times 1108–1186 s and 1252–1430 s, results screen and next race reached), plus one complete race under instruction counting. No faults. The harness plays full five-race seasons, plus fuzzed seasons, on every build. |
| 4 | Stereo separation on physical speakers | Needs hardware | Audio block unchanged since 3.1.0 (8 panned voices, 5 tracks). |
| 5 | Each cartridge below 64 KiB | Verified | `FairWind-napoli97-esp32c6.prg32` 61,418 bytes and `-qemu.prg32` 61,416 bytes, against a limit of 65,536; the code segment is 51,204 bytes. |
| 6 | Two-, three-, four-board races through the MultiplayerServer | Needs hardware | Harness: lobby handshake, stable peer slots, disconnect and AI takeover, AI team assignment. Protocol v8, unchanged since 4.0.0. |
| 7 | Every Bay of Naples skyline on ILI9341 hardware | Host-verified; needs hardware | `release-artifacts/FairWind-napoli97-venues.png` shows all five venues rendered through the firmware's exact 6×6×6 palette. |
| 8 | Procedural hull, sails and kites in all team colours, stern and top views | Host-verified; needs hardware | Venue sheet, store screenshots 05–08, gameplay montage video. |
| 9 | Per-frame instruction count up to date | Verified | `docs/PERFORMANCE.md` §3.1: cartridge ≈ 0.31 M instructions per average race frame, 0.60 M at the worst frame (QEMU `-icount`, null graphics). |
| 10 | Frame rate on ESP32-C6 hardware | Estimated; needs hardware | Model: ≈ 30 fps, SPI-bound (`docs/PERFORMANCE.md` §3.2). The harness draw budget and frame-rate independence pass. Confirm with the metrics firmware (§6). |
| 11 | Polar targets at 8, 12, 16 knots | Verified | Harness: beam reach 7.5 / 8.7 / 9.2 kt against the ORC 7.6 / 8.8 / 9.3; close-hauled 7.2 kt in 12 kt. |
| 12 | Start signals, 20×→4× change, box entry, early return, line removal | Verified | Harness AI races (every AI yacht enters its gate and starts on all three courses); The start and 5-minute horn tracks were observed in the QEMU console; the signal schedule itself is unchanged since 3.x. |
| 13 | Sheet trim, A spinnaker, B gennaker, A+B penalty | Verified | Harness trim, kite (timed hoist, peel, never both), and penalty-combo scenarios. |
| 14 | Top view at 70 m / back at 100 m; map, gauge, lift/header | Verified | Harness view scenario; frames in the store set and the venue sheet. |
| 15 | Shores and SW limit stop the yacht | Verified | Harness land scenario (`AGROUND`, speed < 0.5 kt). |
| 16 | All three courses; `NEXT` transitions | Verified | Harness AI races on all three courses; host frames show the `NEXT` highlight moving mark by mark. |
| 17 | Early finish crossing ignored; valid finish | Verified | Harness finish scenario. |
| 18 | Rules 10, 11, 12, 13, 18, 31; Rule 14 separation | Verified | Harness rules scenario covers all seven. |
| 19 | Main and jib to leeward, swinging in tacks and gybes, legible over the panoramas | Verified (host) | Harness trim scenario (boom side for wind from port and starboard); venue sheet. |
| 20 | SW course-up map, constrained shifts, panorama dead downwind | Verified | Harness wind scenario (shifts −14…+9°, deterministic); venue sheet (downwind frames show the panoramas). |
| 21 | Preview video re-recorded | Done | `release-artifacts/FairWind-napoli97-preview-60s.mp4`, a gameplay montage of real game frames at 30 fps with a soundtrack captured from the game in QEMU. |
| 22 | Logo on the title screen and as the store icon | Verified | Store screenshot 01, `dist/store/icon.png`. |
| 23 | Tutorial matches the code | Verified | `docs/tutorial/` written against this release's `src/game.c`. |
| 24 | Names consistent (`FairWind-napoli97` / `fairwind`) | Verified | Source checks on metadata, package id, rooms, entry prefix. |
| 25 | Publish the store bundle and checksums | Done | `dist/FairWind-napoli97-4.1.0-store.zip`, `dist/SHA256SUMS` (see §Build). |

## Build

- `code=51204 mem=51800 audio=336`
- `store accepts esp32c6: org.riscv-prg32.fairwind-napoli97 4.1.0 rebuilt=61432 bytes`
- `store accepts qemu: org.riscv-prg32.fairwind-napoli97 4.1.0 rebuilt=61430 bytes`
- Bundle `dist/FairWind-napoli97-4.1.0-store.zip`, 85,213 bytes, SHA-256 `21c77224d140da1f4ff84aa51ebdf520abffdeae18c7eedfdbc38fb76a897ab2`

The bundle contains the manifest, the logo icon (128×128), the real-frame screenshot and both cartridges.

## Still to do on hardware

1. **Frame rate.** Flash the metrics firmware and measure the race frame rate (item 10), expecting about 28–30 fps.
2. **Sound and screen.** Listen to the stereo separation (item 4), and look at the skylines and yachts on the ILI9341 panel (items 7 and 8).
3. **Multiplayer.** Race two, three and four boards through the MultiplayerServer (item 6).
