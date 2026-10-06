#!/usr/bin/env python3
"""Build private textured Pallet/Red-house/Oak-lab scenes from a user cartridge.

No art is bundled: original cartridge pixels are read locally, and architectural
heights, lighting and details are authored interpretations. Uses only Python's
standard library. Never modifies cartridge or emulator memory.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import zlib

import pokemon_red_map as maps
import pokemon_red_mesh as legacy

ROOT = Path(__file__).resolve().parents[1]
MAX_VERTICES = 3_000_000


def private_directory(path):
    path = Path(path).resolve()
    if (ROOT / "build").resolve() not in path.parents:
        raise ValueError("Derived world files must remain inside this repository's build/")
    return path


def png(path, width, height, pixels):
    """Original minimal RGBA PNG writer; no imaging dependency."""
    if len(pixels) != width * height * 4:
        raise ValueError("PNG dimensions disagree with RGBA payload")
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))
    rows = b"".join(b"\0" + pixels[y * width * 4:(y + 1) * width * 4] for y in range(height))
    Path(path).write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", width, height, 8, 6, 0, 0, 0)) +
                           chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


class Atlas:
    def __init__(self, data):
        self.width = self.height = 512 if data["map_id"] == 0 else 256
        self.pixels = bytearray([255]) * (self.width * self.height * 4)
        for y in range(data["pixel_height"]):
            for x in range(data["pixel_width"]):
                shade = legacy.map_pixel(data, x, y)
                self.set(x, y, (255, 207, 128, 27)[shade])
        self.pattern_y = data["pixel_height"] + 8
        # Taller rooms still fit a 256-square atlas by placing the two material
        # patches in the free column beside the map rather than below it.
        if self.pattern_y + 64 <= self.height:
            self.patterns = {"plank": (0, self.pattern_y), "roof": (72, self.pattern_y)}
        else:
            self.patterns = {"plank": (data["pixel_width"] + 8, 0), "roof": (data["pixel_width"] + 8, 72)}
        # Generic authored plank and roof patterns, not extracted assets.
        for y in range(64):
            for x in range(64):
                px, py = self.patterns["plank"]
                self.set(x + px, y + py, 180 if y % 8 == 7 else 237 if y % 8 == 0 else 255)
                px, py = self.patterns["roof"]
                self.set(x + px, y + py, 177 if y % 8 == 7 or (x + (y // 8 % 2) * 8) % 16 == 15 else 245)
        self.white = ((self.width - .5) / self.width, (self.height - .5) / self.height)

    def set(self, x, y, gray):
        if not 0 <= x < self.width or not 0 <= y < self.height:
            raise ValueError("Atlas pixel out of bounds")
        p = (y * self.width + x) * 4
        self.pixels[p:p + 4] = bytes((gray, gray, gray, 255))

    def rect(self, x, y, width, height):
        if min(x, y) < 0 or min(width, height) <= 0 or x + width > self.width or y + height > self.height:
            raise ValueError("Atlas rectangle out of bounds")
        return ((x + .5) / self.width, (y + .5) / self.height,
                (x + width - .5) / self.width, (y + height - .5) / self.height)

    def tiles(self, x, z, width=1, depth=1):
        return self.rect(x * 8, z * 8, width * 8, depth * 8)

    def pattern(self, kind):
        x, y = self.patterns[kind]
        return self.rect(x, y, 64, 64)


class Mesh:
    def __init__(self, atlas):
        self.atlas, self.vertices, self.count = atlas, bytearray(), 0
        self.bounds_min, self.bounds_max = [math.inf] * 3, [-math.inf] * 3

    def quad(self, points, color=(1, 1, 1), rect=None):
        if len(points) != 4 or len(color) != 3 or self.count + 6 > MAX_VERTICES:
            raise ValueError("Invalid quad or vertex budget exceeded")
        if rect is None:
            uv = [self.atlas.white] * 4
        else:
            a, b, c, d = rect
            uv = [(a, b), (a, d), (c, d), (c, b)]
        for n in (0, 1, 2, 0, 2, 3):
            values = (*points[n], *color, *uv[n])
            if len(values) != 8 or not all(math.isfinite(v) for v in values):
                raise ValueError("Invalid vertex")
            if any(abs(v) > 1_000_000 for v in values[:3]) or any(not 0 <= v <= 1 for v in values[3:]):
                raise ValueError("Vertex value out of range")
            self.vertices.extend(struct.pack("<8f", *values))
            for axis in range(3):
                self.bounds_min[axis] = min(self.bounds_min[axis], points[n][axis])
                self.bounds_max[axis] = max(self.bounds_max[axis], points[n][axis])
        self.count += 6

    def horizontal(self, x, z, width, depth, y, color=(1, 1, 1), rect=None):
        self.quad(((x, y, z), (x, y, z + depth), (x + width, y, z + depth), (x + width, y, z)), color, rect)

    def box(self, x, z, width, depth, y0, y1, color, top=None, front=None, sides=None):
        self.horizontal(x, z, width, depth, y1, color, top)
        for side, light in ((x, .68), (x + width, .84)):
            self.quad(((side, y1, z), (side, y0, z), (side, y0, z + depth), (side, y1, z + depth)), tint(color, light), sides)
        self.quad(((x, y1, z), (x, y0, z), (x + width, y0, z), (x + width, y1, z)), tint(color, .70), sides)
        self.quad(((x, y1, z + depth), (x, y0, z + depth), (x + width, y0, z + depth), (x + width, y1, z + depth)), tint(color, .94), front or sides)

    def binary(self):
        return b"R2SCENE2" + struct.pack("<III", self.count, self.atlas.width, self.atlas.height) + self.vertices + self.atlas.pixels


def tint(color, strength):
    return tuple(c * strength for c in color)


def building(mesh, data, spec, index):
    x, z, w, d, rows, eaves, rise = spec
    a = mesh.atlas
    wall = (.95, .91, .70)
    roof = (.65, .28, .22) if index == 0 else (.24, .43, .58)
    # Preserve original facade pixels; authored side boards, frames and eaves.
    mesh.box(x, z, w, d, 0, eaves, wall, front=a.tiles(x, z + rows, w, d - rows), sides=a.pattern("plank"))
    over = .20
    if index < 2:
        # Front-facing gable, original interpretation rather than the old
        # sideways wedge. Small attic window and eaves make the silhouette read.
        mid = x + w / 2
        for start, end, high_start, high_end, light in ((x - over, mid, eaves, eaves + rise, .88), (mid, x + w + over, eaves + rise, eaves, 1)):
            mesh.quad(((start, high_start, z - over), (start, high_start, z + d + over), (end, high_end, z + d + over), (end, high_end, z - over)), tint(roof, light), a.pattern("roof"))
        for edge, light in ((z, .70), (z + d, 1.0)):
            mesh.quad(((x, eaves, edge), (mid, eaves, edge), (mid, eaves + rise, edge), (x, eaves, edge)), tint(wall, light))
            mesh.quad(((mid, eaves + rise, edge), (mid, eaves, edge), (x + w, eaves, edge), (mid, eaves + rise, edge)), tint(wall, light))
        for edge in (x - over, x + w + over):
            mesh.box(edge, z - over, .13, d + over * 2, eaves - .12, eaves, (.89, .86, .72))
        mesh.box(mid - .45, z + d + .01, .9, .12, eaves + .10, eaves + .76, (.89, .89, .77))
        mesh.quad(((mid - .30, eaves + .64, z + d + .14), (mid - .30, eaves + .20, z + d + .14), (mid + .30, eaves + .20, z + d + .14), (mid + .30, eaves + .64, z + d + .14)), (.31, .49, .60))
        # Chimney and dark opening, original architectural interpretation.
        mesh.box(x + w - 1.7, z + 1.3, .65, .72, eaves + .1, eaves + rise + .8, (.53, .35, .29), sides=a.pattern("roof"))
        mesh.box(x + w - 1.8, z + 1.2, .85, .92, eaves + rise + .8, eaves + rise + .95, (.44, .45, .41))
        mesh.horizontal(x + w - 1.65, z + 1.35, .55, .62, eaves + rise + .951, (.14, .16, .17))
    else:
        # Lab has a low roof terrace with three skylights; all are authored
        # scenery, never a claim about data recovered from the cartridge.
        mesh.horizontal(x - over, z - over, w + over * 2, d + over * 2, eaves + .05, (.34, .43, .46), a.pattern("roof"))
        for edge in (x - over, x + w):
            mesh.box(edge, z - over, .20, d + over * 2, eaves, eaves + .42, roof)
        for edge in (z - over, z + d):
            mesh.box(x - over, edge, w + over * 2, .20, eaves, eaves + .42, roof)
        for sx in (x + 2, x + 5.3, x + 8.6):
            mesh.box(sx, z + 2, 1.3, 2.1, eaves + .06, eaves + .34, (.72, .77, .76))
            mesh.horizontal(sx + .12, z + 2.12, 1.06, 1.86, eaves + .345, (.24, .42, .54))
    # Doors align with the cartridge's warp columns, not guessed walkable cells.
    for warp in data["warps"]:
        door_x, door_z = warp["x"] * 2, warp["y"] * 2 + 2
        if x <= door_x < x + w and abs(door_z - (z + d)) < .1:
            front = z + d + .035
            mesh.quad(((door_x + .1, 2.0, front), (door_x + .1, 0, front), (door_x + 1.9, 0, front), (door_x + 1.9, 2.0, front)), (.30, .45, .53))
            mesh.box(door_x - .04, z + d, 2.08, .30, 2.03, 2.18, roof)
            mesh.box(door_x + 1.6, z + d + .04, .10, .08, .82, 1.02, (.91, .74, .29))
    # Raised window frame on a region that is solid in both visual map and ROM.
    wx, front = x + w - 2.8, z + d + .06
    for px, py, ww, hh in ((wx, .85, 1.7, .12), (wx, 2.12, 1.7, .12), (wx, .85, .12, 1.39), (wx + 1.58, .85, .12, 1.39)):
        mesh.quad(((px, py + hh, front), (px, py, front), (px + ww, py, front), (px + ww, py + hh, front)), (.93, .93, .79))
    mesh.quad(((wx + .12, 2.12, front - .01), (wx + .12, .97, front - .01), (wx + 1.58, .97, front - .01), (wx + 1.58, 2.12, front - .01)), (.37, .66, .75))
    for px, py, ww, hh in ((wx + .79, .97, .10, 1.15), (wx + .12, 1.5, 1.46, .10)):
        mesh.quad(((px, py + hh, front + .01), (px, py, front + .01), (px + ww, py, front + .01), (px + ww, py + hh, front + .01)), (.93, .93, .79))
    mesh.box(wx - .08, z + d + .02, 1.86, .42, .58, .82, (.52, .32, .21))
    for px in (wx + .05, wx + .62, wx + 1.19):
        mesh.box(px, z + d + .04, .44, .34, .79, .98, (.31, .52, .28))


def outdoor(mesh, data, profile):
    width, height = legacy.validate_map(data)
    materials, overrides, buildings, occupied = legacy.validate_profile(profile, data)
    cells = [[overrides.get((x, z), materials.get(data["tile_ids"][z][x], "ground")) for x in range(width)] for z in range(height)]
    shadow = {(xx, zz) for x, z, w, d, *_ in buildings for xx in range(x + 1, min(width, x + w + 2)) for zz in range(z + d, min(height, z + d + 2))}
    for z in range(height):
        for x in range(width):
            if (x, z) in occupied:
                continue
            elevation, color = legacy.MATERIALS[cells[z][x]]
            mesh.horizontal(x, z, 1, 1, elevation, tint(color, .76 if (x, z) in shadow else 1), mesh.atlas.tiles(x, z))
            for dx, dz, light in ((-1, 0, .63), (1, 0, .83), (0, -1, .75), (0, 1, .91)):
                nx, nz = x + dx, z + dz
                neighbor = legacy.MATERIALS[cells[nz][nx]][0] if 0 <= nx < width and 0 <= nz < height else -.45
                if neighbor >= elevation or (nx, nz) in occupied:
                    continue
                if dx:
                    edge = x + (dx > 0)
                    points = ((edge, elevation, z), (edge, neighbor, z), (edge, neighbor, z + 1), (edge, elevation, z + 1))
                else:
                    edge = z + (dz > 0)
                    points = ((x, elevation, edge), (x, neighbor, edge), (x + 1, neighbor, edge), (x + 1, elevation, edge))
                mesh.quad(points, tint(color, light))
    for index, spec in enumerate(buildings):
        building(mesh, data, spec, index)
    mesh.horizontal(0, 0, width, height, -.46, (.24, .31, .25))
    return len(buildings), 0


def validate_furniture(profile, data):
    if profile.get("schema") != "r2n64.pokemon-red.world-profile.v1":
        raise ValueError("Unsupported world profile")
    entries = profile.get("maps", {}).get(str(data["map_id"]), {}).get("furniture")
    if not isinstance(entries, list) or len(entries) > 64:
        raise ValueError("Missing or excessive furniture")
    occupied = set()
    for obj in entries:
        if obj.get("kind") not in ("cabinet", "tv", "stairs", "table", "chair", "computer", "workstation", "console", "bed", "plant"):
            raise ValueError("Unknown authored furniture type")
        for key in ("x", "z", "width", "depth"):
            legacy.integer(obj.get(key), 0 if key in ("x", "z") else 1, 128, key)
        x, z, w, d = (obj[k] for k in ("x", "z", "width", "depth"))
        if x + w > data["tile_width"] or z + d > data["tile_height"]:
            raise ValueError("Furniture exceeds map")
        h = obj.get("height")
        if type(h) not in (int, float) or not math.isfinite(h) or not 0 < h <= 6:
            raise ValueError("Invalid furniture height")
        cells = {(xx, zz) for xx in range(x, x + w) for zz in range(z, z + d)}
        if cells & occupied:
            raise ValueError("Overlapping furniture footprints")
        occupied |= cells
    return entries, occupied


def interior(mesh, data, profile):
    width, height = legacy.validate_map(data)
    entries, occupied = validate_furniture(profile, data)
    a = mesh.atlas
    settings = profile["maps"][str(data["map_id"])]
    floor = settings.get("floor_color", (1.0, .97, .86))
    wall = settings.get("wall_color", (.82, .69, .42))
    for color in (floor, wall):
        if not isinstance(color, (list, tuple)) or len(color) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) or not 0 <= v <= 1 for v in color):
            raise ValueError("Invalid room material color")
    sample = settings.get("floor_sample", (4, 4))
    if not isinstance(sample, (list, tuple)) or len(sample) != 2:
        raise ValueError("Invalid room floor sample")
    legacy.integer(sample[0], 0, width - 1, "floor sample x")
    legacy.integer(sample[1], 0, height - 1, "floor sample z")
    wall_sample = settings.get("wall_sample", (4, 0))
    if not isinstance(wall_sample, (list, tuple)) or len(wall_sample) != 2:
        raise ValueError("Invalid wall sample")
    legacy.integer(wall_sample[0], 0, width - 1, "wall sample x")
    legacy.integer(wall_sample[1], 0, height - 2, "wall sample z")
    shadow = {(x + 1, z + 1) for x, z in occupied} - occupied
    for z in range(height):
        for x in range(width):
            if (x, z) in occupied or z < 2:
                continue
            rug = (data["map_id"] == 37 and 4 <= x < 8 and z >= 14) or (data["map_id"] == 40 and 8 <= x < 12 and z >= 22)
            color = (.79, .33, .35) if rug else floor
            mesh.horizontal(x, z, 1, 1, 0, tint(color, .86 if (x, z) in shadow else 1), a.tiles(x, z))
    # Open-front dollhouse cutaway. No side/front walls hide the player.
    for wx in range(width):
        # The 2D map includes the projected upper half of tall furniture in
        # the wall rows. Remove that ghost when rendering a separate 3D object.
        source = a.tiles(*wall_sample, 1, 2) if (wx, 0) in occupied or (wx, 1) in occupied else a.tiles(wx, 0, 1, 2)
        mesh.quad(((wx, 2.6, 2), (wx, 0, 2), (wx + 1, 0, 2), (wx + 1, 2.6, 2)), wall, source)
    mesh.horizontal(0, 1.8, width, .25, 2.6, tint(wall, .80))
    mesh.box(0, 2, width, .10, 0, .16, tint(wall, .66))
    mesh.horizontal(0, 0, width, height, -.12, (.43, .37, .25))
    for obj in entries:
        x, z, w, d, h = (obj[k] for k in ("x", "z", "width", "depth", "height"))
        kind = obj["kind"]
        color = {"cabinet": (.83, .72, .40), "tv": (.43, .46, .77), "computer": (.75, .77, .81), "workstation": (.67, .71, .72), "table": (.84, .70, .36), "chair": (.86, .84, .73), "bed": (.86, .51, .45), "plant": (.43, .75, .46), "console": (.90, .90, .83), "stairs": (.86, .83, .71)}[kind]
        if data["map_id"] == 40 and kind in ("cabinet", "table"):
            color = (.75, .78, .76) if kind == "table" else (.70, .72, .66)
        if kind == "stairs":
            # Four low steps ascend away from the camera inside the warp area.
            for step in range(4):
                mesh.box(x, z + step * d / 4, w, d / 4, 0, h * (4 - step) / 4, color)
                mesh.box(x + .06, z + (step + 1) * d / 4 - .10, w - .12, .08, h * (4 - step) / 4, h * (4 - step) / 4 + .025, tint(color, .60))
        elif kind in ("table", "chair"):
            mesh.box(x + .08, z + .08, w - .16, d - .16, h - .16, h, color, a.tiles(x, z, w, max(1, d - 1)), front=a.tiles(x, z + d - 1, w, 1), sides=a.pattern("plank"))
            for lx in (x + .15, x + w - .4):
                for lz in (z + .15, z + d - .4):
                    mesh.box(lx, lz, .25, .25, 0, h - .16, tint(color, .64))
            # Fill original footprint beneath raised table with neutral floor,
            # otherwise the original projected tabletop would be duplicated.
            for fz in range(z, z + d):
                for fx in range(x, x + w):
                    mesh.horizontal(fx, fz, 1, 1, .005, tint(floor, .86), a.tiles(*sample))
            # Thin lip follows the tabletop; it adds separation without baking
            # fake items such as starter balls/Pokedex into a static scene.
            for ex in (x + .07, x + w - .14):
                mesh.box(ex, z + .07, .07, d - .14, h, h + .025, tint(color, .77))
        elif kind in ("tv", "computer"):
            # Avoid projecting the front screen a second time on the top face.
            # Their ROM footprint includes the projected height. Keep the CRT
            # at the lower collision cell so it cannot intersect the back wall.
            depth = min(1.4 if kind == "computer" else 1.1, d - .16)
            for fz in range(max(2, z), z + d):
                for fx in range(x, x + w):
                    mesh.horizontal(fx, fz, 1, 1, .005, tint(floor, .94), a.tiles(*sample))
            mesh.box(x + .06, z + d - depth - .06, w - .12, depth, 0, h, color,
                     front=a.tiles(x, z, w, d), sides=a.pattern("plank"))
            mesh.box(x + .02, z + d - depth - .09, w - .04, depth + .06, 0, .12, tint(color, .59))
        elif kind == "workstation":
            mesh.box(x, z, w, d, 0, h, color, sides=a.pattern("plank"))
            for n in range(w // 2):
                sx = x + n * 2
                mesh.box(sx + .15, z + .10, 1.65, .65, h, h + 1.0, (.72, .75, .73),
                         front=a.tiles(sx, z, 2, 2))
                mesh.box(sx + .30, z + 1.00, 1.35, .65, h, h + .06, (.82, .83, .78))
        elif kind == "plant":
            for fz in range(z, z + d):
                for fx in range(x, x + w):
                    mesh.horizontal(fx, fz, 1, 1, .005, tint(floor, .92), a.tiles(*sample))
            mesh.box(x + w * .25, z + d - 1.45, w * .5, 1.0, 0, h * .43, (.63, .43, .30))
            mesh.box(x + w * .44, z + d - 1.05, w * .12, .20, h * .4, h * .75, (.33, .40, .20))
            mesh.box(x + .06, z + d - 1.8, w - .12, 1.7, h * .66, h, color, a.tiles(x, z, w, 2))
            mesh.box(x + w * .2, z + d - 1.6, w * .6, 1.2, h, h + .25, tint(color, 1.05), a.tiles(x, z, w, 2))
        else:
            front_rows = min(d, 2)
            mesh.box(x, z, w, d, 0, h, color, a.tiles(x, z, w, max(1, d - front_rows)),
                     front=a.tiles(x, z + d - front_rows, w, front_rows), sides=a.pattern("plank"))
            if kind == "cabinet":
                mesh.box(x - .025, z + d - .05, w + .05, .10, h - .10, h + .025, tint(color, .71))
                mesh.box(x + .04, z + d + .01, w - .08, .10, .04, .15, tint(color, .56))
            elif kind == "bed":
                mesh.box(x + .16, z + .18, w - .32, .65, h, h + .15, (.91, .91, .84))
                mesh.box(x + .10, z + 1.02, w - .20, d - 1.14, h, h + .055, (.43, .53, .70))
    return 0, len(entries)


def make_world(data, exterior_profile, world_profile):
    legacy.validate_map(data)
    if data.get("map_id") not in maps.MAPS or data.get("name") != maps.MAPS[data["map_id"]][0]:
        raise ValueError("Unsupported world map")
    _, width, height, *_ = maps.MAPS[data["map_id"]]
    if data["tile_width"] != width * 4 or data["tile_height"] != height * 4:
        raise ValueError("World dimensions do not match supported map")
    mesh = Mesh(Atlas(data))
    buildings, furniture = outdoor(mesh, data, exterior_profile) if data["map_id"] == 0 else interior(mesh, data, world_profile)
    return mesh, {"map_id": data["map_id"], "name": data["name"], "vertices": mesh.count, "triangles": mesh.count // 3,
                  "texture_width": mesh.atlas.width, "texture_height": mesh.atlas.height,
                  "texture_bytes": len(mesh.atlas.pixels), "vertex_bytes": len(mesh.vertices),
                  "buildings": buildings, "furniture": furniture, "bounds_min": mesh.bounds_min, "bounds_max": mesh.bounds_max}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--symbols", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = private_directory(args.output_dir)
        if args.rom.stat().st_size != 1048576 or args.symbols.stat().st_size > 4 * 1024 * 1024:
            raise ValueError("Unsupported input size")
        rom = args.rom.read_bytes()
        maps.verify_rom(rom)
        symbol_data = args.symbols.read_bytes()
        if hashlib.sha256(symbol_data).hexdigest() != maps.SYMBOLS_SHA256:
            raise ValueError("Symbols do not match pinned revision")
        symbols = maps.parse_symbols(symbol_data.decode("utf-8-sig"))
        exterior = json.loads((ROOT / "tools/pokemon3d/pallet_height_profile.json").read_text())
        profile = json.loads((ROOT / "tools/pokemon3d/world_profile.json").read_text())
        scenes = []
        # Build all maps successfully in memory before producing artifacts.
        for map_id in maps.MAPS:
            data = maps.extract_map(rom, symbols, map_id)
            mesh, report = make_world(data, exterior, profile)
            scenes.append((data, mesh, report))
        output.mkdir(parents=True, exist_ok=True)
        for data, mesh, report in scenes:
            stem = output / ("map-" + str(data["map_id"]))
            blob = mesh.binary()
            stem.with_suffix(".r2scene").write_bytes(blob)
            stem.with_suffix(".json").write_text(json.dumps(data, indent=2) + "\n")
            png(stem.with_suffix(".atlas.png"), mesh.atlas.width, mesh.atlas.height, mesh.atlas.pixels)
            report["scene_bytes"] = len(blob)
            report["sha256"] = hashlib.sha256(blob).hexdigest()
        report = {"schema": "r2n64.pokemon-red.world-report.v1", "rom_sha256": maps.ROM_SHA256,
                  "symbols_sha256": maps.SYMBOLS_SHA256, "maps": [entry[2] for entry in scenes],
                  "height_source": "Authored geometry and materials; not original 3D game data",
                  "lighting": "Baked per-face shading and static approximate contact shadows",
                  "packaged_assets": False, "ps4_hardware_tested": False}
        (output / "world-report.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
        return 0
    except (ValueError, OSError, UnicodeError, KeyError, TypeError) as error:
        print("World build failed: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
