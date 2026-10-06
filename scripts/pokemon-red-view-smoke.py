#!/usr/bin/env python3
"""Opt-in comparison: presentation switches must not alter a saved game."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("rom", "world", "exe", "session"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if (root / "build").resolve() not in args.session.resolve().parents:
        raise ValueError("Use a disposable session checkpoint inside build/")
    rom_hash = hashlib.sha256(args.rom.read_bytes()).hexdigest()
    if rom_hash != "5ca7ba01642a3b27b0cc0b5349b52792795b62d3ed977e98a09390659af96b7b":
        raise ValueError("Unsupported cartridge revision")
    output = Path(tempfile.mkdtemp(prefix="red-view-smoke-", dir=root / "build"))
    env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1", MESA_GLES_VERSION_OVERRIDE="2.0")

    def run(data, name, inputs, frames, view=None):
        script = output / (name + ".txt")
        script.write_text(inputs, encoding="ascii")
        cmd = [str(args.exe.resolve()), "--rom", str(args.rom.resolve()), "--world", str(args.world.resolve()),
               "--data", str(data), "--script", str(script), "--frames", str(frames),
               "--resume", "--no-save", "--render-all"]
        if view:
            cmd += ["--view", view]
        result = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=120)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise RuntimeError(f"{name} failed; see {output}")
        evidence = max(data.glob("run-*"), key=lambda p: p.name)
        return evidence, json.loads((evidence / "live-report.json").read_text())

    for name in ("baseline", "switches"):
        for folder in ("states", "saves"):
            if (args.session / folder).is_dir():
                shutil.copytree(args.session / folder, output / name / folder)
    baseline, first = run(output / "baseline", "baseline", "120 WAIT\n", 120, "3d")
    switched, second = run(output / "switches", "switches",
                           "30 WAIT\n1 TOGGLE3D\n29 WAIT\n1 TOGGLE3D\n29 WAIT\n1 TOGGLE3D\n29 WAIT\n", 120, "3d")
    for artifact in ("last-wram.bin", "game-last.png"):
        assert (baseline / artifact).read_bytes() == (switched / artifact).read_bytes(), artifact + " changed with presentation"
    assert second["view_switches"] == 3 and not second["view_final_enabled"]
    assert second["view3d_frames"] > 20 and second["view2d_frames"] > 20
    assert first["view3d_frames"] > 100, "Checkpoint must be inside a supported 3D map"
    _, persisted = run(output / "switches", "persisted-off", "4 WAIT\n", 4)
    assert not persisted["view_initial_enabled"] and persisted["view3d_frames"] == 0
    _, restored = run(output / "switches", "override-on", "4 WAIT\n", 4, "3d")
    assert restored["view_initial_enabled"] and restored["view3d_frames"] > 0
    assert hashlib.sha256(args.rom.read_bytes()).hexdigest() == rom_hash
    (output / "summary.json").write_text(json.dumps({
        "wram_equal": True, "original_image_equal": True, "switches": second,
        "persistence_verified": True, "rom_unchanged": True, "baseline": str(baseline),
        "switched": str(switched), "ps4_hardware_tested": False,
    }, indent=2) + "\n")
    print(f"PASS: 3D/2D switches, equal game memory/image, persistence and override; {output}")


if __name__ == "__main__":
    main()
