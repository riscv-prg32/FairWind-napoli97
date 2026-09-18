#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="${PRG32_ROOT:-}"
if [[ -z "$ROOT" || ! -d "$ROOT/prg32" ]]; then echo "Set PRG32_ROOT to a current PRG32 checkout" >&2; exit 2; fi
python3 "$HERE/tools/generate_assets.py"
python3 "$HERE/tests/source_checks.py"; bash "$HERE/tests/host_syntax.sh"
BUILD="$HERE/build"; DIST="$HERE/dist"; STORE="$DIST/store"; rm -rf "$BUILD" "$STORE"; mkdir -p "$BUILD" "$STORE"
cd "$ROOT"
python3 tools/prg32audio_pack.py "$HERE/audio.json" --out "$BUILD/nacup-audio.block"
python3 "$HERE/tools/prg32_cli_64.py" cartridge build "$HERE/src/game.c" --portable --multiplayer --entry-prefix nacup --name NaCup-napoli97 --audio-block "$BUILD/nacup-audio.block" --out "$BUILD/nacup-base.prg32"
for arch in esp32c6 qemu; do python3 -m prg32 store attach-metadata "$BUILD/nacup-base.prg32" --metadata "$HERE/metadata/metadata.json" --icon "$HERE/assets/generated/icon.png" --screenshot "$HERE/assets/generated/screenshot.png" --colophon "$HERE/metadata/colophon.json" --architecture "$arch" --out "$STORE/NaCup-napoli97-$arch.prg32"; done
cp "$HERE/metadata/manifest.json" "$HERE/assets/generated/icon.png" "$HERE/assets/generated/screenshot.png" "$HERE/metadata/colophon.json" "$STORE/"
python3 -m prg32 store pack-bundle --manifest "$STORE/manifest.json" --out "$DIST/NaCup-napoli97-3.0.0-store.zip"
for f in "$STORE"/*.prg32; do test "$(wc -c < "$f")" -le 65536 || { echo "$f exceeds 64 KiB" >&2; exit 3; }; done
(cd "$DIST" && sha256sum store/*.prg32 NaCup-napoli97-3.0.0-store.zip > SHA256SUMS)
echo "Built portable 64 KiB profile: $DIST/NaCup-napoli97-3.0.0-store.zip"
