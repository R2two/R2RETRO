#!/usr/bin/env python3
"""Opt-in local-ROM regression runs. Never copies or bundles the input ROM."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
PROFILES = {
    "handheld": ("auto", "hle", "4"),  # N64 options are ignored by portable cores.
    "cached-lle-4": ("cached", "lle", "4"),
    "auto-lle-4": ("auto", "lle", "4"),
    "cached-hle-4": ("cached", "hle", "4"),
    "auto-hle-4": ("auto", "hle", "4"),
    "auto-hle-1": ("auto", "hle", "1"),
}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--probe", type=Path, default=ROOT / "build/desktop/rom_probe")
    parser.add_argument("--core-library", type=Path,
                        default=ROOT / "build/core-lab/source/mupen64plus_next_libretro.so",
                        help="Exact N64 library linked by --probe, for isolated core comparisons")
    parser.add_argument("--output", type=Path, default=ROOT / "build/local-rom-tests")
    parser.add_argument("--frames", type=int, default=600)
    parser.add_argument("--scenario", choices=("intro", "scripted"), default="intro")
    parser.add_argument("--profiles", default="cached-lle-4,auto-lle-4,cached-hle-4,auto-hle-4")
    parser.add_argument("--repetitions", type=int, default=1)
    parser.add_argument("--sessions", type=int, choices=(1, 2), default=1)
    parser.add_argument("--session-data", choices=("shared", "fresh"), default="shared")
    parser.add_argument("--timeout", type=float, default=300)
    parser.add_argument("--require-av", action="store_true", help="Fail a case without visible pixels or nonzero PCM")
    parser.add_argument("--require-jit", action="store_true", help="Require active JIT in automatic profiles")
    parser.add_argument("--require-hle", action="store_true", help="Require processed HLE tasks in HLE profiles")
    parser.add_argument("--profile-core", action="store_true", help="Enable and validate cumulative component timing")
    args = parser.parse_args()
    rom, probe = args.rom.resolve(strict=True), args.probe.resolve(strict=True)
    core_library = args.core_library.resolve(strict=True)
    profiles = args.profiles.split(",")
    if not rom.is_file() or any(name not in PROFILES for name in profiles):
        parser.error("Expected a regular local ROM and known profile names")
    if not 120 <= args.frames <= 36000 or not 1 <= args.repetitions <= 10 or args.timeout <= 0:
        parser.error("frames=120..36000, repetitions=1..10, timeout>0 required")
    digest = sha256(rom)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output = args.output.resolve() / f"{stamp}-{os.getpid()}"
    output.mkdir(parents=True, exist_ok=False)
    result = {
        "rom_name": rom.name, "rom_bytes": rom.stat().st_size, "rom_sha256": digest,
        "probe_sha256": sha256(probe),
        "core_library_path": str(core_library),
        "core_library_sha256": sha256(core_library),
        "handheld_library_sha256": {
            name: sha256(ROOT / f"build/handheld-linux/lib/lib{name}_libretro.a")
            for name in ("sameboy", "mgba")
        },
        "console_library_sha256": {
            name: sha256(ROOT / f"build/console-linux/lib/lib{name}_libretro.a")
            for name in ("fceumm", "bsnes_mercury")
            if (ROOT / f"build/console-linux/lib/lib{name}_libretro.a").is_file()
        },
        "platform": "desktop-linux", "ps4_hardware_tested": False,
        "scenario": args.scenario, "frames_per_session": args.frames,
        "sessions_per_case": args.sessions, "cases": [],
        "session_data_mode": args.session_data,
        "component_profiling": args.profile_core,
        "requirements": {"av": args.require_av, "jit": args.require_jit, "hle": args.require_hle},
        "scope": "Automated local-ROM runs with synthetic input; no ROM is copied or packaged.",
    }
    manifest = output / "matrix.json"
    print(f"Evidence: {output}", flush=True)
    for repetition in range(args.repetitions):
        # Reverse the second pass to reduce order/temperature bias.
        order = profiles if repetition % 2 == 0 else list(reversed(profiles))
        for name in order:
            case_dir = output / f"{repetition + 1}-{name}"
            case_dir.mkdir()
            command = [str(probe), str(rom), str(case_dir), *PROFILES[name],
                       str(args.frames), args.scenario, str(args.sessions), args.session_data,
                       "profile" if args.profile_core else "off"]
            start = time.monotonic()
            print(f"Running {name}, pass {repetition + 1}...", flush=True)
            timed_out = False
            with (case_dir / "stdout.log").open("w") as stdout, (case_dir / "stderr.log").open("w") as stderr:
                child = subprocess.Popen(command, stdout=stdout, stderr=stderr, start_new_session=True)
                try:
                    status = child.wait(timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    timed_out = True
                    os.killpg(child.pid, signal.SIGKILL)
                    status = child.wait()
            reports = sorted(case_dir.glob("**/report.json"))
            failures = []
            for report_path in reports:
                report = json.loads(report_path.read_text())
                sessions = report.get("sessions", [])
                if len(sessions) != args.sessions:
                    failures.append("Unexpected session count")
                for session in sessions:
                    if session["vi_count"] != args.frames:
                        failures.append("Incomplete emulated interval count")
                    if args.require_av and (not session["nonblack_frames"] or not session["audio_nonzero_samples"]):
                        failures.append("Missing visible video or nonzero audio")
                    if args.require_jit and PROFILES[name][0] == "auto" and not session["jit_active"]:
                        failures.append("Requested automatic CPU did not execute JIT")
                    if PROFILES[name][0] == "cached" and session["jit_active"]:
                        failures.append("Cached profile unexpectedly executed JIT")
                    if args.require_hle and PROFILES[name][1] == "hle" and not session["audio_hle_tasks_total"]:
                        failures.append("Requested audio HLE processed no tasks")
                    if PROFILES[name][1] == "lle" and session["audio_hle_tasks_total"]:
                        failures.append("Disabled HLE unexpectedly processed tasks")
                    counters = session.get("profile_cumulative", {})
                    if args.profile_core:
                        if counters.get("run_calls") != args.frames or not counters.get("run_us"):
                            failures.append("Profiler did not measure every emulated interval")
                        if counters.get("timer_failures", 1) or counters.get("dropped_scopes", 1):
                            failures.append("Profiler reported timer or nesting failures")
                        if sum(counters.get(key, 0) for key in ("rsp_us", "rdp_us", "scanout_us", "audio_hle_us")) > counters.get("run_us", 0):
                            failures.append("Exclusive component time exceeds inclusive core time")
                    elif any(counters.values()):
                        failures.append("Disabled profiler unexpectedly accumulated counters")
            case = {"profile": name, "repetition": repetition + 1, "returncode": status,
                    "timed_out": timed_out, "wall_seconds": round(time.monotonic() - start, 3),
                    "directory": str(case_dir.relative_to(output)),
                    "check_failures": failures,
                    "reports": [str(path.relative_to(output)) for path in reports]}
            result["cases"].append(case)
            manifest.write_text(json.dumps(result, indent=2) + "\n")
            print(f"Finished {name}: exit={status}, timeout={timed_out}, {case['wall_seconds']:.2f}s", flush=True)
            for failure in failures:
                print(f"CHECK FAILED ({name}): {failure}", flush=True)
    # A final digest proves the harness did not alter the user's source file.
    result["rom_unchanged"] = sha256(rom) == digest
    result["all_processes_passed"] = result["rom_unchanged"] and all(
        c["returncode"] == 0 and not c["timed_out"] and c["reports"] for c in result["cases"])
    result["requested_checks_passed"] = result["all_processes_passed"] and all(
        not c["check_failures"] for c in result["cases"])
    manifest.write_text(json.dumps(result, indent=2) + "\n")
    print("Requested checks: " + ("PASS" if result["requested_checks_passed"] else "FAIL"), flush=True)
    print(f"Completed: {manifest}", flush=True)
    return 0 if result["requested_checks_passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
