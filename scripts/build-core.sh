#!/usr/bin/env bash
# Builds the desktop core; optional isolated diagnostics remain available.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="$ROOT/external/mupen64plus-next"
REVISION=12edd2c74a517ff86dfa8cfc71ad75e4c10486d5
BUILD="$ROOT/build/core-lab"
SOURCE="$BUILD/source"
[[ -f "$UPSTREAM/Makefile" ]] || { echo 'Run git submodule update --init first.' >&2; exit 1; }
source "$ROOT/scripts/prepare-core.sh"
command -v nasm >/dev/null || { echo 'Install the NASM assembler dependency for the x64 core.' >&2; exit 1; }
prepare_core "$BUILD"
JOBS="${R2N64_BUILD_JOBS:-8}"
PROFILE="unix;WITH_DYNAREC=x86_64;CPPFLAGS=-DARCH_MIN_SSE2 -msse2;FORCE_GLES=1;HAVE_THR_AL=1;LLE=1;HAVE_PARALLEL_RSP=0;HAVE_PARALLEL_RDP=0;SYSTEM_LIBPNG=1;SYSTEM_ZLIB=1;$(nasm -v)"
if [[ ! -f "$BUILD/build-profile" || "$(cat "$BUILD/build-profile")" != "$PROFILE" ]]; then
    # Upstream's Makefile does not track changes in compiler flags.
    [[ "$(realpath "$SOURCE")" == "$(realpath "$ROOT")/build/core-lab/source" ]] || exit 1
    make -s -C "$SOURCE" clean GIT_VERSION="${REVISION:0:7}"
    printf '%s\n' "$PROFILE" > "$BUILD/build-profile"
fi
echo "Building Mupen64Plus-Next $REVISION (desktop, guarded x64 dynarec, Angrylion/GLideN64 GLES2/CXD4)."
echo "Compiler output: $BUILD/build.log"
if ! make -s -C "$SOURCE" -j "$JOBS" all platform=unix WITH_DYNAREC=x86_64 'CPPFLAGS=-DARCH_MIN_SSE2 -msse2' FORCE_GLES=1 HAVE_THR_AL=1 LLE=1 GIT_VERSION="${REVISION:0:7}" \
    HAVE_PARALLEL_RSP=0 HAVE_PARALLEL_RDP=0 SYSTEM_LIBPNG=1 SYSTEM_ZLIB=1 > "$BUILD/build.log" 2>&1; then
    tail -n 60 "$BUILD/build.log"
    exit 1
fi
sha256sum "$SOURCE/mupen64plus_next_libretro.so" > "$BUILD/core.sha256"
echo "Core built: $SOURCE/mupen64plus_next_libretro.so"
[[ "${1:-test}" == build-only ]] && exit 0
cmake -S "$ROOT" -B "$BUILD/host" -DCMAKE_BUILD_TYPE=Debug -DR2N64_CORE_LAB=ON
cmake --build "$BUILD/host" --target core_probe --parallel "$JOBS"
ctest --test-dir "$BUILD/host" -R '^core_(diagnostic_fixture|software_execution)$' --output-on-failure
