#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
MODE="${1:-ps4}"
python3 scripts/make_diagnostic_rom.py assets/diagnostic.z64
case "$MODE" in
  ps4)
    OO="${OO_PS4_TOOLCHAIN:-/opt/pacbrew/ps4/openorbis}"
    for file in cmake/ps4.cmake bin/clang bin/create-fself bin/linux/PkgTool.Core usr/lib/libSDL2.a usr/lib/libSDL2_ttf.a usr/lib/libSDL2_image.a; do
      [[ -f "$OO/$file" ]] || { echo "Falta OpenOrbis/PacBrew: $OO/$file" >&2; exit 1; }
    done
    export OPENORBIS="$OO" OO_PS4_TOOLCHAIN="$OO" DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
    bash scripts/check-core-ps4.sh
    bash scripts/build-handheld-cores.sh ps4
    bash scripts/build-console-cores.sh ps4
    BUILD="$ROOT/build/ps4"
    mkdir -p "$BUILD/payload/assets/fonts" "$BUILD/payload/assets/overlays" "$BUILD/payload/sce_sys" "$BUILD/payload/sce_module" "$BUILD/payload/licenses"
    cp assets/fonts/DejaVuSans.ttf "$BUILD/payload/assets/fonts/"
    cp assets/background.jpg "$BUILD/payload/assets/background.jpg"
    cp assets/background-room.jpg "$BUILD/payload/assets/background-room.jpg"
    cp assets/console-logos.png "$BUILD/payload/assets/console-logos.png"
    mkdir -p "$BUILD/payload/assets/certs"
    cp assets/certs/cacert.pem "$BUILD/payload/assets/certs/cacert.pem"
    cp assets/certs/README.md "$BUILD/payload/licenses/CA-bundle.md"
    cp assets/certs/MPL-2.0.txt "$BUILD/payload/licenses/CA-MPL-2.0.txt"
    mkdir -p "$BUILD/payload/licenses/network"
    cp assets/licenses/network/* "$BUILD/payload/licenses/network/"
    cp assets/overlays/gb.png assets/overlays/gbc.png assets/overlays/gba.png assets/overlays/snes.jpg "$BUILD/payload/assets/overlays/"
    cp assets/README.md "$BUILD/payload/licenses/App-artwork.md"
    cp assets/overlays/README.md "$BUILD/payload/licenses/Overlay-artwork.md"
    cp assets/diagnostic.z64 "$BUILD/payload/assets/diagnostic.z64"
    cp assets/fonts/LICENSE.txt "$BUILD/payload/licenses/DejaVuSans.txt"
    cp LICENSE "$BUILD/payload/licenses/R2N64-GPL-3.0.txt"
    cp external/goldhen/LICENSE "$BUILD/payload/licenses/GoldHEN.txt"
    cp docs/THIRD-PARTY.md "$BUILD/payload/licenses/THIRD-PARTY.md"
    cp THIRD_PARTY_LICENSES.md "$BUILD/payload/licenses/THIRD_PARTY_LICENSES.md"
    python3 scripts/stage-core-licenses.py "$BUILD/payload/licenses"
    cp -R build/handheld-ps4/licenses/. "$BUILD/payload/licenses/"
    cp -R build/console-ps4/licenses/. "$BUILD/payload/licenses/"
    cp pkg/icon0.png "$BUILD/payload/sce_sys/icon0.png"
    # Minimal public loader modules, identical sources to R2FPKGI.
    cp "$OO/samples/piglet/sce_module/libc.prx" "$OO/samples/piglet/sce_module/libSceFios2.prx" "$BUILD/payload/sce_module/"
    cmake -S . -B "$BUILD" -DCMAKE_TOOLCHAIN_FILE="$OO/cmake/ps4.cmake" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD" --parallel "$(nproc)"
    python3 scripts/package.py --build "$BUILD" --toolchain "$OO"
    ;;
  desktop|test)
    bash scripts/build-core.sh build-only
    bash scripts/build-handheld-cores.sh linux
    bash scripts/build-console-cores.sh linux
    cmake -S . -B build/desktop -DCMAKE_BUILD_TYPE=Debug
    cmake --build build/desktop --parallel "$(nproc)"
    if [[ "$MODE" == test ]]; then ctest --test-dir build/desktop --output-on-failure; fi
    ;;
  *) echo "Uso: bash scripts/build.sh [ps4|desktop|test]" >&2; exit 2 ;;
esac
