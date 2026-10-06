#!/usr/bin/env bash
# Isolated hybrid-RSP validation with original synthetic command lists.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="$ROOT/external/mupen64plus-next"
BUILD="$ROOT/build/audio-hle-test"
SOURCE="$BUILD/source"
CC="${R2N64_AUDIO_HLE_CC:-cc}"
mkdir -p "$SOURCE" "$BUILD/objects"
# Only copy the two small RSP sources. Never modify the pinned submodule or the
# active desktop/PS4 build tree; both may be compiling concurrently.
git -C "$UPSTREAM" archive HEAD mupen64plus-rsp-hle mupen64plus-rsp-cxd4 | tar -x -C "$SOURCE"
patch --batch --forward -s -d "$SOURCE" -p1 < "$ROOT/external/patches/0005-audio-hle-fallback.patch"
FLAGS=(-std=gnu11 -O2 -g -fsigned-char -fno-strict-aliasing -DNO_ASM
       -DARCH_MIN_SSE2 -msse2 -D__LIBRETRO__ -DM64P_PLUGIN_API -DM64P_CORE_PROTOTYPES
       -I"$UPSTREAM/mupen64plus-core/src" -I"$UPSTREAM/mupen64plus-core/src/api"
       -I"$UPSTREAM/libretro-common/include" -I"$UPSTREAM/mupen64plus-core/subprojects/md5"
       -I"$SOURCE/mupen64plus-rsp-hle/src")
[[ "${R2N64_AUDIO_HLE_SANITIZE:-1}" != 1 ]] || FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all)
OBJECTS=()
for name in alist alist_audio alist_naudio alist_nead audio cicx105 hle hvqm jpeg memory mp3 musyx re2 plugin; do
    file="$SOURCE/mupen64plus-rsp-hle/src/$name.c"
    object="$BUILD/objects/$(basename "${file%.c}").o"
    "$CC" "${FLAGS[@]}" -c "$file" -o "$object"
    OBJECTS+=("$object")
done
"$CC" "${FLAGS[@]}" -I"$SOURCE/mupen64plus-rsp-cxd4" \
    -c "$SOURCE/mupen64plus-rsp-cxd4/rsp.c" -o "$BUILD/objects/cxd4-rsp.o"
"$CC" "${FLAGS[@]}" "$ROOT/tests/audio_hle_tests.c" "${OBJECTS[@]}" \
    "$BUILD/objects/cxd4-rsp.o" -lm -o "$BUILD/audio_hle_tests"
"$BUILD/audio_hle_tests" | tee "$BUILD/result.txt"
