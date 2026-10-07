#!/usr/bin/env python3
"""Original NES/SNES CPU/PPU/APU/controller/SRAM programs, generated only in build/.

No game assets or external firmware. NES NROM/MMC3 and SNES LoROM code below is
authored for this test. SNES uploads our own SPC700 tone program and BRR wave.
SRAM: bytes0..3=R2NE/R2SN,4=bootcount,5=framecount,6..7=controller registers.
Byte256 is untouched for persistence testing. See docs/NES-SNES-V040.md.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct


class Code:
    def __init__(self, base=0x8000):
        self.base, self.data, self.labels, self.fixups = base, bytearray(), {}, []

    def emit(self, *values):
        self.data.extend(values)

    def label(self, name):
        self.labels[name] = self.base + len(self.data)

    def absolute(self, opcode, address):
        self.emit(opcode, address & 255, address >> 8)

    def store(self, address, value):
        self.emit(0xa9, value)
        self.absolute(0x8d, address)

    def branch(self, opcode, name):
        self.emit(opcode, 0)
        self.fixups.append((len(self.data)-1, name, True))

    def jump(self, name):
        self.emit(0x4c, 0, 0)
        self.fixups.append((len(self.data)-2, name, False))

    def finish(self):
        for offset, name, relative in self.fixups:
            value = self.labels[name]
            if relative:
                value -= self.base + offset + 1
                assert -128 <= value <= 127, (name, value)
                self.data[offset] = value & 255
            else:
                struct.pack_into('<H', self.data, offset, value)
        return self.data


def nes(mmc3=False, pal=False):
    # MMC3 executes from its fixed final 8 KiB bank; switching PRG cannot replace
    # our program. Its 256 KiB PRG/128 KiB CHR sizes exercise high bank bits too.
    p = Code(0xe000 if mmc3 else 0x8000)
    p.emit(0x78, 0xd8, 0xa2, 0xff, 0x9a)  # SEI CLD LDX #FF TXS
    for address, value in ((0x2000,0),(0x2001,0),(0x4010,0),(0x4017,0x40)):
        p.store(address,value)
    if mmc3:
        p.store(0xe000,0)  # IRQ acknowledge/disable
        p.store(0xa001,0x80)  # WRAM enabled, writable
        for i in range(8,16): p.store(0x6000+i,0)
        # Store actual readbacks rather than unconditional success flags.
        for mode,bank,address,result in ((6,2,0x8000,8), (0x46,17,0xc000,9),
                                          (0x46,17,0x8000,10), (7,9,0xa000,11)):
            p.store(0x8000,mode); p.store(0x8001,bank)
            p.absolute(0xad,address); p.absolute(0x8d,0x6000+result)
    for phase in (1,2):
        p.label(f'warm{phase}')
        p.absolute(0x2c,0x2002)  # BIT PPUSTATUS
        p.branch(0x10,f'warm{phase}')
    # Actual CPU RAM alias readbacks, separate from MMC3's status bytes.
    p.store(0x0345,0x5a)
    for slot,address in enumerate((0x0b45,0x1345,0x1b45)):
        p.absolute(0xad,address); p.absolute(0x8d,0x6010+slot)
    if mmc3:
        # Test CHR banking with and without the pattern-table inversion bit.
        for mode,bank,address,result in ((0,8,0,12), (0x80,14,0x1000,13)):
            p.store(0x8000,mode); p.store(0x8001,bank)
            p.absolute(0x2c,0x2002)
            p.store(0x2006,address >> 8); p.store(0x2006,0)
            p.absolute(0xad,0x2007)  # buffered read, discard
            p.absolute(0xad,0x2007); p.absolute(0x8d,0x6000+result)
        for register,bank in enumerate((0,2,4,5,6,7,0,1)):
            p.store(0x8000,register); p.store(0x8001,bank)
        # Vertical mirroring: $2800 must alias $2000.
        p.store(0xa000,0)
        p.store(0x2006,0x20); p.store(0x2006,0); p.store(0x2007,0x63)
        p.store(0x2006,0x28); p.store(0x2006,0)
        p.absolute(0xad,0x2007); p.absolute(0xad,0x2007); p.absolute(0x8d,0x600e)
    p.store(0x2006,0x3f); p.store(0x2006,0)
    for shade in (0x0f,0x16,0x2a,0x12)*8:
        p.store(0x2007,shade)
    p.store(0x2006,0x20); p.store(0x2006,0)
    p.emit(0xa2,0,0xa0,4)
    p.label('names')
    p.emit(0x8a,0x29,3)  # TXA AND #3
    if mmc3: p.emit(0x18,0x69,1)  # tiles1..4; leave each CHR bank marker unused
    p.absolute(0x8d,0x2007)
    p.emit(0xe8); p.branch(0xd0,'names')
    p.emit(0x88); p.branch(0xd0,'names')
    # All attributes select palette0.
    p.store(0x2006,0x23); p.store(0x2006,0xc0)
    p.emit(0xa2,64,0xa9,0)
    p.label('attributes'); p.absolute(0x8d,0x2007)
    p.emit(0xca); p.branch(0xd0,'attributes')
    for address,value in ((0x2005,0),(0x2005,0),(0x4015,1),
                          (0x4000,0xbf),(0x4001,0),(0x4002,0xfd),(0x4003,0x08)):
        p.store(address,value)
    for i,value in enumerate(b'R2NE'): p.store(0x6000+i,value)
    p.absolute(0xee,0x6004)  # INC persistent boot count
    p.store(0x6005,0); p.store(0x6007,0)
    if mmc3:
        p.store(0x2000,0x10)  # background table high, sprite table low: A12 clocks
        p.emit(0x58)  # CLI: mapper IRQ handler below, NMI stays disabled
    p.store(0x2001,0x0a)  # background, including left edge
    p.label('vblank')
    p.absolute(0x2c,0x2002); p.branch(0x10,'vblank')
    p.absolute(0xee,0x6005)
    if mmc3:
        p.store(0xc000,80); p.store(0xc001,0); p.store(0xe001,0)
    p.store(0x4016,1); p.store(0x4016,0)
    p.store(0x6006,0); p.emit(0xa2,8)
    p.label('controller')
    p.absolute(0xad,0x4016); p.emit(0x4a)  # serial bit -> carry
    p.absolute(0x2e,0x6006)  # ROL A B Select Start Up Down Left Right
    p.emit(0xca); p.branch(0xd0,'controller')
    p.store(0x2006,0x3f); p.store(0x2006,0)
    p.absolute(0xad,0x6006); p.emit(0x4a,0x4a,0x29,0x3f,0x09,0x10)
    p.absolute(0x8d,0x2007)
    if mmc3:
        # PPUADDR palette writes also changed temporary nametable bits.
        # Restore table0 before PPUSCROLL instead of showing uninitialized RAM.
        p.store(0x2000,0x10)
    p.store(0x2005,0); p.store(0x2005,0)
    p.jump('vblank')
    p.label('interrupt'); p.emit(0x40)
    if mmc3:
        p.label('irq')
        p.emit(0x48)  # PHA; preserve interrupted palette/controller accumulator
        p.absolute(0xee,0x600f)
        p.store(0xe000,0)  # acknowledge and wait for the next frame's rearm
        p.emit(0x68,0x40)  # PLA RTI
    program = p.finish()
    if mmc3:
        assert len(program) < 0x1ffa
        prg = bytearray([0xea]) * (256 * 1024)
        for bank in range(32): prg[bank*8192] = 0x40 + bank
        prg[-8192:-8192+len(program)] = program
        struct.pack_into('<3H',prg,len(prg)-6,p.labels['interrupt'],0xe000,p.labels['irq'])
        chr_rom = bytearray(128 * 1024)
        for bank in range(128):
            start = bank*1024
            chr_rom[start] = bank
            for tile in range(1,5):
                chr_rom[start+tile*16:start+tile*16+8] = bytes([255 if tile&1 else 0])*8
                chr_rom[start+tile*16+8:start+tile*16+16] = bytes([255 if tile&2 else 0])*8
        return b'NES\x1a'+bytes([16,16,0x43,0,1,int(pal),0,0,0,0,0,0])+prg+chr_rom
    prg = bytearray([0xea])*32768
    prg[:len(program)] = program
    struct.pack_into('<3H',prg,0x7ffa,p.labels['interrupt'],0x8000,p.labels['interrupt'])
    chr_rom = bytearray(8192)
    for tile in range(4):
        chr_rom[tile*16:tile*16+8] = bytes([255 if tile&1 else 0])*8
        chr_rom[tile*16+8:tile*16+16] = bytes([255 if tile&2 else 0])*8
    return b'NES\x1a'+bytes([2,1,2,0,1,int(pal),0,0,0,0,0,0])+prg+chr_rom


def spc_payload():
    payload = bytearray(192)
    struct.pack_into('<HH',payload,0,0x0220,0x0220)
    payload[0x20:0x29] = bytes([0xc3,0x77,0x77,0x77,0x77,0x99,0x99,0x99,0x99])
    code = bytearray([0x8f,0,0xf1])  # MOV direct,#imm: IPL/timers off
    for register,value in ((0x6c,0x20),(0x5c,0),(0x0c,0x60),(0x1c,0x60),
                           (0x5d,2),(0x00,0x50),(0x01,0x50),(0x02,0),
                           (0x03,4),(0x04,0),(0x05,0),(0x07,0x7f),(0x4c,1)):
        code.extend((0x8f,register,0xf2,0x8f,value,0xf3))
    code.extend((0x2f,0xfe))  # BRA to itself; DSP runs autonomously
    payload[0x40:0x40+len(code)] = code
    return payload


def snes(pal=False):
    p = Code()
    p.emit(0x78,0xd8,0x18,0xfb,0xc2,0x10,0xa2,0xff,0x1f,0x9a)  # Native, A8/X16
    for address,value in ((0x4200,0),(0x420c,0),(0x2100,0x80),
                          (0x2105,0),(0x212c,0),(0x2130,0),(0x2131,0),(0x2133,0)):
        p.store(address,value)
    # Communicate via the hardware SPC upload protocol; no external SPC image.
    p.label('apu-ready'); p.absolute(0xad,0x2140); p.emit(0xc9,0xaa)
    p.branch(0xd0,'apu-ready')
    p.store(0x2142,0); p.store(0x2143,2); p.store(0x2141,1); p.store(0x2140,0xcc)
    p.label('apu-command'); p.absolute(0xad,0x2140); p.emit(0xc9,0xcc)
    p.branch(0xd0,'apu-command')
    p.emit(0xe2,0x10,0xa2,0)  # X8 for transfer counter
    p.label('apu-copy')
    p.absolute(0xbd,0x8800)  # LDA data,X
    p.absolute(0x8d,0x2141); p.absolute(0x8e,0x2140)
    p.label('apu-ack'); p.absolute(0xec,0x2140); p.branch(0xd0,'apu-ack')
    p.emit(0xe8,0xe0,192); p.branch(0xd0,'apu-copy')
    p.store(0x2142,0x40); p.store(0x2143,2); p.store(0x2141,0); p.store(0x2140,193)
    # Real DMA from ROM to WRAM via $2180, followed by CPU readback. The data
    # is also our original 4bpp checker tile; no commercial graphics involved.
    for address,value in ((0x2181,0),(0x2182,0x1f),(0x2183,0),
                          (0x4300,0),(0x4301,0x80),(0x4302,0),(0x4303,0x89),
                          (0x4304,0),(0x4305,4),(0x4306,0),(0x420b,1)):
        p.store(address,value)
    for i in range(4):
        p.emit(0xaf,i,0x1f,0x7e,0x8f,8+i,0,0x70)
    p.store(0x0345,0x5a)
    p.emit(0xaf,0x45,3,0x7e,0x8f,12,0,0x70)
    p.emit(0xa9,0xa5,0x8f,0x46,3,0x7e)
    p.absolute(0xad,0x0346); p.emit(0x8f,13,0,0x70)
    # Forced blank stays enabled while DMA writes a tile and the CPU clears
    # the full BG1 tilemap. Do not rely on emulator-initialized VRAM contents.
    for address,value in ((0x2115,0x80),(0x2116,0),(0x2117,0),
                          (0x4300,1),(0x4301,0x18),(0x4302,0),(0x4303,0x89),
                          (0x4305,32),(0x4306,0),(0x420b,1),
                          (0x2116,0),(0x2117,4)):
        p.store(address,value)
    p.emit(0xc2,0x10,0xa2,0,4,0xa9,0)  # X16, 1024 tilemap words
    p.label('tilemap')
    p.absolute(0x8d,0x2118); p.absolute(0x8d,0x2119)
    p.emit(0xca); p.branch(0xd0,'tilemap')
    p.store(0x2121,1)
    for byte in (0x1f,0,0xe0,3): p.store(0x2122,byte)
    for address,value in ((0x2105,1),(0x2107,4),(0x210b,0),
                          (0x210d,0),(0x210d,0),(0x210e,0),(0x210e,0),(0x212c,1)):
        p.store(address,value)
    for i,value in enumerate(b'R2SN'):
        p.emit(0xa9,value,0x8f,i,0,0x70)  # long SRAM bank70
    p.emit(0xaf,4,0,0x70,0x1a,0x8f,4,0,0x70)
    p.emit(0xa9,0,0x8f,5,0,0x70)
    p.store(0x4200,1)  # auto joypad, no NMI
    p.store(0x2121,0); p.store(0x2122,0x1f); p.store(0x2122,0)
    p.store(0x2100,0x0f)
    p.label('visible'); p.absolute(0xad,0x4212); p.branch(0x30,'visible')
    p.label('vblank'); p.absolute(0xad,0x4212); p.branch(0x10,'vblank')
    # VBlank can begin before auto-read sets busy. Observe its start AND end,
    # otherwise the diagnostic can read the previous controller report.
    p.label('joy-start'); p.absolute(0xad,0x4212); p.emit(0x29,1); p.branch(0xf0,'joy-start')
    p.label('joy-busy'); p.absolute(0xad,0x4212); p.emit(0x29,1); p.branch(0xd0,'joy-busy')
    p.emit(0xaf,5,0,0x70,0x1a,0x8f,5,0,0x70)
    p.absolute(0xad,0x4219); p.emit(0x8f,6,0,0x70)
    p.absolute(0xad,0x4218); p.emit(0x8f,7,0,0x70)
    p.store(0x2121,0)
    p.absolute(0xad,0x4218); p.emit(0x09,0x1f); p.absolute(0x8d,0x2122)
    p.absolute(0xad,0x4219); p.emit(0x29,0x7f); p.absolute(0x8d,0x2122)
    p.jump('visible')
    p.label('interrupt'); p.emit(0x40)
    program = p.finish()
    assert len(program) < 0x800
    rom = bytearray([0xea])*32768
    rom[:len(program)] = program
    rom[0x800:0x800+192] = spc_payload()
    rom[0x900:0x920] = bytes([0xaa,0x55]*8 + [0]*16)
    rom[0x7fc0:0x7fd5] = b'R2N64 SNES DIAGNOSTIC'
    rom[0x7fd5:0x7fdc] = bytes([0x20,0x02,5,3,2 if pal else 1,0x33,0])  # LoROM battery8K
    rom[0x7fdc:0x7fe0] = bytes(4)
    for vector in range(0x7fe0,0x8000,2): struct.pack_into('<H',rom,vector,p.labels['interrupt'])
    struct.pack_into('<H',rom,0x7ffc,0x8000)
    checksum = (sum(rom)+510)&0xffff
    struct.pack_into('<HH',rom,0x7fdc,checksum^0xffff,checksum)
    assert len(rom) == 32768
    return rom


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    files = {'diagnostic.nes':nes(),'diagnostic-mmc3.nes':nes(mmc3=True),'diagnostic.sfc':snes()}
    files['diagnostic.smc'] = bytes(512)+files['diagnostic.sfc']
    files.update({'diagnostic-pal.nes':nes(pal=True),
                  'diagnostic-mmc3-pal.nes':nes(mmc3=True,pal=True),
                  'diagnostic-pal.sfc':snes(pal=True)})
    manifest = {}
    for name,content in files.items():
        (args.output/name).write_bytes(content)
        manifest[name] = {'bytes':len(content),'sha256':hashlib.sha256(content).hexdigest()}
    (args.output/'original-fixtures.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Original NES/SNES diagnostics:',args.output)


if __name__ == '__main__':
    main()
