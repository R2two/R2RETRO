#!/usr/bin/env bash
# Test actual patched CXD4 dispatch and Angrylion barriers with original data.
# SOURCE is a prepared isolated core copy; never modifies it or the submodule.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="${1:-$ROOT/build/core-lab/source}"
OUT="${2:-$ROOT/build/n64-hotpath-tests}"
CC="${CC:-cc}"
CXX="${CXX:-c++}"
mkdir -p "$OUT"
FLAGS=(-O2 -g -fsigned-char -fno-strict-aliasing -DNO_ASM
       -DARCH_MIN_SSE2 -msse2 -D__LIBRETRO__ -DM64P_PLUGIN_API -DM64P_CORE_PROTOTYPES
       -I"$SOURCE/mupen64plus-core/src" -I"$SOURCE/mupen64plus-core/src/api"
       -I"$SOURCE/libretro-common/include" -I"$SOURCE/mupen64plus-core/subprojects/md5"
       -I"$SOURCE/mupen64plus-rsp-cxd4")
SANITIZERS=()
[[ "${R2N64_HOTPATH_SANITIZE:-1}" != 1 ]] || SANITIZERS=(-fsanitize=address,undefined -fno-sanitize-recover=all)
"$CC" -std=gnu11 "${FLAGS[@]}" "${SANITIZERS[@]}" \
    "$ROOT/tests/rsp_element_tests.c" -lm -o "$OUT/rsp_element_tests"
"$OUT/rsp_element_tests"
"$CXX" -std=c++17 -O2 -g -Wall -Wextra -Werror -pthread "${SANITIZERS[@]}" \
    -I"$SOURCE/mupen64plus-video-angrylion" "$ROOT/tests/rdp_workers_tests.cpp" \
    "$SOURCE/mupen64plus-video-angrylion/parallel_al.cpp" -o "$OUT/rdp_workers_tests"
"$OUT/rdp_workers_tests"
