#!/usr/bin/env python3
"""Original synthetic geometry only. No ROM or decoded commercial data."""
import copy
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location("red_mesh", Path(__file__).resolve().parents[1] / "scripts/pokemon_red_mesh.py")
mesher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mesher)


def fixture():
    return {"schema": "r2n64.pokemon-red.map.v1", "name": "Synthetic", "map_id": 0,
            "tile_width": 2, "tile_height": 1, "pixel_width": 16, "pixel_height": 8,
            "tile_ids": [[1, 2]], "tile_pixels": {"1": [0] * 64, "2": [0] * 64}}


def profile():
    return {"schema": "r2n64.pokemon-red.height-profile.v1", "map_name": "Synthetic", "map_id": 0,
            "tile_materials": {"1": "roof", "2": "ground"}, "overrides": []}


class MeshTests(unittest.TestCase):
    def test_geometry_follows_tile_identity_not_pixel_brightness(self):
        mesh, report = mesher.make_mesh(fixture(), profile())
        self.assertEqual(report["bounds_max"], [2.0, 2.5, 1.0])
        self.assertEqual(report["side_quads"], 7)  # only tall cell owns their shared wall
        self.assertEqual(report["top_quads"], 16)
        blob = mesh.binary()
        self.assertEqual(blob[:8], b"R2SCENE1")
        self.assertEqual(struct.unpack("<I", blob[8:12])[0], mesh.count)
        self.assertEqual(len(blob), 12 + mesh.count * 24)
        data = fixture()
        data["tile_pixels"]["1"] = [3] * 64
        _, dark = mesher.make_mesh(data, profile())
        self.assertEqual(dark["bounds_max"], report["bounds_max"])
        self.assertEqual(dark["side_quads"], report["side_quads"])

    def test_no_internal_wall_between_equal_heights(self):
        options = profile()
        options["tile_materials"]["2"] = "roof"
        _, report = mesher.make_mesh(fixture(), options)
        self.assertEqual(report["side_quads"], 6)

    def test_water_has_real_lower_surface(self):
        options = profile()
        options["tile_materials"]["2"] = "water"
        mesh, report = mesher.make_mesh(fixture(), options)
        self.assertEqual(report["material_cells"]["water"], 1)
        ys = [v[1] for v in struct.iter_unpack("<6f", mesh.vertices)]
        self.assertTrue(any(abs(v + .12) < .00001 for v in ys))

    def test_overrides_and_determinism(self):
        options = profile()
        options["overrides"] = [{"x": 0, "y": 0, "material": "tree"}]
        a, report = mesher.make_mesh(fixture(), options)
        b, _ = mesher.make_mesh(fixture(), options)
        self.assertEqual(a.binary(), b.binary())
        self.assertEqual(report["bounds_max"][1], 1.75)

    def test_invalid_input(self):
        for key, value in (("schema", "other"), ("tile_width", 0), ("tile_height", True),
                           ("pixel_width", 1), ("tile_ids", [[256, 2]]),
                           ("tile_pixels", {"1": [0] * 63, "2": [0] * 64})):
            data = copy.deepcopy(fixture())
            data[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                mesher.make_mesh(data, profile())
        for key, value in (("map_id", 1), ("map_name", "wrong"), ("tile_materials", {"1": "unknown"}),
                           ("overrides", [{"x": 9, "y": 0, "material": "tree"}])):
            options = profile()
            options[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                mesher.make_mesh(fixture(), options)

    def test_non_object_json_is_rejected_cleanly(self):
        for invalid in (None, [], 3, "map"):
            with self.subTest(kind="map", value=invalid), self.assertRaises(ValueError):
                mesher.make_mesh(invalid, profile())
            with self.subTest(kind="profile", value=invalid), self.assertRaises(ValueError):
                mesher.make_mesh(fixture(), invalid)

    def test_budget_and_nonfinite_vertices(self):
        mesh = mesher.Mesh()
        with self.assertRaises(ValueError):
            mesh.quad(((0, 0, float("nan")),) * 4, (1, 1, 1))
        mesh.count = mesher.MAX_VERTICES
        with self.assertRaises(ValueError):
            mesh.quad(((0, 0, 0),) * 4, (1, 1, 1))

    def test_derived_assets_stay_in_private_build(self):
        expected = mesher.BUILD_ROOT / "synthetic.r2scene"
        self.assertEqual(mesher.private_output(expected), expected.resolve())
        for path in (mesher.BUILD_ROOT.parent / "assets/map.r2scene", mesher.BUILD_ROOT / "map.png",
                     mesher.BUILD_ROOT / "../dist/map.r2scene"):
            with self.assertRaises(ValueError):
                mesher.private_output(path)

    def test_building_has_sloped_roof_and_vertical_front(self):
        data = fixture()
        data.update(tile_height=2, pixel_height=16, tile_ids=[[1, 2], [1, 2]])
        options = profile()
        options["buildings"] = [{"x": 0, "z": 0, "width": 2, "depth": 2,
                                 "roof_rows": 1, "eaves": 2, "rise": 1}]
        mesh, report = mesher.make_mesh(data, options)
        self.assertEqual(report["buildings"], 1)
        self.assertEqual(report["building_footprint_cells"], 4)
        self.assertEqual(report["top_quads"], 0)
        self.assertEqual(report["bounds_max"], [2.0, 3.0, 2.0])
        vertices = list(struct.iter_unpack("<6f", mesh.vertices))
        self.assertTrue(any(v[:3] == (0., 3., 1.) for v in vertices))
        self.assertTrue(any(v[:3] == (0., 0., 2.) for v in vertices))
        options["buildings"].append(dict(options["buildings"][0]))
        with self.assertRaisesRegex(ValueError, "Overlapping"):
            mesher.make_mesh(data, options)

    def test_bad_building_geometry_is_rejected(self):
        data = fixture()
        data.update(tile_height=2, pixel_height=16, tile_ids=[[1, 2], [1, 2]])
        building = {"x": 0, "z": 0, "width": 2, "depth": 2,
                    "roof_rows": 1, "eaves": 2, "rise": 1}
        for key, value in (("width", 3), ("roof_rows", 2), ("eaves", float("nan")), ("rise", -1)):
            options = profile()
            options["buildings"] = [dict(building, **{key: value})]
            with self.subTest(key=key), self.assertRaises(ValueError):
                mesher.make_mesh(data, options)


if __name__ == "__main__":
    unittest.main()
