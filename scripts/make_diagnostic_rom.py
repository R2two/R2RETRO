#!/usr/bin/env python3
"""Original R2N64 diagnostic, not a game or Nintendo bootcode.

Runs from IPL3 DMEM under Mupen's HLE boot. Draws RGB bands, plays a quiet
500 Hz square wave through AI and reads controller 1 through SI/PIF. A square
turns white for button/stick activity. Not intended to boot on a physical N64.
RDRAM words: 0x400="R2N6", 0x404=loop counter, 0x408=Joybus input response.
Encodings and VI/AI/SI behavior are exercised against the pinned core.
"""
import argparse
import struct
from pathlib import Path


def diagnostic_rom():
    words = []
    labels = {}
    fixups = []

    def emit(value):
        words.append(value)

    def imm(op, rt, rs, value):
        emit((op << 26) | (rs << 21) | (rt << 16) | (value & 0xFFFF))

    def load(rt, value):
        imm(15, rt, 0, value >> 16)  # lui
        imm(13, rt, rt, value)      # ori

    def branch(op, rs, rt, target):
        if isinstance(target, str):
            fixups.append((len(words), target))
            target = len(words) + 1
        imm(op, rt, rs, target - len(words) - 1)
        emit(0)                    # branch delay slot

    def label(name):
        assert name not in labels
        labels[name] = len(words)

    def shift(rd, rt, count, right=False):
        emit((rt << 16) | (rd << 11) | (count << 6) | (2 if right else 0))

    def wait_si(name):
        label(name)
        imm(35, 9, 18, 0x18)      # lw t1, SI_STATUS(s2)
        imm(12, 9, 9, 3)          # andi DMA_BUSY | IO_BUSY
        branch(5, 9, 0, name)
        imm(43, 0, 18, 0x18)      # acknowledge SI interrupt

    load(8, 0xA0100000)             # uncached framebuffer
    for color in (0xF801, 0x07C1, 0x003F):
        load(9, 320 * 80)
        load(10, color)
        loop = len(words)
        imm(41, 10, 8, 0)          # sh t2, 0(t0)
        imm(9, 8, 8, 2)            # addiu t0, t0, 2
        imm(9, 9, 9, -1)
        branch(5, 9, 0, loop)      # bne t1, zero, loop

    load(8, 0xA4400000)             # VI base; NTSC, 320x240 RGBA5551
    registers = {
        0x04: 0x00100000, 0x08: 320, 0x0C: 2,
        0x14: 0x03E52239, 0x18: 525, 0x1C: 0xC15,
        0x20: 0x0C150C15, 0x24: 0x006C02EC, 0x28: 0x002501FF,
        0x2C: 0x000E0204, 0x30: 512, 0x34: 1024, 0x00: 0x3202,
    }
    for offset, value in registers.items():
        load(9, value)
        imm(43, 9, 8, offset)      # sw

    # Original stereo PCM: 1024 frames, +/-2048, 64-frame square-wave period.
    # NTSC AI clock / (1520 + 1) = 32006 Hz, giving approximately 500.1 Hz.
    load(8, 0xA0200000)
    load(9, 1024)
    label("pcm")
    load(10, 0x08000800)
    imm(12, 11, 9, 32)
    branch(4, 11, 0, "pcm_store")
    load(10, 0xF800F800)
    label("pcm_store")
    imm(43, 10, 8, 0)
    imm(9, 8, 8, 4)
    imm(9, 9, 9, -1)
    branch(5, 9, 0, "pcm")

    load(16, 0xA4500000)          # s0: AI registers
    for offset, value in ((0x08, 1), (0x10, 1520), (0x14, 15)):
        load(9, value)
        imm(43, 9, 16, offset)
    load(21, 0x00200000)          # s5: physical PCM buffer
    load(22, 4096)                # s6: PCM buffer bytes

    # Joybus command: dummy FF, Tx=1, Rx=4, command 01 (read controller).
    # Align its four-byte response to RDRAM 0x604 for an ordinary word load.
    load(19, 0xA0000600)          # s3: 64-byte SI DMA buffer
    for offset in range(0, 64, 4):
        imm(43, 0, 19, offset)
    for offset, value in ((0, 0xFF010401), (8, 0xFE000000), (60, 1)):
        load(9, value)
        imm(43, 9, 19, offset)
    load(18, 0xA4800000)          # s2: SI registers
    load(20, 0x1FC007C0)          # s4: PIF RAM physical address
    load(9, 0x600)
    imm(43, 9, 18, 0)            # SI_DRAM_ADDR
    imm(43, 20, 18, 0x10)        # SI_PIF_ADDR_WR64B: format channel 1
    wait_si("si_setup")

    load(17, 0xA0000400)          # s1: diagnostic RAM words
    load(9, 0x52324E36)             # "R2N6": independent CPU execution oracle
    imm(43, 9, 17, 0)
    load(23, 0)                  # s7: incrementing counter for state restore
    label("main")
    imm(9, 23, 23, 1)
    imm(43, 23, 17, 4)

    # Keep the two-entry AI FIFO fed without overwriting a full queue.
    imm(35, 9, 16, 0x0C)
    shift(9, 9, 31, right=True)
    branch(5, 9, 0, "input")
    imm(43, 21, 16, 0)
    imm(43, 22, 16, 4)
    imm(43, 0, 16, 0x0C)         # acknowledge AI interrupt

    label("input")
    imm(43, 20, 18, 4)           # SI_PIF_ADDR_RD64B: perform Joybus read
    wait_si("si_read")
    imm(35, 11, 19, 4)
    imm(43, 11, 17, 8)           # buttons[31:16], signed stick X/Y[15:0]

    # Visible activity marker at (16,16): black idle, white on input.
    load(10, 0x0001)
    branch(4, 11, 0, "indicator")
    load(10, 0xFFFF)
    label("indicator")
    load(8, 0xA0100000 + (16 * 320 + 16) * 2)
    load(12, 16)
    label("indicator_row")
    load(9, 16)
    label("indicator_pixel")
    imm(41, 10, 8, 0)
    imm(9, 8, 8, 2)
    imm(9, 9, 9, -1)
    branch(5, 9, 0, "indicator_pixel")
    imm(9, 8, 8, (320 - 16) * 2)
    imm(9, 12, 12, -1)
    branch(5, 12, 0, "indicator_row")
    branch(4, 0, 0, "main")

    for index, target in fixups:
        distance = labels[target] - index - 1
        assert -32768 <= distance <= 32767
        words[index] = (words[index] & 0xFFFF0000) | (distance & 0xFFFF)

    rom = bytearray(4096)
    struct.pack_into(">4I", rom, 0, 0x80371240, 0xF, 0xA4000040, 0)
    rom[0x20:0x34] = b"R2N64 DIAGNOSTIC".ljust(20, b" ")
    rom[0x3B:0x3F] = b"NR2E"
    code = struct.pack(">" + "I" * len(words), *words)
    assert len(code) <= 0xFC0
    rom[0x40:0x40 + len(code)] = code
    return rom


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(diagnostic_rom())
    print(f"Original synthetic N64 diagnostic: {args.output}")
