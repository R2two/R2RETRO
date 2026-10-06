#!/usr/bin/env python3
"""Preserve upstream license files and local patch/source provenance in the PKG."""
from pathlib import Path
import argparse
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("destination", type=Path)
args = parser.parse_args()
upstream = ROOT / "external/mupen64plus-next"
destination = args.destination / "Mupen64Plus-Next"
destination.mkdir(parents=True, exist_ok=True)
for path in sorted(upstream.rglob("*")):
    if path.is_file() and path.name.upper().startswith(("LICENSE", "COPYING")):
        target = destination / path.relative_to(upstream)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)

# libretro-common/libco normally keep their permissions in source comments, not
# standalone LICENSE files. Preserve the complete opening notices, including
# copyright lines and disclaimers, rather than reducing them to an SPDX label.
embedded = destination / "embedded-notices"
notice_count = 0
def stage_embedded(path, relative, source_url):
    global notice_count
    source = path.read_text(encoding="utf-8-sig")
    position = 0
    comments = []
    while match := re.match(r"\s*(/\*.*?\*/|(?://[^\n]*(?:\n|$))+)", source[position:], re.DOTALL):
        comments.append(match.group(1))
        position += match.end()
    notice = "\n\n".join(comments)
    if not re.search(r"copyright|licen[cs]e|public domain|permission is hereby", notice, re.IGNORECASE):
        return
    target = embedded / (relative.as_posix() + ".notice.txt")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("Source: " + source_url + "\n\n" + notice + "\n", encoding="utf-8")
    notice_count += 1

revision = "12edd2c74a517ff86dfa8cfc71ad75e4c10486d5"
for path in sorted((upstream / "libretro-common").rglob("*")):
    if path.is_file() and path.suffix.lower() in {".c", ".cpp", ".h", ".s"}:
        relative = path.relative_to(upstream)
        stage_embedded(path, relative,
                       f"https://github.com/libretro/mupen64plus-libretro-nx/blob/{revision}/{relative.as_posix()}")

# Each handheld core ships its own ABI header with its original copyright years.
for name, revision, relative, repository in [
    ("sameboy", "8230189896a8bb6598574d302ba0ad3658f98ab4", "libretro/libretro.h", "LIJI32/SameBoy"),
    ("mgba", "26b7884bc25a5933960f3cdcd98bac1ae14d42e2", "src/platform/libretro/libretro.h", "mgba-emu/mgba"),
]:
    path = ROOT / "external" / name / relative
    stage_embedded(path, Path(name) / relative,
                   f"https://github.com/{repository}/blob/{revision}/{relative}")
for path in sorted((ROOT / "external/patches").glob("*")):
    if path.is_file():
        target = destination / "R2N64-patches" / path.name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
(destination / "SOURCE.txt").write_text(
    "Mupen64Plus-Next / libretro\n"
    "https://github.com/libretro/mupen64plus-libretro-nx\n"
    "Revision: 12edd2c74a517ff86dfa8cfc71ad75e4c10486d5\n"
    "Local changes: R2N64-patches/ (included verbatim).\n"
    "Complete source is in the R2N64 workspace external/mupen64plus-next submodule; "
    "scripts/prepare-core.sh applies the patches to an archived copy for builds.\n"
    "This local development package includes no commercial ROMs. "
    "assets/diagnostic.z64 is generated from original R2N64 source in scripts/make_diagnostic_rom.py.\n",
    encoding="utf-8")
print(f"Core license notices staged: {destination}")
print(f"Complete embedded source notices staged: {notice_count}")
