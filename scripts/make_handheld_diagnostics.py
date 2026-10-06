#!/usr/bin/env python3
"""Generate original GB/GBC/GBA integration fixtures without external assets.

No Nintendo logo, commercial game code or BIOS is present. GB/GBC intentionally
use the core's bundled, freely licensed SameBoy bootstrap; GBA uses mGBA HLE.
The programs draw a pattern, synthesize a PSG tone and poll physical registers.
SRAM[0:4] = R2GB/R2GC/R2GA; [4] boot count; [5] frame counter; [6:8] inputs.
SRAM[256] is reserved for the persistence test and never written by the program.
Outputs are build fixtures only and are not copied into the application package.
"""
from pathlib import Path
import argparse
import hashlib
import struct


class SM83:
    def __init__(self):
        self.code = bytearray()
        self.labels = {}
        self.fixups = []

    def emit(self, *data):
        self.code.extend(data)

    def label(self, name):
        self.labels[name] = 0x150 + len(self.code)

    def jump(self, name, opcode=0xc3):
        self.emit(opcode, 0, 0)
        self.fixups.append((len(self.code) - 2, name))

    def store(self, address, value):
        self.emit(0x3e, value, 0xea, address & 255, address >> 8)

    def io(self, address, value):
        self.emit(0x3e, value, 0xe0, address)

    def finish(self):
        for position, name in self.fixups:
            struct.pack_into("<H", self.code, position, self.labels[name])
        return self.code


def gameboy(color):
    p = SM83()
    p.emit(0xf3, 0x31, 0xfe, 0xdf)  # DI; SP=DFFE
    p.label("initial-vblank")
    p.emit(0xf0, 0x44, 0xfe, 144)
    p.jump("initial-vblank", 0xda)  # LY < 144
    p.io(0x40, 0)  # LCD off before VRAM writes
    p.io(0x4f, 0)  # CGB VRAM bank 0 (ignored on DMG)
    p.emit(0x21, 0, 0x80)  # HL = tile data
    for shade in range(4):
        for _ in range(8):
            for bit in range(2):
                p.emit(0x3e, 255 if shade & (1 << bit) else 0, 0x22)
    p.emit(0x21, 0, 0x98, 0x01, 0, 4)  # 1024 tilemap cells
    p.label("tilemap")
    p.emit(0x7d, 0xe6, 3, 0x22, 0x0b, 0x78, 0xb1)
    p.jump("tilemap", 0xc2)
    if color:
        p.io(0x68, 0x80)
        for value in struct.pack("<4H", 0x7fff, 0x001f, 0x03e0, 0x7c00):
            p.io(0x69, value)
    p.io(0x47, 0xe4)
    p.io(0x42, 0)
    p.io(0x43, 0)
    p.io(0x26, 0x80)
    p.io(0x24, 0x77)
    p.io(0x25, 0x11)
    p.io(0x10, 0)
    p.io(0x11, 0x80)
    p.io(0x12, 0xf0)
    p.io(0x13, 0)
    p.io(0x14, 0x87)
    p.store(0x0000, 0x0a)  # Enable MBC1 cartridge RAM
    for i, value in enumerate(b"R2GC" if color else b"R2GB"):
        p.store(0xa000 + i, value)
    p.emit(0xfa, 4, 0xa0, 0x3c, 0xea, 4, 0xa0)  # Persistent boot count
    p.store(0xc000, 0)
    p.store(0xa006, 0)
    p.store(0xa007, 0)
    p.io(0x40, 0x91)  # LCD on, unsigned tiles, BG enabled
    p.label("visible")
    p.emit(0xf0, 0x44, 0xfe, 144)
    p.jump("visible", 0xd2)
    p.label("vblank")
    p.emit(0xf0, 0x44, 0xfe, 144)
    p.jump("vblank", 0xda)
    p.emit(0xfa, 0, 0xc0, 0x3c, 0xea, 0, 0xc0, 0xea, 5, 0xa0, 0xe0, 0x43)
    for selection, destination in ((0x10, 6), (0x20, 7)):
        p.io(0, selection)
        p.emit(0xf0, 0, 0xf0, 0, 0xf0, 0, 0x2f, 0xe6, 15, 0xea, destination, 0xa0)
    p.io(0, 0x30)
    p.jump("visible")

    rom = bytearray(32768)
    rom[0x100:0x104] = bytes((0, 0xc3, 0x50, 1))
    title = b"R2 COLOR TEST" if color else b"R2 MONO TEST"
    rom[0x134:0x134 + len(title)] = title
    rom[0x143] = 0x80 if color else 0
    rom[0x147] = 3  # MBC1 + RAM + battery
    rom[0x149] = 2  # 8 KiB RAM
    rom[0x14a] = 1
    rom[0x14d] = (-sum(rom[0x134:0x14d]) - 25) & 255
    code = p.finish()
    rom[0x150:0x150 + len(code)] = code
    struct.pack_into(">H", rom, 0x14e, sum(rom) & 65535)
    assert not any(rom[0x104:0x134])
    return rom


class ARM:
    def __init__(self):
        self.words = []
        self.labels = {}
        self.branches = []
        self.constants = []

    def emit(self, value):
        self.words.append(value)

    def label(self, name):
        self.labels[name] = len(self.words) * 4

    def branch(self, name, condition=14):
        self.branches.append((len(self.words), name))
        self.emit(condition << 28 | 0x0a000000)

    def constant(self, register, value):
        self.constants.append((len(self.words), value))
        self.emit(0xe59f0000 | register << 12)

    def mov(self, register, byte):
        assert 0 <= byte <= 255
        self.emit(0xe3a00000 | register << 12 | byte)

    def strh(self, source, base, offset=0):
        assert 0 <= offset < 256
        self.emit(0xe1c000b0 | base << 16 | source << 12 | (offset & 240) << 4 | (offset & 15))

    def ldrh(self, target, base, offset=0):
        assert 0 <= offset < 256
        self.emit(0xe1d000b0 | base << 16 | target << 12 | (offset & 240) << 4 | (offset & 15))

    def store_byte(self, source, base, offset=0):
        self.emit(0xe5c00000 | base << 16 | source << 12 | offset)

    def register(self, offset, value):
        self.constant(1, value)
        self.strh(1, 0, offset)

    def finish(self):
        for position, name in self.branches:
            distance = (self.labels[name] - (position * 4 + 8)) // 4
            self.words[position] |= distance & 0xffffff
        for position, value in self.constants:
            distance = len(self.words) * 4 - (position * 4 + 8)
            assert 0 <= distance < 4096
            self.words[position] |= distance
            self.words.append(value)
        return b"".join(struct.pack("<I", word) for word in self.words)


def gameboy_advance():
    p = ARM()
    p.constant(0, 0x04000000)
    p.register(0, 0x0403)  # Mode 3, BG2
    p.register(0x84, 0x80)  # PSG master
    p.register(0x80, 0x1177)  # Channel 1 stereo
    p.register(0x82, 2)  # PSG full output
    p.register(0x60, 0)
    p.register(0x62, 0xf080)  # Fixed volume, 50% duty
    p.register(0x64, 0x8400)  # Trigger 128 Hz PSG pulse
    p.constant(2, 0x06000000)
    for number, (color, count) in enumerate(((0x001f, 240 * 53), (0x03e0, 240 * 53), (0x7c00, 240 * 54))):
        p.constant(3, count)
        p.constant(4, color)
        p.label(f"band-{number}")
        p.strh(4, 2)
        p.emit(0xe2822002)  # ADD r2,r2,#2
        p.emit(0xe2533001)  # SUBS r3,r3,#1
        p.branch(f"band-{number}", 1)
    p.constant(7, 0x0e000000)
    for i, byte in enumerate(b"R2GA"):
        p.mov(1, byte)
        p.store_byte(1, 7, i)
    p.emit(0xe5d71004,  )  # LDRB r1,[r7,#4]
    p.emit(0xe2811001)  # Boot count
    p.store_byte(1, 7, 4)
    p.mov(8, 0)
    p.constant(9, 0x04000130)
    p.constant(10, 0x06000000)
    p.label("visible")
    p.ldrh(1, 0, 6)
    p.emit(0xe35100a0)  # CMP r1,#160
    p.branch("visible", 10)  # BGE
    p.label("vblank")
    p.ldrh(1, 0, 6)
    p.emit(0xe35100a0)
    p.branch("vblank", 11)  # BLT
    p.emit(0xe2888001)  # ADD r8,r8,#1
    p.store_byte(8, 7, 5)
    p.ldrh(1, 9)
    p.emit(0xe1e01001)  # MVN r1,r1: input becomes active high
    p.store_byte(1, 7, 6)
    p.emit(0xe1a02421)  # MOV r2,r1,LSR #8
    p.emit(0xe2022003)  # AND r2,r2,#3
    p.store_byte(2, 7, 7)
    p.emit(0xe208201f)  # AND r2,r8,#31
    p.strh(2, 10)  # Animated red pixel demonstrates restored program progress
    p.branch("visible")
    code = p.finish()
    rom = bytearray(32768)
    struct.pack_into("<I", rom, 0, 0xea00002e)  # ARM B 0x080000c0
    rom[0xa0:0xac] = b"R2 GBA TEST "
    rom[0xac:0xb0] = b"R2AT"
    rom[0xb2] = 0x96
    rom[0xbd] = (-sum(rom[0xa0:0xbd]) - 0x19) & 255
    rom[0xc0:0xc0 + len(code)] = code
    rom[0x1000:0x1000 + 10] = b"SRAM_V113\0"  # Public save-type tag consumed by mGBA
    assert not any(rom[4:0xa0])
    return rom


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", nargs="?", default="build/handheld-fixtures", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for name, rom in (("diagnostic.gb", gameboy(False)), ("diagnostic.gbc", gameboy(True)),
                      ("diagnostic.gba", gameboy_advance())):
        path = args.output / name
        path.write_bytes(rom)
        print(f"{path}: {len(rom)} bytes, SHA256 {hashlib.sha256(rom).hexdigest()}")


if __name__ == "__main__":
    main()
