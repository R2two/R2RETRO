#!/usr/bin/env bash
# Isolated deterministic profiler unit test; never loads any ROM.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="${1:-$ROOT/build/core-lab/source}"
OUT="$ROOT/build/profile-unit"
mkdir -p "$OUT"
cmp "$ROOT/include/core_profile.h" "$SOURCE/libretro/core_profile.h"
cc -std=c99 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  -I"$SOURCE/libretro" -I"$ROOT/external/mupen64plus-next/libretro-common/include" \
  "$SOURCE/libretro/r2n64_profile.c" "$ROOT/tests/core_profile_unit.c" \
  -o "$OUT/core_profile_unit"
"$OUT/core_profile_unit"
