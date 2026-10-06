#!/usr/bin/env python3
"""Read the user's verified Pokemon Red cartridge; export one research map.

This is an original binary-format reader, not game/engine code. It reads no
bundled ROM, graphics, or third-party assets. Generated cartridge-derived data
must stay under this repository's ignored build/ directory.

Format references (not vendored): pret/pokered macros/scripts/maps.asm,
data/tilesets/tileset_headers.asm and its generated symbols branch.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


ROOT = Path(__file__).resolve().parents[1]
ROM_SHA1 = "ea9bcae617fdf159b045185467ae58b2e4a48b9a"
ROM_SHA256 = "5ca7ba01642a3b27b0cc0b5349b52792795b62d3ed977e98a09390659af96b7b"
SYMBOLS_URL = "https://raw.githubusercontent.com/pret/pokered/3f618d59edf43918f48f5e558c34e04cb2fc5619/pokered.sym"
SYMBOLS_SHA256 = "cb30d0cc5e05875ceda3e2f2abcf55245afe2dc0fe2bf841b042b473c4a7dfb3"
FORMAT_REFERENCE = "https://github.com/pret/pokered/tree/d2704a63c26f9ba046ade877445216b3de0519a4"
SCHEMA = "r2n64.pokemon-red.map.v1"


class MapError(ValueError):
    pass


def parse_symbols(text):
    symbols = {}
    for line in text.splitlines():
        clean = line.split(";", 1)[0].strip()
        if not clean:
            continue
        # Current RGBDS also writes exported constants without a bank/address.
        # They are not ROM locations and must never be interpreted as pointers.
        if re.fullmatch(r"[0-9a-fA-F]+\s+\S+", clean):
            continue
        match = re.fullmatch(r"([0-9a-fA-F]+):([0-9a-fA-F]{4})\s+(\S+)", clean)
        if not match:
            raise MapError("Malformed symbol line")
        bank, address = int(match[1], 16), int(match[2], 16)
        if bank > 0xFFFF:
            raise MapError("Symbol bank is out of range")
        name = match[3]
        if name in symbols and symbols[name] != (bank, address):
            raise MapError("Conflicting symbol: " + name)
        symbols[name] = (bank, address)
    return symbols


def rom_offset(bank, address):
    if not 0 <= bank < 64:
        raise MapError("ROM bank is outside the supported cartridge")
    if 0 <= address < 0x4000:
        if bank != 0:
            raise MapError("Fixed-ROM address must use bank zero")
        return address
    if 0x4000 <= address < 0x8000 and bank:
        return bank * 0x4000 + address - 0x4000
    raise MapError("Address is not in the ROM address space")


def resolve(symbols, name):
    if name not in symbols:
        raise MapError("Missing symbol: " + name)
    return rom_offset(*symbols[name])


def checked_bytes(rom, offset, size):
    if size < 0 or offset < 0 or offset + size > len(rom):
        raise MapError("ROM read is out of bounds")
    if size and offset // 0x4000 != (offset + size - 1) // 0x4000:
        raise MapError("ROM structure crosses a bank boundary")
    return rom[offset:offset + size]


def u16(rom, offset):
    return struct.unpack("<H", checked_bytes(rom, offset, 2))[0]


def bank_pointer(bank, address):
    return rom_offset(0 if address < 0x4000 else bank, address)


def decode_tile(data):
    if len(data) != 16:
        raise MapError("A 2bpp tile must contain exactly 16 bytes")
    return [((data[y * 2] >> (7 - x)) & 1) |
            (((data[y * 2 + 1] >> (7 - x)) & 1) << 1)
            for y in range(8) for x in range(8)]


def verify_rom(rom):
    if len(rom) != 1024 * 1024:
        raise MapError("Only the verified 1 MiB Pokemon Red UE cartridge is supported")
    if hashlib.sha256(rom).hexdigest() != ROM_SHA256 or hashlib.sha1(rom).hexdigest() != ROM_SHA1:
        raise MapError("ROM revision/hash is not the supported Pokemon Red UE image")
    if ((-sum(rom[0x134:0x14D]) - 25) & 255) != rom[0x14D]:
        raise MapError("Invalid cartridge header checksum")
    if ((sum(rom) - rom[0x14E] - rom[0x14F]) & 65535) != int.from_bytes(rom[0x14E:0x150], "big"):
        raise MapError("Invalid cartridge global checksum")


MAPS = {
    0: ("PalletTown", 10, 9, 0, "Overworld"),
    37: ("RedsHouse1F", 4, 4, 1, "RedsHouse1"),
    38: ("RedsHouse2F", 4, 4, 4, "RedsHouse2"),
    40: ("OaksLab", 5, 6, 5, "Gym"),
}


def extract_pallet(rom, symbols):
    """Compatibility entry point for the original Pallet-only tools."""
    return extract_map(rom, symbols, 0)


def extract_map(rom, symbols, map_id):
    """Parse structural data; CLI additionally enforces the exact ROM hash.

    The separate structural entry point permits original synthetic fixtures.
    No other map, version or unknown tileset is silently guessed.
    """
    if type(map_id) is not int or map_id not in MAPS:
        raise MapError("Unsupported map; only Pallet, Red's house and Oak's lab are understood")
    name, expected_width, expected_height, expected_tileset, tileset_name = MAPS[map_id]
    pointer_table = resolve(symbols, "MapHeaderPointers") + map_id * 2
    bank_table = resolve(symbols, "MapHeaderBanks") + map_id
    bank = checked_bytes(rom, bank_table, 1)[0]
    header = bank_pointer(bank, u16(rom, pointer_table))
    if header != resolve(symbols, name + "_h"):
        raise MapError("Map table and symbol disagree")
    fixed = checked_bytes(rom, header, 10)
    tileset_id, height, width = fixed[:3]
    if (width, height) != (expected_width, expected_height):
        raise MapError("Map dimensions do not match the supported revision")
    if tileset_id != expected_tileset or fixed[9] & 0xF0:
        raise MapError("Unexpected tileset or map connection flags")
    blocks_offset = bank_pointer(bank, u16(rom, header + 3))
    if blocks_offset != resolve(symbols, name + "_Blocks"):
        raise MapError("Map block pointer and symbol disagree")
    for position in (5, 7):
        checked_bytes(rom, bank_pointer(bank, u16(rom, header + position)), 1)
    block_bytes = checked_bytes(rom, blocks_offset, width * height)
    connections = []
    cursor = header + 10
    for direction, bit in (("north", 8), ("south", 4), ("west", 2), ("east", 1)):
        if not fixed[9] & bit:
            continue
        entry = checked_bytes(rom, cursor, 11)
        connections.append({"direction": direction, "map_id": entry[0],
                            "source_address": int.from_bytes(entry[1:3], "little"),
                            "destination_address": int.from_bytes(entry[3:5], "little"),
                            "strip_length": entry[5], "map_width": entry[6],
                            "y_alignment_raw": entry[7], "x_alignment_raw": entry[8]})
        cursor += 11
    objects_offset = bank_pointer(bank, u16(rom, cursor))
    border_block = checked_bytes(rom, objects_offset, 1)[0]
    cursor = objects_offset + 1

    def records(length, maximum):
        nonlocal cursor
        count = checked_bytes(rom, cursor, 1)[0]
        cursor += 1
        if count > maximum:
            raise MapError("Map event count exceeds the supported bounds")
        data = checked_bytes(rom, cursor, count * length)
        cursor += count * length
        return [data[i * length:(i + 1) * length] for i in range(count)]

    warps = [{"x": e[1], "y": e[0], "destination_warp": e[2], "destination_map": e[3]}
             for e in records(4, 32)]
    signs = [{"x": e[1], "y": e[0], "text_id": e[2]} for e in records(3, 32)]
    count = checked_bytes(rom, cursor, 1)[0]
    cursor += 1
    if count > 15:
        raise MapError("Too many map objects")
    objects = []
    for _ in range(count):
        e = checked_bytes(rom, cursor, 6)
        cursor += 6
        if e[5] & 0xC0 == 0xC0:
            raise MapError("Ambiguous trainer/item flags")
        obj = {"sprite_id": e[0], "x": e[2] - 4, "y": e[1] - 4,
               "movement": e[3], "range_or_direction": e[4], "text_id": e[5] & 0x3F,
               "kind": "trainer" if e[5] & 0x40 else "item" if e[5] & 0x80 else "person",
               "visibility": "static ROM definition; live flags not evaluated"}
        extra = 2 if obj["kind"] == "trainer" else 1 if obj["kind"] == "item" else 0
        if extra:
            obj["parameters"] = list(checked_bytes(rom, cursor, extra))
            cursor += extra
        objects.append(obj)
    for item in warps + signs + objects:
        if not (0 <= item["x"] < width * 2 and 0 <= item["y"] < height * 2):
            raise MapError("Map event coordinates are out of bounds")

    tileset_offset = resolve(symbols, "Tilesets") + tileset_id * 12
    tileset = checked_bytes(rom, tileset_offset, 12)
    tile_bank = tileset[0]
    tile_blocks = bank_pointer(tile_bank, u16(rom, tileset_offset + 1))
    tile_gfx = bank_pointer(tile_bank, u16(rom, tileset_offset + 3))
    collision = bank_pointer(tile_bank, u16(rom, tileset_offset + 5))
    if tile_blocks != resolve(symbols, tileset_name + "_Block") or tile_gfx != resolve(symbols, tileset_name + "_GFX"):
        raise MapError("Tileset pointers and symbols disagree")
    # The labelled end of the graphics is also the start of block definitions.
    # Require a whole number of tiles, and do not interpret block bytes as art.
    gfx_bytes = tile_blocks - tile_gfx
    if gfx_bytes <= 0 or gfx_bytes % 16 or gfx_bytes > 256 * 16:
        raise MapError("Invalid graphics extent")
    checked_bytes(rom, tile_gfx, gfx_bytes)
    available_tiles = gfx_bytes // 16
    collision_tiles = []
    for i in range(256):
        value = checked_bytes(rom, collision + i, 1)[0]
        if value == 255:
            break
        collision_tiles.append(value)
    else:
        raise MapError("Unterminated collision list")
    tile_rows = [[0] * (width * 4) for _ in range(height * 4)]
    block_rows = [list(block_bytes[y * width:(y + 1) * width]) for y in range(height)]
    for by, row in enumerate(block_rows):
        for bx, block in enumerate(row):
            block_data = checked_bytes(rom, tile_blocks + block * 16, 16)
            for ty in range(4):
                tile_rows[by * 4 + ty][bx * 4:bx * 4 + 4] = block_data[ty * 4:ty * 4 + 4]
    used_tiles = sorted({tile for row in tile_rows for tile in row})
    if any(tile >= available_tiles for tile in used_tiles):
        raise MapError("Map references a tile outside the graphics extent")
    pixels = {str(tile): decode_tile(checked_bytes(rom, tile_gfx + tile * 16, 16)) for tile in used_tiles}
    return {"schema": SCHEMA, "map_id": map_id, "name": name,
            "block_width": width, "block_height": height,
            "tile_width": width * 4, "tile_height": height * 4,
            "pixel_width": width * 32, "pixel_height": height * 32,
            "block_ids": block_rows, "tile_ids": tile_rows, "tile_pixels": pixels,
            "border_block": border_block, "warps": warps, "signs": signs, "objects": objects,
            "connections": connections,
            "tileset": {"id": tileset_id, "bank": tile_bank, "blocks_offset": tile_blocks,
                        "gfx_offset": tile_gfx, "tile_count": available_tiles,
                        "collision_tiles": collision_tiles, "grass_tile": tileset[10]},
            "source_offsets": {"header": header, "blocks": blocks_offset, "objects": objects_offset},
            "limitations": ["Static cartridge map, not a live game capture",
                            "Object visibility, motion and script changes are not evaluated",
                            "Pixels are 2bpp indices; no height or material is inferred from brightness"]}


def build_output(path):
    resolved = Path(path).resolve()
    build = (ROOT / "build").resolve()
    if build not in resolved.parents:
        raise MapError("Cartridge-derived output must be inside the repository build/ directory")
    if resolved.suffix.lower() not in (".json", ".png"):
        raise MapError("Output must be a JSON or PNG file")
    return resolved


def write_pngs(document, topdown, atlas):
    try:
        from PIL import Image, ImageDraw
    except ImportError as error:
        raise MapError("PNG requested but Pillow is unavailable; JSON works without it") from error
    gray = (255, 170, 85, 0)
    if topdown:
        im = Image.new("RGB", (document["pixel_width"], document["pixel_height"]))
        destination = im.load()
        for ty, row in enumerate(document["tile_ids"]):
            for tx, tile in enumerate(row):
                pixels = document["tile_pixels"][str(tile)]
                for p, shade in enumerate(pixels):
                    v = gray[shade]
                    destination[tx * 8 + p % 8, ty * 8 + p // 8] = (v, v, v)
        topdown.parent.mkdir(parents=True, exist_ok=True)
        im.save(topdown)
    if atlas:
        ids = sorted(map(int, document["tile_pixels"]))
        sheet = Image.new("RGB", (8 * 76, ((len(ids) + 7) // 8) * 88), "#203040")
        draw = ImageDraw.Draw(sheet)
        for i, tile in enumerate(ids):
            x, y = (i % 8) * 76, (i // 8) * 88
            im = Image.new("RGB", (8, 8))
            im.putdata([(gray[p],) * 3 for p in document["tile_pixels"][str(tile)]])
            sheet.paste(im.resize((64, 64), Image.Resampling.NEAREST), (x + 6, y + 4))
            draw.text((x + 8, y + 70), f"{tile} / {tile:02X}", fill="white")
        atlas.parent.mkdir(parents=True, exist_ok=True)
        sheet.save(atlas)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--symbols", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--png", type=Path)
    parser.add_argument("--atlas", type=Path)
    args = parser.parse_args(argv)
    try:
        output = build_output(args.output)
        if output.suffix.lower() != ".json":
            raise MapError("--output must end in .json")
        images = [build_output(p) if p else None for p in (args.png, args.atlas)]
        if any(p and p.suffix.lower() != ".png" for p in images):
            raise MapError("--png and --atlas must end in .png")
        if args.rom.stat().st_size != 1024 * 1024 or args.symbols.stat().st_size > 4 * 1024 * 1024:
            raise MapError("Unsupported input file size")
        rom = args.rom.read_bytes()
        verify_rom(rom)
        symbols_data = args.symbols.read_bytes()
        if hashlib.sha256(symbols_data).hexdigest() != SYMBOLS_SHA256:
            raise MapError("Symbols do not match the pinned file; download the documented exact URL")
        document = extract_pallet(rom, parse_symbols(symbols_data.decode("utf-8-sig")))
        document["source"] = {"rom_sha1": hashlib.sha1(rom).hexdigest(),
                              "rom_sha256": hashlib.sha256(rom).hexdigest(),
                              "symbols_sha256": hashlib.sha256(symbols_data).hexdigest(),
                              "symbols_url": SYMBOLS_URL, "format_reference": FORMAT_REFERENCE}
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
        if any(images):
            write_pngs(document, *images)
        print(json.dumps({"result": "ok", "output": str(output),
                          "tiles": len(document["tile_pixels"]), "objects": len(document["objects"])}))
        return 0
    except (OSError, UnicodeError, MapError) as error:
        print("Map extraction failed: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
