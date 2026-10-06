#!/usr/bin/env bash
# Pinned NES/SNES cores, built in isolated copies with all exports namespaced.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${1:-linux}"
[[ "$TARGET" == linux || "$TARGET" == ps4 ]] || { echo 'Usage: build-console-cores.sh linux|ps4' >&2; exit 2; }
BUILD="$ROOT/build/console-$TARGET"
JOBS="${R2N64_BUILD_JOBS:-8}"
FCEUMM_REV=7a542dab1e87679921962a9f056186eca425c0c2
BSNES_REV=79d7f9de218b6ffa65a80bbdc5828532bc239232
OPENORBIS="${OPENORBIS:-/opt/pacbrew/ps4/openorbis}"
mkdir -p "$BUILD/lib" "$BUILD/licenses"
for tool in git cmake make python3 nm objcopy ar tar sha256sum; do command -v "$tool" >/dev/null; done
prepare() {
    local name="$1" revision="$2" source="$BUILD/$1-source"
    [[ "$(git -C "$ROOT/external/$name" rev-parse HEAD)" == "$revision" ]] || { echo "Unexpected $name revision" >&2; exit 1; }
    if [[ ! -f "$source/.r2n64-revision" ]]; then
        mkdir -p "$source"
        git -C "$ROOT/external/$name" archive "$revision" | tar -x --exclude=profile -C "$source"
        printf '%s\n' "$revision" > "$source/.r2n64-revision"
    fi
    [[ "$(cat "$source/.r2n64-revision")" == "$revision" ]] || { echo "Stale isolated source $source" >&2; exit 1; }
}
prepare fceumm "$FCEUMM_REV"
prepare bsnes-mercury "$BSNES_REV"
python3 "$ROOT/scripts/prepare-console-cores.py" "$ROOT" "$BUILD"
CC=cc
CXX=c++
AR=ar
FLAGS='-O2 -fPIC -ffunction-sections -fdata-sections'
CXX_ONLY=''
LINK_ARGS=()
if [[ "$TARGET" == ps4 ]]; then
    [[ -x "$OPENORBIS/bin/clang" ]] || { echo 'Existing OpenOrbis toolchain unavailable' >&2; exit 1; }
    CC="$OPENORBIS/bin/clang"
    CXX="$OPENORBIS/bin/clang++"
    AR="$OPENORBIS/bin/llvm-ar"
    FLAGS+=" -target x86_64-pc-freebsd12-elf -D__PS4__ -D__OPENORBIS__ -D__ORBIS__ -DPS4 -D__BSD_VISIBLE -D_BSD_SOURCE -D_GNU_SOURCE -isysroot $OPENORBIS -isystem $OPENORBIS/include -I$OPENORBIS/usr/include"
    CXX_ONLY="-I$OPENORBIS/include/c++/v1"
    export OPENORBIS
    LINK_ARGS+=("-DCMAKE_TOOLCHAIN_FILE=$OPENORBIS/cmake/ps4.cmake")
fi
PROFILE="$FCEUMM_REV;$BSNES_REV;performance;hdpack=0;ntsc=1;$(sha256sum "$ROOT/scripts/prepare-console-cores.py" | cut -d' ' -f1);$CC;$FLAGS;$CXX_ONLY;$("$CC" --version | head -n 1)"
FORCE=()
if [[ ! -f "$BUILD/profile" || "$(cat "$BUILD/profile")" != "$PROFILE" ]]; then FORCE=(-B); fi
echo "Building FCEUmm ($TARGET); log: $BUILD/fceumm.log"
make -C "$BUILD/fceumm-source" -f Makefile.libretro -j "$JOBS" "${FORCE[@]}" \
    platform=unix STATIC_LINKING=1 "TARGET=$BUILD/fceumm_libretro.a" \
    "CC=$CC $FLAGS" "AR=$AR" "GIT_VERSION=${FCEUMM_REV:0:7}" \
    HAVE_HDPACK=0 HAVE_NTSC=1 > "$BUILD/fceumm.log" 2>&1 || { tail -n 70 "$BUILD/fceumm.log"; exit 1; }
echo "Building bsnes-mercury Performance ($TARGET); log: $BUILD/bsnes-mercury.log"
make -C "$BUILD/bsnes-mercury-source" -j "$JOBS" "${FORCE[@]}" \
    platform=unix PROFILE=performance STATIC_LINKING=1 "TARGET=$BUILD/bsnes_mercury_libretro.a" \
    "CC=$CC $FLAGS" "CXX=$CXX $FLAGS $CXX_ONLY" "AR=$AR" "GIT_VERSION=${BSNES_REV:0:7}" \
    > "$BUILD/bsnes-mercury.log" 2>&1 || { tail -n 90 "$BUILD/bsnes-mercury.log"; exit 1; }
python3 "$ROOT/scripts/namespace-handheld-core.py" fceumm "$BUILD/fceumm_libretro.a" "$BUILD/lib/libfceumm_libretro.a"
python3 "$ROOT/scripts/namespace-handheld-core.py" bsnes_mercury "$BUILD/bsnes_mercury_libretro.a" "$BUILD/lib/libbsnes_mercury_libretro.a"
printf '%s\n' "$PROFILE" > "$BUILD/profile"
printf 'fceumm %s\nbsnes_mercury %s\nplatform %s\nbsnes_profile performance\nfceumm_hdpack disabled\nflags %s\n' "$FCEUMM_REV" "$BSNES_REV" "$TARGET" "$FLAGS $CXX_ONLY" > "$BUILD/provenance.txt"
"$CC" --version | head -n 1 >> "$BUILD/provenance.txt"
sha256sum "$BUILD/lib/"*.a > "$BUILD/cores.sha256"
cp "$ROOT/external/fceumm/Copying" "$BUILD/licenses/FCEUmm-GPL-2.0-or-later.txt"
cp "$ROOT/external/fceumm/Authors" "$BUILD/licenses/FCEUmm-Authors.txt"
cp "$ROOT/external/bsnes-mercury/LICENSE" "$BUILD/licenses/bsnes-mercury-GPL-3.0.txt"
sed -n '1,26p' "$ROOT/external/bsnes-mercury/libco/libco.h" > "$BUILD/licenses/bsnes-libco-MIT.txt"
cp "$BUILD/provenance.txt" "$BUILD/licenses/console-versions.txt"
python3 "$ROOT/scripts/stage-console-licenses.py" "$BUILD/licenses"
cp "$ROOT/scripts/prepare-console-cores.py" "$BUILD/licenses/R2N64-console-source-changes.py"
printf 'FCEUmm source: https://github.com/libretro/libretro-fceumm/tree/%s\nbsnes-mercury source: https://github.com/libretro/bsnes-mercury/tree/%s\nReproduction: scripts/build-console-cores.sh linux|ps4\nNo external firmware or game ROM files are copied.\n' "$FCEUMM_REV" "$BSNES_REV" > "$BUILD/licenses/console-sources.txt"
cmake -S "$ROOT/scripts/console-link" -B "$BUILD/link" \
    "-DR2N64_CONSOLE_LIBDIR=$BUILD/lib" "${LINK_ARGS[@]}" > "$BUILD/link.log" 2>&1
cmake --build "$BUILD/link" --parallel 2 >> "$BUILD/link.log" 2>&1 || { tail -n 90 "$BUILD/link.log"; exit 1; }
if [[ "$TARGET" == linux ]]; then
    "$BUILD/link/console_link_check"
    "$BUILD/link/fceumm_palette_check" "$BUILD/palette-tests"
    "$BUILD/link/bsnes_chip_check" "$BUILD/chip-tests"
fi
echo "Built and linked: $BUILD/lib/lib{fceumm,bsnes_mercury}_libretro.a"
