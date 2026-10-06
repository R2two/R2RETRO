#!/usr/bin/env python3
"""Namespace definitions AND their internal references; leave libc imports intact."""
from pathlib import Path
import subprocess
import sys

name, source, destination = sys.argv[1:]
if name not in ("sameboy", "mgba", "fceumm", "bsnes_mercury"):
    raise SystemExit("Unsupported core namespace")
source, destination = Path(source), Path(destination)
result = subprocess.run(["nm", "-g", "--defined-only", "--format=posix", str(source)], check=True, text=True, capture_output=True)
symbols = sorted({line.split()[0] for line in result.stdout.splitlines() if len(line.split()) >= 3 and line.split()[1] in "ABCDGIRSTVWu"})
required = {"retro_init", "retro_deinit", "retro_run", "retro_load_game", "retro_unload_game", "retro_serialize", "retro_unserialize"}
if not required.issubset(symbols):
    raise SystemExit(f"Missing required libretro symbols: {required.difference(symbols)}")
mapping = destination.with_suffix(".symbols")
mapping.write_text("".join(f"{s} {name}_{s}\n" for s in symbols))
subprocess.run(["objcopy", "--redefine-syms=" + str(mapping), str(source), str(destination)], check=True)
subprocess.run(["ar", "s", str(destination)], check=True)
exports = subprocess.run(["nm", "-g", "--defined-only", str(destination)], check=True, text=True, capture_output=True).stdout
destination.with_suffix(".exports").write_text(exports)
for symbol in required:
    assert f" {name}_{symbol}\n" in exports, symbol
    assert f" {symbol}\n" not in exports, symbol
print(f"Namespaced {len(symbols)} definitions: {destination}")
