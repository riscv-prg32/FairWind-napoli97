# NaCup-napoli97

An original PRG32 sports-management cartridge set around the **fictional 12-Metre America's Cup held in Naples in 1997**.

The player is both syndicate manager and helmsman. Between races, a compact team-HQ interface inspired by the decision rhythm of modern motorsport-management games handles sponsors, cash flow, technical development, crew, and strategy. On the water, the player helms the yacht against AI rivals or up to three remote players through PRG32 multiplayer.

## Campaign

- Choose one of four fictional syndicates.
- Sign sponsors and collect their race retainers.
- Earn prize money according to finishing position.
- Wins unlock richer sponsors and add contractual win bonuses.
- Invest cash in hull efficiency, sails, crew responsiveness, and race strategy.
- Contest five races and win the final championship.

The five venues are based on authored late-1990s console-style pixel panoramas of Santa Lucia and Castel dell'Ovo, Vesuvius, Capri and the Faraglioni, the Sorrento cliffs, and the Naples waterfront/Castel Nuovo. The source and release artwork retains recognizable landmark silhouettes and warm Mediterranean colours; the cartridge renders compact procedural counterparts within its execution-RAM budget. The fleet's visual proportions and deck treatment take **Il Moro di Venezia V (ITA-25), the 1992 America's Cup challenger**, as their period reference: slender deep-red IACC hulls, pale decks and sails, and dark rigs. The four syndicates remain fictional and are distinguished by their trim and spinnaker colours; no real sponsor marks or exact livery are reproduced.

## Multiplayer

Select **Network Multiplayer** on the mode screen. The cartridge joins a PRG32 v7 room dedicated to the selected course through the resident firmware's Wi-Fi/WebSocket snapshot service. Each console owns its local yacht and publishes position, heading, input, selected team, course leg, start-line crossing, readiness, and finish state. Up to three remote peers occupy the remaining fleet slots; AI controls only unoccupied or disconnected slots.

The lobby supports two, three, or four human players. Press A to become ready; the race starts when every visible player is ready. Press B to leave the room and race against AI instead. During a race the HUD shows the currently connected fleet count as `NET n/4`. If a peer disconnects, AI takes over the vacated slot.

Requirements:

1. Configure Wi-Fi station mode and `PRG32_MULTIPLAYER_SERVER_URL` in the firmware.
2. Run the official PRG32 MultiplayerServer on the same reachable network.
3. Install the same cartridge revision on all participating consoles.
4. Prefer a different syndicate for each human player so every yacht has a distinct colour and grid position.

## Controls

| Screen | Control | Action |
|---|---|---|
| Menus | D-pad | Navigate or select team/mode |
| Menus | A | Confirm, sign, invest, or continue |
| Team HQ | B | Sell one development level |
| Race | Left/Right | Helm port/starboard |
| Race | A | Hoist or lower the spinnaker |
| Race | Hold B | Complete the required penalty turn |

## Starting procedure and simulated time

Every race opens with a playable ten-minute pre-start compressed to about 30 seconds at 60 updates per second. The committee boat and pin buoy define the line while the HUD counts simulated time. A short stereo horn accompanies the warning/class signal and the International Code P preparatory signal. At −2, the fleet must enter the displayed pre-start box through its assigned port or starboard gate. P is removed with a short horn at −1; the class flag is removed with a longer horn at the start.

The yachts begin on the course side and may not enter early. In multiplayer fleets, entry assignments alternate port and starboard. A valid start requires the yacht to have entered the box from its assigned side, be wholly on the pre-start side at or after zero, and then cross toward the course. An early yacht must return before making a valid crossing. The complete starting-line assembly is removed only after the last yacht crosses. The race clock then uses the same accelerated simulated-time scale, giving a 30-minute simulated limit in roughly 90 seconds of play.

## Race paths and finishing

The player selects a path on every race briefing screen with Left/Right before pressing A:

- **Windward / Run:** windward buoy, leeward buoy, second windward rounding, then a run to the finish.
- **Olympic Triangle:** windward, wing and leeward marks followed by the traditional additional windward/leeward section.
- **1992 IACC Z:** a compact six-mark zig-zag with beats, broad reaches and a final run, inspired by the reaching/running “Z” course developed for the first IACC generation in 1992.

The next required buoy receives a flashing gold `NEXT` frame. After the last rounding, the highlight moves to the finish buoy and line. The finish is a separate line extending from that buoy to the committee boat; crossing it does not count until the yacht has completed every mark in order. The 1992 inspiration is documented by the [America's Cup historical account](https://www.americascup.com/history/68_THE-CUP-COMES-ROARING-BACK).

The race camera is nearly orthographic and course-aligned rather than north-up. Upscreen represents the south-westerly/upwind end of the field, matching the common daytime summer sea-breeze regime in the Gulf of Naples. Wind stays close to SW with small oscillations instead of wandering through arbitrary directions. The elevated landmark strip is scenic orientation, while yacht and mark scale remains constant across the playable water.

## Basic Racing Rules of Sailing

The game enforces a compact deterministic subset of the [World Sailing Racing Rules of Sailing 2025–2028](https://www.sailing.org/racingrules), beginning at the preparatory signal:

- Rule 10: port-tack yacht keeps clear of starboard tack.
- Rule 11: an overlapped windward yacht keeps clear of the leeward yacht.
- Rule 12: a yacht clear astern keeps clear of the yacht ahead.
- Rule 13: a yacht that is tacking keeps clear until established on its new tack.
- Rule 14: contact is slowed and separated immediately.
- Rule 18: the nearer/inside yacht receives mark-room inside the three-length zone.
- Rule 31: touching a course mark incurs a penalty.

The HUD identifies the infringement. Hold B to perform a complete low-speed 360-degree penalty turn; unresolved penalties add three simulated minutes at the finish. This is intentionally a playable rules subset, not a protest-hearing simulator.

## Technical profile

- PRG32 portable ABI with multiplayer feature declaration
- Executable footprint below the firmware's **65,536-byte (64 KiB)** cartridge RAM ceiling
- Hard maximum package size: **65,536 bytes (64 KiB)** per architecture variant
- Normal 320×200 cartridge viewport with firmware-owned status bands
- Runtime-composed hull, mainsail, jib, and spinnaker sprites
- Compact four-plane hulls with distinct two-plane mainsail, jib, and spinnaker layers
- A distinct cyan, red, purple, or yellow spinnaker for each team
- Table-driven 12‑Metre polar performance at 8–16 knots true wind
- Accelerated simulated race time with a complete 10/5–4–2–1–start match-racing sequence
- Three selectable paths with active-buoy highlighting and a committee-boat finish line
- Deterministic enforcement of basic RRS 10–14, 18, and 31 with penalty turns
- Il Moro di Venezia V-inspired four-plane yachts with slender deep-red topsides, pale deck insets, dark rigs, hull waterlines, cockpit crew, panelled sails, and radial team spinnakers
- Five authored late-1990s-style panoramas reduced to compact one-plane runtime skyline silhouettes
- Eight-voice stereo tracker score
- Allocation-free update/draw loop suitable for the physical ESP32-C6 profile
- Store metadata and ESP32-C6/QEMU variants

## Build and test

```sh
python3 -m pip install -r requirements-dev.txt
make test
PRG32_ROOT=/path/to/current/PRG32 ./build.sh
```

The build passes both `--portable` and `--multiplayer`. It rejects either generated `.prg32` file if it exceeds 131,072 bytes and produces `dist/NaCup-napoli97-3.0.0-store.zip`.

## Store media

- [60-second audiovisual preview](release-artifacts/NaCup-napoli97-preview-60s.mp4)
- [Store screenshot set](release-artifacts/store-screenshots/)

The preview audio and menu frames come from the deterministic QEMU capture workflow in `tools/capture_qemu_demo.py`; authored 320×200 gameplay renders complete the store-safe edit.

## Fiction notice

The Naples 1997 event, teams, sponsors, results, and characters are fictional. Il Moro di Venezia V is used only as a historical visual reference. This project is not affiliated with any real America's Cup organization or syndicate.
