#!/usr/bin/env bash
# Build pinned, statically namespaced libretro handheld cores; never edits submodules.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${1:-linux}"
[[ "$TARGET" == linux || "$TARGET" == ps4 ]] || { echo 'Usage: build-handheld-cores.sh linux|ps4' >&2; exit 2; }
BUILD="$ROOT/build/handheld-$TARGET"
MGBA_BUILD="$BUILD/mgba-build"
JOBS="${R2N64_BUILD_JOBS:-8}"
SAMEBOY_REV=8230189896a8bb6598574d302ba0ad3658f98ab4
MGBA_REV=26b7884bc25a5933960f3cdcd98bac1ae14d42e2
OPENORBIS="${OPENORBIS:-/opt/pacbrew/ps4/openorbis}"
mkdir -p "$BUILD/lib"
for tool in git cmake make python3 nm objcopy ar tar hexdump sha256sum; do command -v "$tool" >/dev/null; done
prepare() {
    local name="$1" revision="$2" source="$BUILD/$1-source"
    [[ "$(git -C "$ROOT/external/$name" rev-parse HEAD)" == "$revision" ]] || { echo "Unexpected $name revision" >&2; exit 1; }
    if [[ ! -f "$source/.r2n64-revision" ]]; then
        mkdir -p "$source"
        git -C "$ROOT/external/$name" archive "$revision" | tar -x -C "$source"
        printf '%s\n' "$revision" > "$source/.r2n64-revision"
    fi
    [[ "$(cat "$source/.r2n64-revision")" == "$revision" ]] || { echo "Remove stale isolated build $source before rebuilding" >&2; exit 1; }
}
prepare sameboy "$SAMEBOY_REV"
prepare mgba "$MGBA_REV"
# SameBoy libretro upstream emits at half the GB clock (~2 MHz). Generate audio
# through SameBoy's own sample-rate API at the frontend/device rate instead.
python3 - "$BUILD/sameboy-source/libretro/libretro.c" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
old = 'GB_set_sample_rate(&gameboy[i], GB_get_clock_rate(&gameboy[i]) / 2);'
new = 'GB_set_sample_rate(&gameboy[i], 48000); /* R2N64 device audio rate */'
assert s.count(old) == 1 or s.count(new) == 1
if old in s:
    p.write_text(s.replace(old, new))
PY
CC=cc
AR=ar
COMMON_FLAGS='-O2 -fPIC -ffunction-sections -fdata-sections'
CMAKE_ARGS=()
if [[ "$TARGET" == ps4 ]]; then
    MGBA_BUILD="$BUILD/mgba-openorbis-build"
    [[ -x "$OPENORBIS/bin/clang" ]] || { echo 'Existing OpenOrbis toolchain unavailable' >&2; exit 1; }
    CC="$OPENORBIS/bin/clang"
    AR="$OPENORBIS/bin/llvm-ar"
    COMMON_FLAGS+=" -target x86_64-pc-freebsd12-elf -D__PS4__ -D__OPENORBIS__ -D__ORBIS__ -DPS4 -D__BSD_VISIBLE -D_BSD_SOURCE -D_GNU_SOURCE -isysroot $OPENORBIS -isystem $OPENORBIS/include -I$OPENORBIS/usr/include"
    export OPENORBIS
    CMAKE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=$ROOT/scripts/handheld-openorbis.cmake" "-DCMAKE_TRY_COMPILE_TARGET_TYPE=EXECUTABLE" "-DCMAKE_CXX_FLAGS=$COMMON_FLAGS -I$OPENORBIS/include/c++/v1" -U 'HAVE_*')
fi
# The official libretro tag supplies SameBoy's own open source replacement boot ROMs.
echo "Building SameBoy ($TARGET); log: $BUILD/sameboy.log"
SAMEBOY_FORCE=()
SAMEBOY_PROFILE="$SAMEBOY_REV;audio=48000;$CC;$COMMON_FLAGS;$("$CC" --version | head -n 1)"
if [[ ! -f "$BUILD/sameboy-profile" || "$(cat "$BUILD/sameboy-profile")" != "$SAMEBOY_PROFILE" ]]; then
    SAMEBOY_FORCE=(-B)
fi
if ! CFLAGS="$COMMON_FLAGS" make -C "$BUILD/sameboy-source/libretro" -j "$JOBS" "${SAMEBOY_FORCE[@]}" \
    STATIC_LINKING=1 platform=unix CC="$CC" AR="$AR" GIT_VERSION="${SAMEBOY_REV:0:7}" \
    BOOTROMS_DIR="$BUILD/sameboy-source/BootROMs/prebuilt" BIN="$BUILD" > "$BUILD/sameboy.log" 2>&1; then
    tail -n 60 "$BUILD/sameboy.log"; exit 1
fi
printf '%s\n' "$SAMEBOY_PROFILE" > "$BUILD/sameboy-profile"
# Upstream's libretro target is shared only; change that target in the isolated
# copy. Pin metadata as well: git describe must not discover the parent project.
python3 - "$BUILD/mgba-source" "$ROOT/external/mgba" "$MGBA_REV" <<'PY'
from pathlib import Path
import subprocess, sys
root = Path(sys.argv[1])
p = root / 'CMakeLists.txt'
s = p.read_text()
old = 'add_library(${BINARY_NAME}_libretro SHARED ${CORE_SRC} ${RETRO_SRC})'
new = 'add_library(${BINARY_NAME}_libretro STATIC ${CORE_SRC} ${RETRO_SRC})'
assert s.count(old) == 1 or s.count(new) == 1
if old in s:
    p.write_text(s.replace(old, new))
version = subprocess.check_output(['git', '-C', sys.argv[2], 'show', sys.argv[3] + ':version.cmake'], text=True)
version = ('set(SKIP_GIT ON)\nset(GIT_COMMIT "' + sys.argv[3] + '")\n'
           'set(GIT_COMMIT_SHORT "' + sys.argv[3][:7] + '")\nset(GIT_TAG "0.10.5")\n' + version)
vp = root / 'version.cmake'
if vp.read_text() != version:
    vp.write_text(version)
libretro = subprocess.check_output(['git', '-C', sys.argv[2], 'show', sys.argv[3] + ':src/platform/libretro/libretro.c'], text=True)
old = '\tcase RETRO_MEMORY_SAVE_RAM:\n\t\treturn savedata;'
new = '''\tcase RETRO_MEMORY_SAVE_RAM:
#ifdef M_CORE_GBA
\t\t/* R2N64: a loaded state can install a temporary SRAM mask. Expose
\t\t * that active memory when the frontend persists before unload.
\t\t * Before deferred setup, native saves must still fill the original
\t\t * frontend buffer which _doDeferredSetup attaches to the core. */
\t\tif (!deferredSetup && core && core->platform(core) == mPLATFORM_GBA) {
\t\t\tstruct GBA* gba = core->board;
\t\t\tif (gba->memory.savedata.data) {
\t\t\t\treturn gba->memory.savedata.data;
\t\t\t}
\t\t}
#endif
\t\treturn savedata;'''
assert libretro.count(old) == 1
libretro = libretro.replace(old, new)
lp = root / 'src/platform/libretro/libretro.c'
if lp.read_text() != libretro:
    lp.write_text(libretro)
PY
echo "Building mGBA ($TARGET); logs: $BUILD/mgba-configure.log, $BUILD/mgba.log"
cmake -S "$BUILD/mgba-source" -B "$MGBA_BUILD" \
    -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_FLAGS=$COMMON_FLAGS" \
    -DBUILD_LIBRETRO=ON -DSKIP_LIBRARY=ON -DDISABLE_FRONTENDS=ON -DDISABLE_DEPS=ON \
    -DM_CORE_GBA=ON -DM_CORE_GB=OFF -DUSE_DEBUGGERS=OFF -DENABLE_SCRIPTING=OFF \
    -DUSE_FFMPEG=OFF -DUSE_MINIZIP=OFF -DUSE_LIBZIP=OFF -DUSE_LZMA=OFF -DUSE_ELF=OFF \
    -DBUILD_GL=OFF -DBUILD_GLES2=OFF -DBUILD_GLES3=OFF -DUSE_EPOXY=OFF \
    -DBUILD_LTO=OFF -DMINIMAL_CORE=ON "${CMAKE_ARGS[@]}" > "$BUILD/mgba-configure.log" 2>&1 || { tail -n 60 "$BUILD/mgba-configure.log"; exit 1; }
cmake --build "$MGBA_BUILD" --target mgba_libretro --parallel "$JOBS" > "$BUILD/mgba.log" 2>&1 || { tail -n 70 "$BUILD/mgba.log"; exit 1; }
python3 "$ROOT/scripts/namespace-handheld-core.py" sameboy "$BUILD/sameboy_libretro.a" "$BUILD/lib/libsameboy_libretro.a"
python3 "$ROOT/scripts/namespace-handheld-core.py" mgba "$MGBA_BUILD/mgba_libretro.a" "$BUILD/lib/libmgba_libretro.a"
printf 'sameboy %s\nmgba %s\nplatform %s\nsameboy_audio_rate 48000\nmgba_libretro_linkage STATIC\nmgba_sram_getter active_after_deferred_setup\nflags %s\n' "$SAMEBOY_REV" "$MGBA_REV" "$TARGET" "$COMMON_FLAGS" > "$BUILD/provenance.txt"
"$CC" --version | head -n 1 >> "$BUILD/provenance.txt"
sha256sum "$BUILD/lib/"*.a > "$BUILD/cores.sha256"
mkdir -p "$BUILD/licenses"
cp "$ROOT/external/sameboy/LICENSE" "$BUILD/licenses/SameBoy-Expat.txt"
cp "$ROOT/external/mgba/LICENSE" "$BUILD/licenses/mGBA-MPL-2.0.txt"
cp "$ROOT/external/mgba/src/third-party/blip_buf/license.txt" "$BUILD/licenses/mGBA-blip_buf-LGPL-2.1.txt"
sed -n '1,21p' "$ROOT/external/mgba/src/third-party/blip_buf/blip_buf.c" > "$BUILD/licenses/mGBA-blip_buf-NOTICE.txt"
cp "$ROOT/external/mgba/src/third-party/inih/LICENSE.txt" "$BUILD/licenses/mGBA-inih-BSD-3-Clause.txt"
cp "$BUILD/provenance.txt" "$BUILD/licenses/handheld-versions.txt"
cp "$ROOT/docs/HANDHELD-CORES.md" "$BUILD/licenses/handheld-sources.md"
LINK_CMAKE_ARGS=()
if [[ "$TARGET" == ps4 ]]; then
    LINK_CMAKE_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=$OPENORBIS/cmake/ps4.cmake")
fi
cmake -S "$ROOT/scripts/handheld-link" -B "$BUILD/link" \
    "-DR2N64_HANDHELD_LIBDIR=$BUILD/lib" "${LINK_CMAKE_ARGS[@]}" > "$BUILD/link.log" 2>&1
cmake --build "$BUILD/link" --parallel 2 >> "$BUILD/link.log" 2>&1 || { tail -n 60 "$BUILD/link.log"; exit 1; }
if [[ "$TARGET" == linux ]]; then "$BUILD/link/handheld_link_check"; fi
echo "Built: $BUILD/lib/lib{sameboy,mgba}_libretro.a"
