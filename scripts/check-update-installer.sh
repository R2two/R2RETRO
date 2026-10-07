#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
OO="${OO_PS4_TOOLCHAIN:-/opt/pacbrew/ps4/openorbis}"
mkdir -p build/update-tests
g++ -std=c++17 -Wall -Wextra -Wpedantic -g -fsanitize=address,undefined \
    -DR2N64_PS4=1 -Iinclude -idirafter "$OO/include" tests/update_installer_tests.cpp \
    src/update_installer.cpp -Wl,--wrap=open -o build/update-tests/installer
ASAN_OPTIONS=detect_leaks=0 build/update-tests/installer
for backend in native ps4-path; do
    extra=()
    if [[ "$backend" == ps4-path ]]; then
        extra=(-DR2N64_PATH_FILEOPS=1 tests/file_ops_stubs.cpp
            -Wl,--wrap=openat -Wl,--wrap=mkdirat -Wl,--wrap=renameat
            -Wl,--wrap=unlinkat -Wl,--wrap=fstatat)
    fi
    g++ -std=c++17 -Wall -Wextra -Wpedantic -g -pthread -fsanitize=address,undefined \
        '-DR2N64_VERSION="0.5.4"' '-DR2N64_SFO_VERSION="00.54"' -Iinclude \
        tests/update_tests.cpp src/update.cpp src/update_sha256.cpp src/file_ops.cpp \
        "${extra[@]}" -o "build/update-tests/validation-$backend"
    "build/update-tests/validation-$backend"
done
