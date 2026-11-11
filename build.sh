#!/usr/bin/env bash
# Build FairWind-napoli97 for the PRG32 portable 128 KiB profile and pack the
# Cartridge Store bundle.
#
#   ./build.sh                 # esp32c6 + qemu cartridges, Store bundle
#
# Environment:
#   PRG32_ROOT            PRG32 checkout (default ../PRG32)
#   CARTRIDGE_STORE_ROOT  CartridgeStore checkout; when set, the bundle is
#                         validated with the Store's own intake code
#
# Outputs dist/store/FairWind-napoli97-<arch>.prg32, the bundle
# dist/FairWind-napoli97-<version>-store.zip (cartridges, icon, splash and the
# 60-second QEMU preview.mp4) and dist/SHA256SUMS. Publishing is a separate,
# authenticated, human step: this script never contacts a Store.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${PRG32_ROOT:-$HERE/../PRG32}" && pwd)"
[[ -d "$ROOT/prg32" ]] || { echo "Set PRG32_ROOT to a current PRG32 checkout" >&2; exit 2; }
CART_RAM_KIB=128          # PRG32's large profile, the default on ESP32-C6 and QEMU
PACKAGE_LIMIT=131072      # one 128 KiB cartridge slot: code + audio + Store trailer
if ! command -v riscv32-esp-elf-gcc >/dev/null 2>&1; then
  toolchain="$(ls -d "$HOME"/.espressif/tools/riscv32-esp-elf/*/riscv32-esp-elf/bin 2>/dev/null | tail -1 || true)"
  [[ -n "$toolchain" ]] && export PATH="$toolchain:$PATH"
fi
command -v riscv32-esp-elf-gcc >/dev/null 2>&1 || { echo "riscv32-esp-elf-gcc not found" >&2; exit 2; }

# 1. Deterministic art and music, then the host tests.
python3 "$HERE/tools/generate_assets.py"
python3 "$HERE/tools/build_audio.py"
python3 "$HERE/tests/source_checks.py"; bash "$HERE/tests/host_syntax.sh"; bash "$HERE/tests/run_harness.sh"

VERSION="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["version"])' "$HERE/metadata/metadata.json")"
BUNDLE="FairWind-napoli97-$VERSION-store.zip"
BUILD="$HERE/build"; DIST="$HERE/dist"; STORE="$DIST/store"
PREVIEW="$HERE/release-artifacts/FairWind-napoli97-preview-60s.mp4"
[[ -f "$PREVIEW" ]] || { echo "missing $PREVIEW (scripts/qemu_preview.py)" >&2; exit 1; }
rm -rf "$BUILD/cart" "$STORE"; mkdir -p "$BUILD/cart" "$STORE"
prg32() { (cd "$ROOT" && python3 -m prg32 "$@"); }

# 2. AUDIO block: instruments and the six songs.
python3 "$ROOT/tools/prg32audio_pack.py" "$HERE/audio.json" --out "$BUILD/cart/fairwind-audio.block" >/dev/null

# 3. Position independence: no absolute relocations, so every host can load
#    the image anywhere (QEMU and the ESP32-C6 use different addresses).
PRG32_REPO="$ROOT" python3 "$HERE/tools/check_relocatable.py" | tail -1

# 4. One portable image per architecture, with the Store trailer attached.
for arch in esp32c6 qemu; do
  raw="$BUILD/cart/fairwind-$arch.raw.prg32"; out="$STORE/FairWind-napoli97-$arch.prg32"; log="$BUILD/cart/fairwind-$arch.log"
  prg32 cartridge build "$HERE/src/game.c" --portable --multiplayer --entry-prefix fairwind --name FairWind-napoli97 \
    --architecture "$arch" --cart-ram-kib "$CART_RAM_KIB" --audio-block "$BUILD/cart/fairwind-audio.block" \
    --build-dir "$BUILD/cart/$arch" --out "$raw" > "$log" 2>&1 || { cat "$log" >&2; exit 1; }
  mem="$(grep -o 'mem=[0-9]*' "$log" | tail -1 | cut -d= -f2)"
  prg32 store attach-metadata "$raw" --metadata "$HERE/metadata/metadata.json" --icon "$HERE/assets/generated/icon.png" \
    --screenshot "$HERE/assets/generated/screenshot.png" --colophon "$HERE/metadata/colophon.json" --architecture "$arch" --out "$out" >/dev/null
  size=$(wc -c < "$out" | tr -d ' ')
  echo "$arch: $(grep -o 'code=[0-9]* mem=[0-9]* audio=[0-9]*' "$log" | tail -1), package $size/$PACKAGE_LIMIT"
  (( size <= PACKAGE_LIMIT )) || { echo "$out exceeds 128 KiB" >&2; exit 3; }
  [[ -z "$mem" ]] || (( mem <= CART_RAM_KIB * 1024 )) || { echo "$arch: executable RAM exceeded" >&2; exit 3; }
done

# 5. Store bundle: manifest from metadata/*.json, icon, splash, preview.
python3 "$HERE/tools/store_manifest.py" "$STORE/manifest.json"
cp "$HERE/assets/generated/icon.png" "$HERE/assets/generated/screenshot.png" "$STORE/"
cp "$PREVIEW" "$STORE/preview.mp4"
rm -f "$DIST/$BUNDLE"
prg32 store pack-bundle --manifest "$STORE/manifest.json" --out "$DIST/$BUNDLE" >/dev/null
if [[ -n "${CARTRIDGE_STORE_ROOT:-}" ]]; then python3 "$HERE/tools/check_store_bundle.py" "$DIST/$BUNDLE" "$CARTRIDGE_STORE_ROOT"; else echo "CARTRIDGE_STORE_ROOT not set: skipping Store intake check" >&2; fi
(cd "$DIST" && shasum -a 256 store/*.prg32 "$BUNDLE" > SHA256SUMS)
echo "Built portable 128 KiB profile: $DIST/$BUNDLE"
