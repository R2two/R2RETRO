#!/usr/bin/env bash
# Cross-compile/link check only. Does not create or replace a PKG.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="$ROOT/external/mupen64plus-next"
REVISION=12edd2c74a517ff86dfa8cfc71ad75e4c10486d5
BUILD="$ROOT/build/core-ps4"
SOURCE="$BUILD/source"
export OPENORBIS="${OO_PS4_TOOLCHAIN:-/opt/pacbrew/ps4/openorbis}"
[[ -x "$OPENORBIS/bin/clang" ]] || { echo 'OpenOrbis compiler missing.' >&2; exit 1; }
source "$ROOT/scripts/prepare-core.sh"
command -v nasm >/dev/null || { echo 'Install the NASM assembler dependency for the x64 core.' >&2; exit 1; }
prepare_core "$BUILD"
export PKG_CONFIG_DIR= PKG_CONFIG_PATH= PKG_CONFIG_SYSROOT_DIR=
export PKG_CONFIG_LIBDIR="$OPENORBIS/usr/lib/pkgconfig"
MAKE_ARGS=(platform=unix WITH_DYNAREC=x86_64 "GIT_VERSION=${REVISION:0:7}"
    HAVE_THR_AL=1 LLE=1 HAVE_PARALLEL_RSP=0 HAVE_PARALLEL_RDP=0
    SYSTEM_LIBPNG=1 SYSTEM_ZLIB=1 STATIC_LINKING=1 FORCE_GLES=1 TARGET=mupen64plus_next_libretro.a)
PROFILE="$( { printf '%s\n' "${MAKE_ARGS[@]}" "$OPENORBIS"; cat "$ROOT/scripts/mupen64plus-openorbis.mk"; "$OPENORBIS/bin/clang" --version; nasm -v; } | sha256sum)"
if [[ -f "$BUILD/build-profile" && "$(cat "$BUILD/build-profile")" != "$PROFILE" ]]; then
    [[ "$(realpath "$SOURCE")" == "$(realpath "$ROOT")/build/core-ps4/source" ]] || exit 1
    make -s -C "$SOURCE" clean "${MAKE_ARGS[@]}"
fi
printf '%s\n' "$PROFILE" > "$BUILD/build-profile"
# Recreate the archive: ar replaces members by basename and may retain stale
# members when upstream source lists or the selected profile change.
rm -f -- "$SOURCE/mupen64plus_next_libretro.a"
echo "Cross-compiling core with OpenOrbis. Log: $BUILD/build.log"
if ! make -s -C "$SOURCE" -f Makefile -f "$ROOT/scripts/mupen64plus-openorbis.mk" \
    -j "${R2N64_BUILD_JOBS:-8}" all "${MAKE_ARGS[@]}" \
    > "$BUILD/build.log" 2>&1; then
    tail -n 60 "$BUILD/build.log"
    exit 1
fi
sha256sum "$SOURCE/mupen64plus_next_libretro.a" > "$BUILD/core.sha256"
cmake -S "$ROOT/tests/core-link" -B "$BUILD/link" \
    -DCMAKE_TOOLCHAIN_FILE="$OPENORBIS/cmake/ps4.cmake" -DCMAKE_BUILD_TYPE=Release \
    -DR2N64_CORE_SOURCE="$UPSTREAM" -DR2N64_CORE_LIBRARY="$SOURCE/mupen64plus_next_libretro.a"
cmake --build "$BUILD/link" --parallel 2
echo 'PS4 core compile/link check passed. Hardware execution remains unverified.'
