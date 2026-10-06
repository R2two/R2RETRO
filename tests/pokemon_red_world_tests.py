"""Synthetic-only tests; no cartridge pixels, maps or commercial files."""
import copy
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import pokemon_red_world as world


def original_map(map_id):
    name, width, height, *_ = world.maps.MAPS[map_id]
    return {"schema": world.maps.SCHEMA, "map_id": map_id, "name": name,
            "tile_width": width * 4, "tile_height": height * 4,
            "pixel_width": width * 32, "pixel_height": height * 32,
            "tile_ids": [[0] * (width * 4) for _ in range(height * 4)],
            "tile_pixels": {"0": [(x + y) % 4 for y in range(8) for x in range(8)]},
            "warps": []}


def profiles():
    return (json.loads((ROOT / "tools/pokemon3d/pallet_height_profile.json").read_text()),
            json.loads((ROOT / "tools/pokemon3d/world_profile.json").read_text()))


class WorldTests(unittest.TestCase):
    def test_four_original_maps_binary_counts_and_triangle_budget(self):
        for map_id in (0, 37, 38, 40):
            mesh, report = world.make_world(original_map(map_id), *profiles())
            blob = mesh.binary()
            self.assertEqual(blob[:8], b"R2SCENE2")
            count, width, height = struct.unpack_from("<III", blob, 8)
            self.assertEqual(len(blob), 20 + count * 32 + width * height * 4)
            self.assertEqual(count % 3, 0)
            self.assertLess(count // 3, 5000)
            self.assertEqual(report["map_id"], map_id)
            self.assertEqual(report["furniture"], {0: 0, 37: 9, 38: 7, 40: 15}[map_id])
            for vertex in struct.iter_unpack("<8f", mesh.vertices):
                self.assertTrue(all(0 <= value <= 1 for value in vertex[3:]))
            self.assertTrue(all(mesh.atlas.pixels[i] == 255 for i in range(3, len(mesh.atlas.pixels), 4)))

    def test_tile_uv_address_and_original_pixel_decode(self):
        data = original_map(37)
        atlas = world.Atlas(data)
        self.assertEqual(atlas.pixels[:16], bytes([255] * 4 + [207] * 3 + [255] + [128] * 3 + [255] + [27] * 3 + [255]))
        self.assertEqual(atlas.tiles(2, 3), ((16.5 / 256), (24.5 / 256), (23.5 / 256), (31.5 / 256)))
        for rect in ((-1, 0, 1, 1), (0, 0, 0, 1), (255, 255, 2, 2)):
            with self.assertRaises(ValueError):
                atlas.rect(*rect)

    def test_room_rejects_unknown_malformed_and_overlapping_furniture(self):
        data = original_map(37)
        for key, value in (("x", 15), ("width", 0), ("height", float("nan")), ("height", 10), ("kind", "unknown")):
            exterior, profile = profiles()
            profile["maps"]["37"]["furniture"][0][key] = value
            with self.assertRaises(ValueError):
                world.make_world(data, exterior, profile)
        exterior, profile = profiles()
        profile["maps"]["37"]["furniture"].append(copy.deepcopy(profile["maps"]["37"]["furniture"][0]))
        with self.assertRaises(ValueError):
            world.make_world(data, exterior, profile)

    def test_oak_atlas_preserves_tall_map_and_ignores_dynamic_objects(self):
        data = original_map(40)
        atlas = world.Atlas(data)
        self.assertEqual((atlas.width, atlas.height), (256, 256))
        for x, y in ((0, 0), (159, 191), (15, 145), (158, 162)):
            p = (y * atlas.width + x) * 4
            expected = (255, 207, 128, 27)[(x + y) % 4]
            self.assertEqual(atlas.pixels[p:p + 4], bytes((expected, expected, expected, 255)))
        first, _ = world.make_world(data, *profiles())
        data["objects"] = [{"sprite_id": 61, "x": 6, "y": 3}, {"sprite_id": 65, "x": 2, "y": 1}]
        second, _ = world.make_world(data, *profiles())
        self.assertEqual(first.binary(), second.binary(), "Dynamic objects must never be baked into scene geometry")

    def test_authored_floor_settings_are_validated(self):
        for key, value in (("floor_color", [1, 2, 0]), ("wall_color", [float("nan"), 0, 0]),
                           ("floor_sample", [0, 255]), ("floor_sample", [1])):
            exterior, profile = profiles()
            profile["maps"]["40"][key] = value
            with self.assertRaises(ValueError):
                world.make_world(original_map(40), exterior, profile)

    def test_wrong_map_dimensions_and_identity_fail(self):
        for key, value in (("map_id", 1), ("name", "Elsewhere"), ("pixel_width", 0)):
            data = original_map(37)
            data[key] = value
            with self.assertRaises(ValueError):
                world.make_world(data, *profiles())
        data = original_map(37)
        data.update(tile_width=8, pixel_width=64, tile_ids=[[0] * 8 for _ in range(16)])
        with self.assertRaises(ValueError):
            world.make_world(data, *profiles())

    def test_outputs_resolve_inside_ignored_build(self):
        self.assertEqual(world.private_directory(ROOT / "build/world"), ROOT / "build/world")
        for path in (ROOT / "assets/world", ROOT / "build/../world", ROOT / "build"):
            with self.assertRaises(ValueError):
                world.private_directory(path)

    def test_original_png_scanlines_and_crc(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "synthetic.png"
            pixels = bytes((255, 0, 0, 255, 0, 255, 0, 255))
            world.png(path, 2, 1, pixels)
            blob = path.read_bytes()
            self.assertEqual(blob[:8], b"\x89PNG\r\n\x1a\n")
            p, image_data = 8, b""
            while p < len(blob):
                size, = struct.unpack_from(">I", blob, p)
                kind, payload = blob[p + 4:p + 8], blob[p + 8:p + 8 + size]
                crc, = struct.unpack_from(">I", blob, p + 8 + size)
                self.assertEqual(crc, zlib.crc32(kind + payload))
                if kind == b"IDAT":
                    image_data += payload
                p += 12 + size
            self.assertEqual(zlib.decompress(image_data), b"\0" + pixels)
            with self.assertRaises(ValueError):
                world.png(path, 3, 1, pixels)


if __name__ == "__main__":
    unittest.main()
