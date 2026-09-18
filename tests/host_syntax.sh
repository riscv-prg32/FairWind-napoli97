#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC_BIN="${CC:-cc}"
"$CC_BIN" -std=c11 -Wall -Wextra -Werror -fsyntax-only -I"$ROOT/tests/stub" -I"$ROOT/src" "$ROOT/src/game.c"
echo "host syntax: OK"
