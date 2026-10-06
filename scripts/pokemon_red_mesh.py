#!/usr/bin/env python3
"""Original, offline map-to-mesh experiment. Never modifies the ROM or emulator.

Input is the private JSON emitted by pokemon_red_map.py. Heights are an explicit
artist-authored interpretation of tile identity, not data recovered from a 2D
screen or heights claimed to exist in the original game. All output is private
build data; do not distribute it with R2N64.
"""
import argparse
import collections
import hashlib
import json
import math
from pathlib import Path
import struct


MATERIALS = {
    "ground": (0.0, (0.82, 0.87, 0.72)),
    "water": (-0.12, (0.39, 0.64, 0.81)),
    "tree": (1.75, (0.44, 0.68, 0.42)),
    "roof": (2.50, (0.76, 0.44, 0.35)),
    "wall": (1.25, (0.83, 0.79, 0.64)),
    "fence": (0.40, (0.73, 0.74, 0.65)),
    "sign": (0.70, (0.77, 0.64, 0.46)),
    "flower": (0.025, (0.85, 0.77, 0.62)),
    "boundary": (0.85, (0.54, 0.68, 0.51)),
}
SHADES = (1.0, 0.74, 0.46, 0.16)
MAX_VERTICES = 3_000_000
BUILD_ROOT = Path(__file__).resolve().parents[1] / "build"


def private_output(path):
    resolved = path.resolve()
    if BUILD_ROOT.resolve() not in resolved.parents or resolved.suffix != ".r2scene":
        raise ValueError("Derived map output must be a .r2scene file under this repository's build/")
    return resolved


def integer(value, minimum, maximum, label):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{label}: expected integer {minimum}..{maximum}")
    return value


def validate_map(data):
    if not isinstance(data, dict) or data.get("schema") != "r2n64.pokemon-red.map.v1":
        raise ValueError("Unsupported map schema")
    width = integer(data.get("tile_width"), 1, 128, "tile_width")
    height = integer(data.get("tile_height"), 1, 128, "tile_height")
    if data.get("pixel_width") != width * 8 or data.get("pixel_height") != height * 8:
        raise ValueError("Pixel dimensions disagree with tile grid")
    grid = data.get("tile_ids")
    if not isinstance(grid, list) or len(grid) != height:
        raise ValueError("Invalid tile row count")
    used = set()
    for row in grid:
        if not isinstance(row, list) or len(row) != width:
            raise ValueError("Invalid tile column count")
        for value in row:
            used.add(integer(value, 0, 255, "tile id"))
    pixels = data.get("tile_pixels")
    if not isinstance(pixels, dict):
        raise ValueError("Missing decoded tiles")
    for tile in used:
        values = pixels.get(str(tile))
        if not isinstance(values, list) or len(values) != 64:
            raise ValueError(f"Tile {tile}: expected 64 pixels")
        for value in values:
            integer(value, 0, 3, "pixel shade")
    return width, height


def validate_profile(profile, data):
    if not isinstance(profile, dict) or profile.get("schema") != "r2n64.pokemon-red.height-profile.v1":
        raise ValueError("Unsupported height profile")
    if profile.get("map_name") != data.get("name") or profile.get("map_id") != data.get("map_id"):
        raise ValueError("Height profile belongs to another map")
    tiles = profile.get("tile_materials")
    if not isinstance(tiles, dict):
        raise ValueError("Missing tile materials")
    result = {}
    for key, material in tiles.items():
        if not isinstance(key, str) or not key.isdecimal():
            raise ValueError("Material tile keys must be decimal ids")
        number = integer(int(key), 0, 255, "material tile")
        if material not in MATERIALS or str(number) != key:
            raise ValueError("Unknown material or ambiguous tile id")
        result[number] = material
    # Per-cell overrides are deliberately explicit: some games reuse the same
    # graphics for objects of different heights. Never infer from brightness.
    overrides = {}
    for item in profile.get("overrides", []):
        if not isinstance(item, dict) or item.get("material") not in MATERIALS:
            raise ValueError("Invalid material override")
        x = integer(item.get("x"), 0, data["tile_width"] - 1, "override x")
        y = integer(item.get("y"), 0, data["tile_height"] - 1, "override y")
        if (x, y) in overrides:
            raise ValueError("Duplicate cell override")
        overrides[x, y] = item["material"]
    buildings = []
    occupied = set()
    for item in profile.get("buildings", []):
        if not isinstance(item, dict):
            raise ValueError("Invalid building")
        x = integer(item.get("x"), 0, data["tile_width"] - 1, "building x")
        z = integer(item.get("z"), 0, data["tile_height"] - 1, "building z")
        w = integer(item.get("width"), 1, data["tile_width"] - x, "building width")
        h = integer(item.get("depth"), 2, data["tile_height"] - z, "building depth")
        roof = integer(item.get("roof_rows"), 1, h - 1, "building source roof rows")
        eaves, rise = item.get("eaves"), item.get("rise")
        if any(type(v) not in (float, int) or not math.isfinite(v) for v in (eaves, rise)):
            raise ValueError("Invalid building height")
        if not .5 <= eaves <= 8 or not 0 <= rise <= 4:
            raise ValueError("Building height outside budget")
        area = {(a, b) for a in range(x, x + w) for b in range(z, z + h)}
        if occupied.intersection(area):
            raise ValueError("Overlapping building footprints")
        occupied.update(area)
        buildings.append((x, z, w, h, roof, eaves, rise))
    return result, overrides, buildings, occupied


class Mesh:
    def __init__(self):
        self.vertices = bytearray()
        self.count = 0
        self.bounds_min = [math.inf] * 3
        self.bounds_max = [-math.inf] * 3

    def quad(self, corners, color):
        if self.count + 6 > MAX_VERTICES:
            raise ValueError("Mesh exceeds vertex budget")
        if len(corners) != 4 or len(color) != 3:
            raise ValueError("Invalid quad")
        for corner in corners:
            if len(corner) != 3 or not all(math.isfinite(v) for v in corner):
                raise ValueError("Invalid vertex")
        if not all(math.isfinite(c) and 0 <= c <= 1 for c in color):
            raise ValueError("Invalid color")
        for index in (0, 1, 2, 0, 2, 3):
            vertex = corners[index]
            self.vertices.extend(struct.pack("<6f", *vertex, *color))
            for axis in range(3):
                self.bounds_min[axis] = min(self.bounds_min[axis], vertex[axis])
                self.bounds_max[axis] = max(self.bounds_max[axis], vertex[axis])
        self.count += 6

    def binary(self):
        return b"R2SCENE1" + struct.pack("<I", self.count) + self.vertices


def map_pixel(data, x, y):
    tile = data["tile_ids"][y // 8][x // 8]
    return data["tile_pixels"][str(tile)][(y % 8) * 8 + x % 8]


def add_building(mesh, data, building):
    """Authored solid building, using actual roof/front pixels as surface art.

    The original projected front is unwrapped onto a vertical wall. The roof
    is stretched over the footprint and placed on two sloping planes. Hidden
    sides are original plain materials; they are not inferred ROM artwork.
    """
    x, z, width, depth, roof_rows, eaves, rise = building
    roof_pixels = roof_rows * 8
    wall_pixels = (depth - roof_rows) * 8
    roof_tint = MATERIALS["roof"][1]
    wall_tint = MATERIALS["wall"][1]

    def roof_height(t):
        return eaves + rise * (1 - abs(2 * t - 1))

    # All roof_rows * 8 counts are even, so no strip crosses the ridge.
    for py in range(depth * 8):
        px = 0
        roof = py < roof_pixels
        while px < width * 8:
            shade = map_pixel(data, x * 8 + px, z * 8 + py)
            end = px + 1
            while end < width * 8 and map_pixel(data, x * 8 + end, z * 8 + py) == shade:
                end += 1
            a, b = x + px / 8, x + end / 8
            if roof:
                t0, t1 = py / roof_pixels, (py + 1) / roof_pixels
                corners = ((a, roof_height(t0), z + depth * t0),
                           (a, roof_height(t1), z + depth * t1),
                           (b, roof_height(t1), z + depth * t1),
                           (b, roof_height(t0), z + depth * t0))
                tint = tuple(c * (1.0 if t0 >= .5 else .82) for c in roof_tint)
            else:
                y0 = eaves * (1 - (py - roof_pixels) / wall_pixels)
                y1 = eaves * (1 - (py + 1 - roof_pixels) / wall_pixels)
                corners = ((a, y0, z + depth), (a, y1, z + depth),
                           (b, y1, z + depth), (b, y0, z + depth))
                tint = wall_tint
            mesh.quad(corners, tuple(c * SHADES[shade] for c in tint))
            px = end

    mesh.quad(((x, 0, z), (x, eaves, z), (x + width, eaves, z), (x + width, 0, z)),
              tuple(c * .68 for c in wall_tint))
    for side, light in ((x, .66), (x + width, .85)):
        # Split the side in two quads so its top follows the roof ridge.
        mid = z + depth / 2
        mesh.quad(((side, 0, z), (side, eaves, z),
                   (side, eaves + rise, mid), (side, 0, mid)),
                  tuple(c * light for c in wall_tint))
        mesh.quad(((side, 0, mid), (side, eaves + rise, mid),
                   (side, eaves, z + depth), (side, 0, z + depth)),
                  tuple(c * light for c in wall_tint))


def make_mesh(data, profile):
    width, height = validate_map(data)
    materials, overrides, buildings, occupied = validate_profile(profile, data)
    cells = [[overrides.get((x, z), materials.get(data["tile_ids"][z][x], "ground"))
              for x in range(width)] for z in range(height)]
    mesh = Mesh()
    side_quads = 0
    top_quads = 0
    counts = collections.Counter()
    base = -0.45
    for z in range(height):
        for x in range(width):
            if (x, z) in occupied:
                continue
            material = cells[z][x]
            counts[material] += 1
            elevation, tint = MATERIALS[material]
            tile = data["tile_pixels"][str(data["tile_ids"][z][x])]
            # Merge consecutive identical original pixels; geometry follows
            # tile identity while the top surface preserves the decoded image.
            for row in range(8):
                start = 0
                while start < 8:
                    shade = tile[row * 8 + start]
                    end = start + 1
                    while end < 8 and tile[row * 8 + end] == shade:
                        end += 1
                    x0, x1 = x + start / 8, x + end / 8
                    z0, z1 = z + row / 8, z + (row + 1) / 8
                    mesh.quad(((x0, elevation, z0), (x0, elevation, z1),
                               (x1, elevation, z1), (x1, elevation, z0)),
                              tuple(c * SHADES[shade] for c in tint))
                    top_quads += 1
                    start = end
            # Closed vertical risers at changes of material height. Equal-height
            # cells share one surface with no internal walls or z-fighting.
            for dx, dz, light in ((-1, 0, .63), (1, 0, .83), (0, -1, .75), (0, 1, .91)):
                nx, nz = x + dx, z + dz
                neighbor = MATERIALS[cells[nz][nx]][0] if 0 <= nx < width and 0 <= nz < height else base
                if neighbor >= elevation:
                    continue
                if dx:
                    edge = x + (1 if dx > 0 else 0)
                    corners = ((edge, neighbor, z), (edge, elevation, z),
                               (edge, elevation, z + 1), (edge, neighbor, z + 1))
                else:
                    edge = z + (1 if dz > 0 else 0)
                    corners = ((x, neighbor, edge), (x, elevation, edge),
                               (x + 1, elevation, edge), (x + 1, neighbor, edge))
                mesh.quad(corners, tuple(c * light for c in tint))
                side_quads += 1
    for building in buildings:
        add_building(mesh, data, building)
    mesh.quad(((0, base, 0), (width, base, 0), (width, base, height), (0, base, height)), (.23, .26, .22))
    report = {
        "schema": "r2n64.pokemon-red.mesh-report.v1",
        "map_name": data["name"], "map_id": data["map_id"],
        "vertices": mesh.count, "triangles": mesh.count // 3,
        "top_quads": top_quads, "side_quads": side_quads,
        "bounds_min": mesh.bounds_min, "bounds_max": mesh.bounds_max,
        "material_cells": dict(sorted(counts.items())),
        "buildings": len(buildings),
        "building_footprint_cells": len(occupied),
        "building_art": "Decoded roof on sloping planes; front on vertical wall; hidden sides authored",
        "height_source": "Original authored tile/material profile; ROM contains no 3D heights",
        "scope": "Static map geometry; no live gameplay, NPC animation, UI or collisions",
        "ps4_hardware_tested": False,
    }
    return mesh, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--map", type=Path, required=True)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        args.output = private_output(args.output)
        if args.map.stat().st_size > 16 * 1024 * 1024 or args.profile.stat().st_size > 65536:
            raise ValueError("Input exceeds size budget")
        data = json.loads(args.map.read_text(encoding="utf-8"))
        profile = json.loads(args.profile.read_text(encoding="utf-8"))
        mesh, report = make_mesh(data, profile)
        blob = mesh.binary()
        report["mesh_sha256"] = hashlib.sha256(blob).hexdigest()
        report["map_sha256"] = hashlib.sha256(args.map.read_bytes()).hexdigest()
        report["profile_sha256"] = hashlib.sha256(args.profile.read_bytes()).hexdigest()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(blob)
        args.output.with_suffix(".report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2))
    except (ValueError, TypeError, OSError, KeyError) as error:
        parser.exit(1, f"Map-to-mesh failed: {error}\n")


if __name__ == "__main__":
    main()
