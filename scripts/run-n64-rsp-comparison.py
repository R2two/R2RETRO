#!/usr/bin/env python3
"""Opt-in Linux/Mesa RSP comparison; input ROMs stay outside the repository.

Builds a standalone host, never changes the frontend, core, saves or PKG.
Each process starts fresh with 300 warmup VI and no controller input.
Before patch 0008, HLE uses the upstream full RSP (different audio path).
With patch 0008, HLE uses recognized graphics plus the same CXD4/audio path.
The report distinguishes these profiles by the optional graphics counters.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("roms", nargs="+", type=Path)
    parser.add_argument("--vi", type=int, default=1800)
    parser.add_argument("--label", default="comparison")
    args = parser.parse_args()
    if not 600 <= args.vi <= 18000 or not args.label.replace("-", "").isalnum():
        parser.error("VI must be 600..18000 and label alphanumeric/hyphen")
    root = Path(__file__).resolve().parents[1]
    core = root / "build/core-lab/source/mupen64plus_next_libretro.so"
    if not core.is_file():
        parser.error("Build the pinned desktop GLES2 core first")
    roms = [p.resolve(strict=True) for p in args.roms]
    output = root / "build/rsp-comparison" / args.label
    output.mkdir(parents=True, exist_ok=False)
    compiler = ["c++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror",
                str(root / "tools/n64_rsp_probe.cpp"), "-I" + str(root / "include"),
                "-I" + str(root / "external/mupen64plus-next/libretro-common/include")]
    compiler += shlex.split(subprocess.check_output(
        ["pkg-config", "--cflags", "--libs", "sdl2", "SDL2_image"], text=True))
    compiler += ["-ldl", "-lGLESv2", "-o", str(output / "probe")]
    subprocess.run(compiler, check=True)
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", LIBGL_ALWAYS_SOFTWARE="1",
               MESA_GLES_VERSION_OVERRIDE="2.0", SDL_AUDIODRIVER="dummy",
               MESA_SHADER_CACHE_DISABLE="true")
    summary = {"platform": "Linux WSL / Mesa llvmpipe, NOT PS4", "core_sha256": sha(core),
               "audio_path": "unknown", "cases": []}
    for index, rom in enumerate(roms):
        before = sha(rom)
        # Reverse the order in the second pair to reduce order bias.
        for repeat, modes in enumerate((('cxd4', 'hle'), ('hle', 'cxd4'))):
            for rsp in modes:
                case = output / f"rom{index}-{rsp}-{repeat}"
                case.mkdir()
                with (case / "process.log").open("w") as log:
                    run = subprocess.run([str(output / "probe"), str(core), str(rom),
                                          str(case), rsp, str(args.vi)], env=env,
                                         stdout=log, stderr=subprocess.STDOUT, timeout=420)
                after = sha(rom)
                result = {"rom": str(rom), "sha256": before, "rom_unchanged": before == after,
                          "rsp": rsp, "repeat": repeat, "exit": run.returncode, "path": str(case)}
                if (case / "result.json").exists():
                    result.update(json.loads((case / "result.json").read_text()))
                    summary["audio_path"] = ("shared-cxd4-audio-hle" if "graphics_hle_tasks" in result
                                             else "upstream-full-hle-versus-cxd4")
                summary["cases"].append(result)
                (output / "results.json").write_text(json.dumps(summary, indent=2) + "\n")
                print(json.dumps(result), flush=True)
                if run.returncode or before != after:
                    raise SystemExit("Probe failed; see isolated process.log")
    if sha(core) != summary["core_sha256"]:
        raise SystemExit("Core changed during comparison; discard results")
    summary["pcm_equal_per_rom"] = {
        str(rom): len({c.get("pcm_hash") for c in summary["cases"] if c["rom"] == str(rom)}) == 1
        and all(c.get("pcm_hash") is not None for c in summary["cases"] if c["rom"] == str(rom))
        for rom in roms}
    (output / "results.json").write_text(json.dumps(summary, indent=2) + "\n")


if __name__ == "__main__":
    main()
