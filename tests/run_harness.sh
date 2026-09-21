#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC_BIN="${CC:-cc}"
BIN="$(mktemp -t nacup-harness)"
trap 'rm -f "$BIN"' EXIT
SRC="$ROOT/tests/harness/run_harness.c"
COMMON=(-std=c11 -Wall -Wextra -O1 -I"$ROOT/tests/stub" -I"$ROOT/src")
if "$CC_BIN" "${COMMON[@]}" -fsanitize=address,undefined "$SRC" -o "$BIN" 2>/dev/null; then
    :
else
    "$CC_BIN" "${COMMON[@]}" "$SRC" -o "$BIN"
fi
"$BIN"
echo "behavioural harness: OK"
