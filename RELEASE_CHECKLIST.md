# Release checklist

- Run `make test`.
- Build with a current `PRG32_ROOT` using `./build.sh`.
- Play all five races on QEMU and ESP32-C6 hardware.
- Confirm left/right stereo separation on physical speakers.
- Confirm each cartridge file stays below the firmware's 64 KiB package limit.
- Run two-, three-, and four-board races through the PRG32 MultiplayerServer.
- Verify every Bay of Naples skyline on physical ILI9341 hardware.
- Inspect composed hull, mainsail, jib, and all four spinnaker colours on hardware.
- Verify polar targets at 8, 12, and 16 knots TWS.
- Verify the 10/5–4–2–1–start signals, port/starboard box entry, an early-return start, and line removal after the fourth crossing.
- Complete all three selectable courses and verify every `NEXT` highlight transition.
- Verify that an early finish crossing is ignored and the valid finish runs between buoy and committee boat.
- Trigger and serve Rules 10, 11, 12, 13, 18, and 31 penalties; verify contact separation under Rule 14.
- Inspect the Il Moro-inspired narrow red hull, pale deck/sails, dark rig, fictional team trim, battens, crew, and all four radial spinnakers on ILI9341 hardware.
- Confirm the main and jib occupy the same leeward side at every heading and remain legible over all five panoramas.
- Confirm the SW course-up convention, constrained wind shifts, flat water glints, and constant yacht scale.
- Confirm title, information band, icon, lobby, metadata, multiplayer rooms, and binaries all use `NaCup-napoli97`/`nacup` consistently.
- Publish `dist/NaCup-napoli97-3.0.0-store.zip` and checksums.
