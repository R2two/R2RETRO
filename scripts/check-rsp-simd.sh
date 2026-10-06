#!/usr/bin/env bash
# Differential CPU-only CXD4 test, using original synthetic vector operands.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="$ROOT/external/mupen64plus-next/mupen64plus-rsp-cxd4"
BUILD="$ROOT/build/rsp-simd"
CC="${R2N64_RSP_CC:-/opt/pacbrew/ps4/openorbis/bin/clang}"
CXX="${R2N64_RSP_CXX:-/opt/pacbrew/ps4/openorbis/bin/clang++}"
FLAGS=(-O3 -DNDEBUG -fsigned-char -ffast-math -fno-strict-aliasing
       -fomit-frame-pointer -fvisibility=hidden -DNO_ASM -I"$SOURCE")
mkdir -p "$BUILD/scalar" "$BUILD/sse2"
for profile in scalar sse2; do
    EXTRA=()
    [[ "$profile" != sse2 ]] || EXTRA=(-DARCH_MIN_SSE2 -msse2)
    OBJECTS=()
    for file in vu add multiply logical select divide; do
        object="$BUILD/$profile/$file.o"
        "$CC" "${FLAGS[@]}" "${EXTRA[@]}" -c "$SOURCE/vu/$file.c" -o "$object"
        OBJECTS+=("$object")
    done
    "$CXX" -std=c++17 "${FLAGS[@]}" "${EXTRA[@]}" "$ROOT/tests/rsp_vector_tests.cpp" \
        "${OBJECTS[@]}" -o "$BUILD/$profile/rsp_vector_tests"
    "$BUILD/$profile/rsp_vector_tests" > "$BUILD/$profile.json"
done
python3 - "$BUILD" <<'PY'
import json
import pathlib
import sys
root = pathlib.Path(sys.argv[1])
scalar = json.loads((root / 'scalar.json').read_text())
simd = json.loads((root / 'sse2.json').read_text())
print(json.dumps(scalar))
print(json.dumps(simd))
if scalar['digest'] != simd['digest']:
    raise SystemExit('FAIL: scalar/SSE2 vector state mismatch')
print(f"PASS: {scalar['compared_operations']} vector operations, matching state digests.")
print(f"Host VU-only median speed ratio: {scalar['median_ms']/simd['median_ms']:.3f}x (not PS4 FPS).")
PY
