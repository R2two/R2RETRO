#!/usr/bin/env python3
"""Original RDP rasterizer workload; no game data or Nintendo boot code.

The CPU submits raw RDP commands for 24 full-screen one-cycle rectangles and
three final RGB bands on each VI. This exercises Angrylion's pixel pipeline,
combiner, framebuffer writes, command synchronization and worker shutdown.
It does not model a particular commercial game or benchmark RSP/CPU speed.
"""
import argparse
import struct
from pathlib import Path


def rdp_benchmark_rom():
    words, labels, fixups = [], {}, []

    def imm(op, rt, rs, value):
        words.append((op << 26) | (rs << 21) | (rt << 16) | (value & 0xFFFF))

    def load(rt, value):
        imm(15, rt, 0, value >> 16)
        imm(13, rt, rt, value)

    def branch(op, rs, rt, target):
        fixups.append((len(words), target))
        imm(op, rt, rs, 0)
        words.append(0)

    # RDP state: RGBA5551, 320x240, one-cycle combiner with primitive RGBA.
    # Both combiner cycles use (0 - 0) * 0 + primitive. Dithering is disabled
    # so exact pixel equality is an independent 1-versus-4-worker oracle.
    combine0 = 0xFC000000 | (15 << 20) | (31 << 15) | (7 << 12) | (7 << 9) | (15 << 5) | 31
    combine1 = (15 << 28) | (15 << 24) | (7 << 21) | (7 << 18) | (3 << 15) | (7 << 12) | (3 << 9) | (3 << 6) | (7 << 3) | 3
    commands = [
        0x3F10013F, 0x00100000,  # SET_COLOR_IMAGE: 16-bit framebuffer
        0x2D000000, (320 * 4 << 12) | (240 * 4),  # SET_SCISSOR
        0x2F0000F0, 0,          # SET_OTHER_MODES: one-cycle, no dither
        combine0, combine1,
    ]

    def rectangle(color, top, bottom):
        commands.extend((0x3A000000, color, 0x36000000 | (320 * 4 << 12) | (bottom * 4), top * 4))

    for _ in range(8):
        for color in (0xFF0000FF, 0x00FF00FF, 0x0000FFFF):
            rectangle(color, 0, 240)
    for row, color in enumerate((0xFF0000FF, 0x00FF00FF, 0x0000FFFF)):
        rectangle(color, row * 80, (row + 1) * 80)
    commands.extend((0x29000000, 0))  # SYNC_FULL flushes all worker commands

    load(8, 0xA0001000)
    for index, value in enumerate(commands):
        load(9, value)
        imm(43, 9, 8, index * 4)

    load(16, 0xA4400000)
    registers = {
        0x04: 0x00100000, 0x08: 320, 0x0C: 2,
        0x14: 0x03E52239, 0x18: 525, 0x1C: 0xC15,
        0x20: 0x0C150C15, 0x24: 0x006C02EC, 0x28: 0x002501FF,
        0x2C: 0x000E0204, 0x30: 512, 0x34: 1024, 0x00: 0x3202,
    }
    for offset, value in registers.items():
        load(9, value)
        imm(43, 9, 16, offset)

    load(17, 0xA4100000)  # DPC registers
    load(18, 0x1000)
    load(19, 0x1000 + len(commands) * 4)
    load(20, 0xA0000400)
    load(9, 0x52324450)   # R2DP, independent CPU execution marker
    imm(43, 9, 20, 0)
    load(21, 0)
    load(9, 0x15)         # clear XBUS, FREEZE and FLUSH
    imm(43, 9, 17, 0x0C)

    labels["frame"] = len(words)
    imm(43, 18, 17, 0)   # DPC_START
    imm(43, 19, 17, 4)   # DPC_END: actual renderer workload
    imm(9, 21, 21, 1)
    imm(43, 21, 20, 4)
    labels["bottom"] = len(words)
    imm(35, 9, 16, 0x10)
    imm(11, 10, 9, 400)  # sltiu current scanline < 400
    branch(5, 10, 0, "bottom")
    labels["top"] = len(words)
    imm(35, 9, 16, 0x10)
    imm(11, 10, 9, 100)
    branch(4, 10, 0, "top")
    branch(4, 0, 0, "frame")

    for index, target in fixups:
        distance = labels[target] - index - 1
        assert -32768 <= distance <= 32767
        words[index] = (words[index] & 0xFFFF0000) | (distance & 0xFFFF)
    rom = bytearray(4096)
    struct.pack_into(">4I", rom, 0, 0x80371240, 0xF, 0xA4000040, 0)
    rom[0x20:0x34] = b"R2N64 RDP BENCH".ljust(20, b" ")
    rom[0x3B:0x3F] = b"NR3E"
    code = struct.pack(">" + "I" * len(words), *words)
    assert len(code) <= 0xFC0
    rom[0x40:0x40 + len(code)] = code
    return rom


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(rdp_benchmark_rom())
    print(f"Original synthetic RDP benchmark: {args.output}")
