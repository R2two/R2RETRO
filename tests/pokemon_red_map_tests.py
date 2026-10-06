"""Original fixtures only: never reads or embeds commercial cartridge data."""
import importlib.util
from pathlib import Path
import struct
import unittest

SPEC = importlib.util.spec_from_file_location("pokemon_red_map", Path(__file__).resolve().parents[1] / "scripts/pokemon_red_map.py")
MAP = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MAP)


def fixture():
    rom = bytearray(4 * 0x4000)
    symbols = {"MapHeaderPointers": (0, 0x200), "MapHeaderBanks": (0, 0x300),
               "PalletTown_h": (1, 0x4000), "PalletTown_Blocks": (1, 0x4100),
               "Tilesets": (0, 0x400), "Overworld_GFX": (2, 0x4000),
               "Overworld_Block": (2, 0x4020)}
    struct.pack_into("<H", rom, 0x200, 0x4000)
    rom[0x300] = 1
    rom[0x4000:0x400C] = bytes([0, 9, 10, 0, 0x41, 0, 0x43, 0, 0x43, 0, 0, 0x42])
    rom[0x4100:0x415A] = bytes(i % 2 for i in range(90))
    # One warp, one sign, one original person event.
    rom[0x4200:0x4212] = bytes([1, 1, 2, 3, 0, 7, 1, 4, 5, 9, 1, 2, 10, 11, 255, 255, 1, 0])
    rom[0x400:0x40C] = bytes([2, 0x20, 0x40, 0, 0x40, 0, 5, 255, 255, 255, 1, 0])
    rom[0x500:0x503] = bytes([0, 1, 255])
    rom[0x8000:0x8010] = bytes([0xAA, 0xCC] * 8)
    rom[0x8010:0x8020] = bytes([0xFF, 0] * 8)
    rom[0x8020:0x8030] = bytes([0] * 16)
    rom[0x8030:0x8040] = bytes([1] * 16)
    return rom, symbols


class PokemonMapTests(unittest.TestCase):
    def test_real_hash_guard_rejects_original_fixture_and_one_mib_fake(self):
        with self.assertRaises(MAP.MapError):
            MAP.verify_rom(fixture()[0])
        with self.assertRaises(MAP.MapError):
            MAP.verify_rom(bytes(1024 * 1024))

    def test_2bpp_planes_and_most_significant_pixel(self):
        self.assertEqual(MAP.decode_tile(bytes([0xAA, 0xCC] * 8))[:8], [3, 2, 1, 0, 3, 2, 1, 0])
        with self.assertRaises(MAP.MapError):
            MAP.decode_tile(bytes(15))

    def test_bank_translation_and_bounds(self):
        self.assertEqual(MAP.rom_offset(3, 0x4234), 0xC234)
        for bank, address in ((64, 0x4000), (1, 0x0100), (0, 0x4000), (1, 0x8000)):
            with self.assertRaises(MAP.MapError):
                MAP.rom_offset(bank, address)
        with self.assertRaises(MAP.MapError):
            MAP.checked_bytes(bytes(0x8000), 0x3FFF, 2)
        with self.assertRaises(MAP.MapError):
            MAP.checked_bytes(bytes(10), 9, 2)

    def test_complete_original_map_and_object_coordinates(self):
        rom, symbols = fixture()
        result = MAP.extract_pallet(rom, symbols)
        self.assertEqual(result["schema"], MAP.SCHEMA)
        self.assertEqual((result["tile_width"], result["tile_height"]), (40, 36))
        self.assertEqual(result["tile_ids"][0][:8], [0] * 4 + [1] * 4)
        self.assertEqual(result["tile_ids"][4][:8], [0] * 4 + [1] * 4)
        self.assertEqual(result["tile_pixels"]["1"], [1] * 64)
        self.assertEqual(result["objects"][0]["x"], 7)
        self.assertEqual(result["objects"][0]["y"], 6)
        self.assertEqual(result["warps"][0]["destination_map"], 7)
        self.assertEqual(result["signs"][0]["text_id"], 9)

    def test_incorrect_symbols_header_bank_dimensions_and_tile_rejected(self):
        for offset, value in ((0x300, 64), (0x4001, 0), (0x4002, 255),
                              (0x4009, 0x80), (0x8020, 2), (0x420C, 0)):
            rom, symbols = fixture()
            rom[offset] = value
            with self.assertRaises(MAP.MapError):
                MAP.extract_pallet(rom, symbols)
        rom, symbols = fixture()
        symbols["PalletTown_h"] = (1, 0x4001)
        with self.assertRaises(MAP.MapError):
            MAP.extract_pallet(rom, symbols)

    def test_variable_length_trainer_and_item_records(self):
        rom, symbols = fixture()
        # Replace only this original fixture's event list with trainer + item.
        rom[0x420A:0x421A] = bytes([2, 2, 10, 11, 255, 255, 0x41, 7, 8,
                                  3, 12, 13, 255, 255, 0x82, 9])
        result = MAP.extract_pallet(rom, symbols)
        self.assertEqual([o["kind"] for o in result["objects"]], ["trainer", "item"])
        self.assertEqual(result["objects"][0]["parameters"], [7, 8])
        self.assertEqual(result["objects"][1]["parameters"], [9])
        rom[0x4210] = 0xC1
        with self.assertRaises(MAP.MapError):
            MAP.extract_pallet(rom, symbols)

    def test_symbols_comments_conflicts_and_missing(self):
        symbols = MAP.parse_symbols("; generated\n00:0100 Start ; test\n01:4000 Room\n0a WIDTH\n")
        self.assertEqual(MAP.resolve(symbols, "Room"), 0x4000)
        for text in ("00:0000 one\n00:0001 one", "not a symbol", "10000:0000 one"):
            with self.assertRaises(MAP.MapError):
                MAP.parse_symbols(text)
        with self.assertRaises(MAP.MapError):
            MAP.resolve(symbols, "Absent")

    def test_no_cartridge_exports_to_source_tree(self):
        self.assertEqual(MAP.build_output(MAP.ROOT / "build/map.json"), MAP.ROOT / "build/map.json")
        for path in (MAP.ROOT / "assets/map.json", MAP.ROOT / "build/../map.json", MAP.ROOT / "build/map.gb"):
            with self.assertRaises(MAP.MapError):
                MAP.build_output(path)

    def test_original_interior_map_tables_and_tilesets(self):
        for map_id, tileset_id, tile_name in ((37, 1, "RedsHouse1"), (38, 4, "RedsHouse2"), (40, 5, "Gym")):
            rom, symbols = fixture()
            name = MAP.MAPS[map_id][0]
            symbols.update({name + "_h": (1, 0x4300), name + "_Blocks": (1, 0x4400),
                            tile_name + "_GFX": (2, 0x4000), tile_name + "_Block": (2, 0x4020)})
            struct.pack_into("<H", rom, 0x200 + map_id * 2, 0x4300)
            rom[0x300 + map_id] = 1
            _, width, height, *_ = MAP.MAPS[map_id]
            rom[0x4300:0x430C] = bytes([tileset_id, height, width, 0, 0x44, 0, 0x46, 0, 0x46, 0, 0, 0x45])
            rom[0x400 + tileset_id * 12:0x40C + tileset_id * 12] = rom[0x400:0x40C]
            result = MAP.extract_map(rom, symbols, map_id)
            self.assertEqual((result["map_id"], result["tile_width"], result["tile_height"]), (map_id, width * 4, height * 4))
            self.assertEqual(result["tileset"]["id"], tileset_id)
            self.assertEqual(result["objects"], [])
            rom[0x4300] = 0
            with self.assertRaises(MAP.MapError):
                MAP.extract_map(rom, symbols, map_id)
        for map_id in (-1, 1, 255, True, "37"):
            with self.assertRaises(MAP.MapError):
                MAP.extract_map(*fixture(), map_id)


if __name__ == "__main__":
    unittest.main()
