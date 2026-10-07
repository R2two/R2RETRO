#!/usr/bin/env python3
"""ASan/UBSan regression of GLideN64's actual header-name decoding block.

Uses original synthetic headers only. Does not run or package game ROMs.
"""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
source = root / "build/core-lab/source/GLideN64/src/RSP.cpp"
output = root / "build/gpu-rom-name-test"
output.mkdir(exist_ok=True)
text = source.read_text()
start = text.index("\tchar romname[21];", text.index("void RSP_Init()"))
end = text.index("\n\tif (strcmp(RSP.romname, romname)", start)
# Compile the production block, not a second implementation of its trimming.
body = text[start:end]
harness = r'''
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
static void check(const char* title, const char* expected) {
    unsigned char HEADER[64]{};
    for (unsigned i=0; i<20; ++i) HEADER[(32+i)^3] = title[i];
    @BODY@
    assert(std::string(romname) == expected);
}
int main() {
    check("\0                   ", "");
    check("                    ", "");
    check("ORIGINAL TEST       ", "ORIGINAL TEST");
    check("12345678901234567890", "12345678901234567890");
    check("AB\0                 ", "AB");
    puts("PASS: empty, spaces, padded, full-length and embedded-NUL GPU ROM names.");
}
'''.replace("@BODY@", body)
(output / "rom_name.cpp").write_text(harness)
subprocess.run(["c++", "-std=c++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                str(output / "rom_name.cpp"), "-o", str(output / "rom-name-tests")], check=True)
subprocess.run([str(output / "rom-name-tests")], check=True)
