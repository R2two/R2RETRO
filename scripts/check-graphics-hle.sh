#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="$ROOT/build/core-lab/source"
BUILD="$ROOT/build/graphics-hle-test"
mkdir -p "$BUILD"
FLAGS=(-std=gnu11 -O1 -g -fsigned-char -fno-strict-aliasing -DNO_ASM
       -DARCH_MIN_SSE2 -msse2 -D__LIBRETRO__ -DM64P_PLUGIN_API -DM64P_CORE_PROTOTYPES
       -I"$SOURCE/mupen64plus-core/src" -I"$SOURCE/mupen64plus-core/src/api"
       -I"$SOURCE/libretro-common/include" -I"$SOURCE/mupen64plus-core/subprojects/md5"
       -I"$SOURCE/mupen64plus-rsp-hle/src" -I"$SOURCE/libretro")
FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all)
SOURCES=()
for name in alist alist_audio alist_naudio alist_nead audio cicx105 hle hvqm jpeg memory mp3 musyx re2 plugin; do
    SOURCES+=("$SOURCE/mupen64plus-rsp-hle/src/$name.c")
done
cc "${FLAGS[@]}" "$ROOT/tests/graphics_hle_tests.c" "${SOURCES[@]}" \
    "$SOURCE/mupen64plus-rsp-cxd4/rsp.c" "$SOURCE/libretro/r2n64_profile.c" \
    -lm -o "$BUILD/graphics-hle-tests"
"$BUILD/graphics-hle-tests"
