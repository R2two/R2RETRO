#!/usr/bin/env python3
"""Original CPU workload for comparing the cached interpreter and x64 dynarec.

No Nintendo bootcode or game data. A short DMEM bootstrap copies our program
to RDRAM. It repeatedly calculates a fixed integer recurrence with MULTU,
MFLO, ADDU, SRL, XOR, load/store, and a non-empty branch delay slot. The host
checks the result with an independent implementation; VI drives retro_run.
"""
import argparse
import struct
from pathlib import Path


def cpu_benchmark_rom():
    body, labels, fixups = [], {}, []

    def imm(op, rt, rs, value):
        body.append((op << 26) | (rs << 21) | (rt << 16) | (value & 0xFFFF))

    def reg(function, rd, rs, rt, shift=0):
        body.append((rs << 21) | (rt << 16) | (rd << 11) | (shift << 6) | function)

    def load(rt, value):
        imm(15, rt, 0, value >> 16)
        imm(13, rt, rt, value)

    def branch(rs, rt, target, delay=0):
        fixups.append((len(body), target))
        imm(5, rt, rs, 0)
        body.append(delay)

    load(8, 0xA4400000)
    for offset, value in {
        0x04: 0x00100000, 0x08: 320, 0x0C: 2,
        0x14: 0x03E52239, 0x18: 525, 0x1C: 0xC15,
        0x20: 0x0C150C15, 0x24: 0x006C02EC, 0x28: 0x002501FF,
        0x2C: 0x000E0204, 0x30: 512, 0x34: 1024, 0x00: 0x3202,
    }.items():
        load(9, value)
        imm(43, 9, 8, offset)

    load(17, 0xA0000400)
    load(9, 0x52324350)  # R2CP signature
    imm(43, 9, 17, 0)
    load(16, 0)          # completed batches
    load(19, 1664525)
    load(20, 1013904223)
    load(21, 0x12345678)
    load(22, 8192)
    load(23, 0xA0000800)
    labels["batch"] = len(body)
    reg(0x21, 8, 21, 0)   # addu t0, s5, zero
    reg(0x21, 9, 22, 0)   # addu t1, s6, zero
    reg(0x21, 18, 0, 0)   # reset checksum
    labels["integer"] = len(body)
    reg(0x19, 0, 8, 19)   # multu t0, s3
    reg(0x12, 8, 0, 0)    # mflo t0
    reg(0x21, 8, 8, 20)   # addu t0, t0, s4
    reg(0x02, 10, 0, 8, 13)
    reg(0x26, 8, 8, 10)   # xor t0, t0, t2
    imm(43, 8, 23, 0)
    imm(35, 8, 23, 0)
    imm(9, 9, 9, -1)
    # bne's delay slot must execute for every iteration, including the last.
    branch(9, 0, "integer", (18 << 21) | (8 << 16) | (18 << 11) | 0x21)
    imm(43, 8, 17, 8)
    imm(43, 18, 17, 12)
    imm(9, 16, 16, 1)
    imm(43, 16, 17, 4)
    branch(22, 0, "batch")  # s6 is a permanent nonzero constant
    for index, target in fixups:
        body[index] |= (labels[target] - index - 1) & 0xFFFF

    # Fixed-length bootstrap with an explicitly encoded copy loop.
    boot = []
    def boot_load(rt, value):
        boot.extend(((15 << 26) | (rt << 16) | (value >> 16),
                     (13 << 26) | (rt << 21) | (rt << 16) | (value & 0xFFFF)))
    boot_words = 19
    boot_load(8, 0xA4000040 + boot_words * 4)
    boot_load(9, 0xA0001000)
    boot_load(10, len(body) * 4)
    loop = len(boot)
    boot.extend(((35 << 26) | (8 << 21) | (11 << 16),
                 (43 << 26) | (9 << 21) | (11 << 16),
                 (9 << 26) | (8 << 21) | (8 << 16) | 4,
                 (9 << 26) | (9 << 21) | (9 << 16) | 4,
                 (9 << 26) | (10 << 21) | (10 << 16) | 0xFFFC))
    boot.extend(((5 << 26) | (10 << 21) | ((loop - len(boot) - 1) & 0xFFFF), 0))
    boot_load(9, 0x80001000)
    boot.extend(((9 << 21) | 8, 0))  # jr t1; nop
    # Keep the bootstrap offset stable and assert rather than silently drift.
    boot.extend([0] * (boot_words - len(boot)))
    assert len(boot) == boot_words
    words = boot + body
    rom = bytearray(4096)
    struct.pack_into(">4I", rom, 0, 0x80371240, 0xF, 0xA4000040, 0)
    rom[0x20:0x34] = b"R2N64 CPU BENCH".ljust(20, b" ")
    rom[0x3B:0x3F] = b"NR4E"
    code = struct.pack(">" + "I" * len(words), *words)
    assert len(code) <= 0xFC0
    rom[0x40:0x40 + len(code)] = code
    return rom


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(cpu_benchmark_rom())
    print(f"Original synthetic CPU benchmark: {args.output}")
