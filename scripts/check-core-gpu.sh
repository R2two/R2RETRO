#!/usr/bin/env bash
# Tests a prebuilt GLES2 core without rebuilding or replacing any core library.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="${1:-$ROOT/build/core-lab/source}"
BUILD="$ROOT/build/core-gpu-check"
[[ -f "$SOURCE/mupen64plus_next_libretro.so" ]] || { echo 'Build the desktop GLES2 core first.' >&2; exit 1; }
mkdir -p "$BUILD"
cc -std=gnu11 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DGLES2 -DEGL \
    -I"$SOURCE/libretro-common" -I"$SOURCE/libretro-common/include" \
    -I"$SOURCE/libretro" -I"$SOURCE/custom" -I"$SOURCE/mupen64plus-core/src" \
    "$ROOT/tests/core_gpu_glsm_tests.c" -o "$BUILD/glsm-tests"
"$BUILD/glsm-tests"
c++ -std=c++17 -O1 -g -Wall -Wextra -Werror \
    "$ROOT/tests/core_gpu_probe.cpp" -I"$SOURCE/libretro-common/include" \
    $(pkg-config --cflags --libs sdl2 SDL2_image) -ldl -lGLESv2 -o "$BUILD/core-gpu-probe"
python3 "$ROOT/scripts/make_rdp_benchmark_rom.py" "$BUILD/rdp-workload.z64"
# Mesa's override prevents a higher context version from masking GLES2 issues.
# Other drivers may ignore it. This is a functional check, not a speed test.
MESA_GLES_VERSION_OVERRIDE=2.0 timeout 90s "$BUILD/core-gpu-probe" \
    "$SOURCE/mupen64plus_next_libretro.so" "$BUILD/rdp-workload.z64" "$BUILD/run"
