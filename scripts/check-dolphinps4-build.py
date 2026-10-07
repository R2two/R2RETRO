#!/usr/bin/env python3
"""Read-only WSL preflight for the pinned DolphinPS4 source; never builds a PKG.

This is a dependency inventory, not an emulation or console compatibility test.
It does not install tools, change OpenOrbis, fetch code, or execute upstream scripts.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def revision(path):
    try:
        return subprocess.check_output(
            ["git", "-C", str(path), "rev-parse", "HEAD"],
            text=True, stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=root / "build/references/DolphinPS4")
    parser.add_argument("--build-root", type=Path, default=root / "build/dolphin-isolated")
    parser.add_argument("--openorbis", type=Path,
                        default=Path(os.environ.get("OPENORBIS", "/opt/pacbrew/ps4/openorbis")))
    args = parser.parse_args()
    lock = json.loads((root / "external/dolphinps4.lock.json").read_text())
    build = args.build_root.resolve()
    paths = [build / "mesa-venv/bin", build / "host-tools/bin"]
    search_path = os.pathsep.join(map(str, paths)) + os.pathsep + os.environ.get("PATH", "")
    tool_names = ("clang-21", "clang++-21", "ld.lld-21", "llvm-ar-21", "llvm-ranlib-21",
                  "llvm-strip-21", "cmake", "ninja", "meson", "glslangValidator", "pkg-config")
    found = {name: shutil.which(name, path=search_path) for name in tool_names}
    required = {
        "openorbis_headers": args.openorbis / "include/orbis",
        "libcxx21": build / "sysroot/lib/libc++.a",
        "libcxxabi21": build / "sysroot/lib/libc++abi.a",
        "libc_header_overlay": build / "sysroot/libc-overlay/time.h",
        "mesa_source": build / "src/mesa/meson.build",
        "libdrm_headers": build / "src/libdrm/amdgpu/amdgpu.h",
        "radv_library": build / "build/mesa/src/amd/vulkan/libvulkan_radeon.a",
    }
    missing = [name for name, path in found.items() if not path]
    missing += [name for name, path in required.items() if not path.exists()]
    source_revision = revision(args.source)
    if source_revision != lock["revision"]:
        missing.append("pinned_dolphinps4_source")
    if revision(build / "src/dolphin") != lock["dolphin_revision"]:
        missing.append("pinned_dolphin_source")
    if sys.platform != "linux":
        missing.append("run_this_check_in_wsl_or_linux")
    print(json.dumps({
        "status": "dependencies_missing" if missing else "inventory_present_not_build_verified",
        "integration_available": False,
        "source_revision": source_revision,
        "build_root": str(build),
        "tools": found,
        "files": {name: {"path": str(path), "present": path.exists()}
                  for name, path in required.items()},
        "missing": missing,
        "limits": "Does not validate applied patches, ABI, linking, SELF launch, GPU or game compatibility."
    }, indent=2))
    return 2 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
