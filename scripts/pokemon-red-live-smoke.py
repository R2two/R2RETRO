#!/usr/bin/env python3
"""Opt-in local cartridge test. No ROM, memory patches or fixtures distributed."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    visual = parser.add_mutually_exclusive_group(required=True)
    visual.add_argument("--scene", type=Path)
    visual.add_argument("--world", type=Path)
    parser.add_argument("--exe", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    before = hashlib.sha256(args.rom.read_bytes()).hexdigest()
    if before != "5ca7ba01642a3b27b0cc0b5349b52792795b62d3ed977e98a09390659af96b7b":
        raise ValueError("Unsupported cartridge revision")
    output = Path(tempfile.mkdtemp(prefix="red-live-smoke-", dir=root / "build"))
    phases = [
        ("intro", 14000, "600 WAIT\n12 START\n180 WAIT\n" + "8 A\n52 WAIT\n" * 220),
        ("bedroom", 900, "8 B\n80 WAIT\n" * 5 + "24 RIGHT\n16 WAIT\n100 UP\n16 WAIT\n80 RIGHT\n180 WAIT\n"),
        ("house", 500, "96 DOWN\n24 WAIT\n64 LEFT\n24 WAIT\n80 DOWN\n180 WAIT\n"),
        ("roundtrip", 1020, "64 RIGHT\n48 DOWN\n48 UP\n64 LEFT\n64 UP\n120 WAIT\n128 DOWN\n120 WAIT\n12 START\n60 WAIT\n12 B\n180 WAIT\n"),
    ]
    if args.world:
        # The bedroom phase ends next to the stairs on 1F. Walk next to Mom,
        # speak, and retain the earlier checkpoint for the exit-house phase.
        phases.insert(2, ("mother-dialogue", 356,
                         "48 DOWN\n20 WAIT\n20 LEFT\n20 WAIT\n8 A\n240 WAIT\n"))
        if (args.world / "map-40.r2scene").is_file():
            phases += [
                ("oak-escort", 3812, "32 UP\n16 WAIT\n80 RIGHT\n16 WAIT\n128 UP\n300 WAIT\n" + "8 A\n100 WAIT\n" * 30),
                ("oak-close", 2256, "8 B\n180 WAIT\n" * 12),
            ]
    reports = []
    env = dict(os.environ, LIBGL_ALWAYS_SOFTWARE="1", MESA_GLES_VERSION_OVERRIDE="2.0")
    for index, (name, frames, inputs) in enumerate(phases):
        script = output / (name + ".txt")
        script.write_text(inputs, encoding="ascii")
        visual_arg = ["--world", str(args.world.resolve())] if args.world else ["--scene", str(args.scene.resolve())]
        cmd = [str(args.exe.resolve()), "--rom", str(args.rom.resolve()),
               *visual_arg, "--data", str(output / "session"),
               "--script", str(script), "--frames", str(frames)]
        if index:
            cmd.append("--resume")
        if name in ("roundtrip", "oak-close"):
            cmd.append("--verify-lifecycle")
        if name == "mother-dialogue":
            cmd.append("--no-save")
        if args.world and name != "intro":
            cmd.append("--render-all")
        result = subprocess.run(cmd, env=env, capture_output=True, text=True, timeout=180)
        (output / (name + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise RuntimeError(f"{name} failed; see {output}")
        run = max((output / "session").glob("run-*"), key=lambda p: p.name)
        report = json.loads((run / "live-report.json").read_text())
        reports.append(dict(phase=name, evidence=str(run), **report))
        if name == "roundtrip":
            with (run / "trace.csv").open() as stream:
                trace = list(csv.DictReader(stream))
            assert report["pallet_positions"] >= 10, "Player did not move through Pallet"
            assert any(row["map"] == "37" for row in trace), "House not entered"
            assert any(row["overlay"] == "1" and row["supported"] == "1" for row in trace), "Menu overlay absent"
            assert trace[-1]["pallet"] == "1", "Did not return to 3D"
            assert report["lifecycle_restore_checks"] == 3, "Restore checks absent"
            assert report["player_animation_frames"] >= 4, "Direction/walk frames did not change"
            assert report["player_pixel_positions"] > report["world_positions"] * 2, "Sub-cell motion absent"
            if args.world:
                assert report["map37_frames"] > 60, "Interior did not render"
                assert report["rendered_frames"] == frames, "Frame rendering incomplete"
        if name == "mother-dialogue":
            assert report["map37_frames"] > 200 and report["overlay_frames"] > 60, "Interior dialogue missing"
        if args.world and name == "bedroom":
            assert report["map38_frames"] > 60 and report["map37_frames"] > 60, "Both floors must render"
        if name == "oak-escort":
            assert report["map40_frames"] > 1000, "Oak escort did not reach the laboratory"
        if name == "oak-close":
            assert report["map40_frames"] > 2000 and report["lifecycle_restore_checks"] == 3
    assert hashlib.sha256(args.rom.read_bytes()).hexdigest() == before, "ROM changed"
    (output / "summary.json").write_text(json.dumps(reports, indent=2) + "\n")
    print(f"PASS: {sum(r['frames'] for r in reports)} frames, real movement/menu/house/restore; {output}")


if __name__ == "__main__":
    main()
