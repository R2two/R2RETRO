#!/usr/bin/env python3
"""Validate the OpenOrbis package before publishing a local dist artifact."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
TITLE_ID = "RNTD00064"
CONTENT_ID = f"IV0001-{TITLE_ID}_00-R2N64APP00000001"
# Preserve installed application/save identity when changing the public brand.
APP_TITLE = "R2RETRO"
VERSION = "0.5.6"
SFO_VERSION = "00.56"
OVERLAYS = {"gb": "gb.png", "gbc": "gbc.png", "gba": "gba.png", "snes": "snes.jpg"}


def rebuild_gp4_directories(path):
    """Describe each file's full parent path, including repeated directory names."""
    project = ET.parse(path)
    root = project.getroot()
    files = root.find("files")
    directories = root.find("rootdir")
    if files is None or directories is None:
        raise RuntimeError("GP4 is missing files or rootdir")
    # The installed create-gp4 merges equal basenames from different parents.
    # A nested notice such as common/audio/x and common/include/audio/y must
    # instead have two distinct audio directories for PkgTool's PFS builder.
    for child in list(directories):
        directories.remove(child)
    parents = {(): directories}
    for entry in files.findall("file"):
        target = PurePosixPath(entry.attrib["targ_path"].replace("\\", "/"))
        if target.is_absolute() or not target.parts or ".." in target.parts:
            raise RuntimeError(f"Invalid GP4 target path: {target}")
        parts = target.parent.parts
        for depth in range(1, len(parts) + 1):
            key = parts[:depth]
            if key not in parents:
                parents[key] = ET.SubElement(parents[key[:-1]], "dir", targ_name=key[-1])
    ET.indent(project, space="\t")
    project.write(path, encoding="utf-8", xml_declaration=True)


def read_sfo(path):
    data = path.read_bytes()
    magic, version, keys, values, count = struct.unpack_from("<4sIIII", data)
    if magic != b"\x00PSF":
        raise ValueError("Invalid SFO magic")
    entries = {}
    for i in range(count):
        key, fmt, length, capacity, offset = struct.unpack_from("<HHIII", data, 20 + i * 16)
        name = data[keys + key:].split(b"\0", 1)[0].decode()
        raw = data[values + offset:values + offset + length]
        entries[name] = struct.unpack("<I", raw)[0] if fmt == 0x404 else raw.rstrip(b"\0").decode()
    return entries


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, required=True)
    args = parser.parse_args()
    package = args.build / f"{CONTENT_ID}.pkg"
    tool = args.toolchain / "bin/linux/PkgTool.Core"
    env = dict(os.environ, DOTNET_SYSTEM_GLOBALIZATION_INVARIANT="1")

    def run(*commands):
        result = subprocess.run([str(tool), *map(str, commands)], env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(result.stdout, end="")
        result.check_returncode()
        return result.stdout

    payload = args.build / "payload"
    # Incremental app/package builds must include the patches actually used by
    # the rebuilt core, even when build.sh did not restage notices this time.
    subprocess.run([sys.executable, str(ROOT / "scripts/stage-core-licenses.py"),
                    str(payload / "licenses")], check=True)
    shutil.copy2(args.build / "eboot.bin", payload / "eboot.bin")
    # Refresh the home artwork for incremental package builds as well.
    shutil.copy2(ROOT / "assets/background-room.jpg", payload / "assets/background-room.jpg")
    shutil.copy2(ROOT / "assets/console-logos.png", payload / "assets/console-logos.png")
    # Test cartridges belong outside the payload. Only our original bundled
    # N64 diagnostic is allowed; opt-in user ROMs must never enter a package.
    cartridge_extensions = {".gb", ".gbc", ".gba", ".z64", ".v64", ".n64", ".nes", ".sfc", ".smc", ".fds"}
    for path in payload.rglob("*"):
        if path.is_file() and path.suffix.lower() in cartridge_extensions:
            if path.relative_to(payload).as_posix() != "assets/diagnostic.z64":
                raise RuntimeError(f"Unexpected cartridge in package payload: {path}")
    sfo_path = payload / "sce_sys/param.sfo"
    run("sfo_new", sfo_path)
    entries = {
        "APP_TYPE": ("Integer", 4, 1), "APP_VER": ("Utf8", 8, SFO_VERSION),
        "ATTRIBUTE": ("Integer", 4, 0), "CATEGORY": ("Utf8", 4, "gde"),
        "FORMAT": ("Utf8", 4, "obs"), "CONTENT_ID": ("Utf8", 48, CONTENT_ID),
        "DOWNLOAD_DATA_SIZE": ("Integer", 4, 0), "SYSTEM_VER": ("Integer", 4, 1020),
        "TITLE": ("Utf8", 128, APP_TITLE), "TITLE_ID": ("Utf8", 12, TITLE_ID),
        "VERSION": ("Utf8", 8, SFO_VERSION),
    }
    for name, (kind, capacity, value) in entries.items():
        run("sfo_setentry", sfo_path, name, "--type", kind, "--maxsize", capacity, "--value", value)
    gp4 = payload / "r2n64.gp4"
    gp4.unlink(missing_ok=True)
    subprocess.run([str(args.toolchain / "bin/linux/create-gp4"), "-out", str(gp4),
                    "--content-id", CONTENT_ID, "--path", str(payload)], env=env, check=True)
    try:
        rebuild_gp4_directories(gp4)
        run("pkg_build", gp4, args.build)
    finally:
        gp4.unlink(missing_ok=True)
    validation = run("pkg_validate", package)
    # PkgTool may report validation failures without a nonzero exit code.
    if not validation.strip() or any(not line.startswith("[OK]") for line in validation.splitlines() if line.strip()):
        raise RuntimeError("PkgTool rejected package validation")
    entry_listing = run("pkg_listentries", package)
    indices = {parts[-1]: int(parts[3]) for line in entry_listing.splitlines()
               if (parts := line.split()) and parts[0].startswith("0x")}
    with tempfile.TemporaryDirectory(prefix="r2n64-pkg-") as temporary:
        extracted = Path(temporary)
        run("pkg_extract", package, extracted)
        run("pkg_extractentry", package, indices["PARAM_SFO"], extracted / "param.sfo")
        run("pkg_extractentry", package, indices["ICON0_PNG"], extracted / "icon0.png")
        sfo = read_sfo(extracted / "param.sfo")
        expected = {"TITLE": APP_TITLE, "TITLE_ID": TITLE_ID, "CONTENT_ID": CONTENT_ID,
                    "APP_VER": SFO_VERSION, "VERSION": SFO_VERSION}
        for key, value in expected.items():
            if sfo.get(key) != value:
                raise RuntimeError(f"Unexpected {key}: {sfo.get(key)!r}")
        payloads = {"uroot/eboot.bin": args.build / "eboot.bin",
                    "uroot/assets/diagnostic.z64": ROOT / "assets/diagnostic.z64",
                    "uroot/assets/background.jpg": ROOT / "assets/background.jpg",
                    "uroot/assets/background-room.jpg": ROOT / "assets/background-room.jpg",
                    "uroot/assets/console-logos.png": ROOT / "assets/console-logos.png",
                    "icon0.png": ROOT / "pkg/icon0.png",
                    "uroot/assets/fonts/DejaVuSans.ttf": ROOT / "assets/fonts/DejaVuSans.ttf"}
        for filename in OVERLAYS.values():
            payloads[f"uroot/assets/overlays/{filename}"] = ROOT / f"assets/overlays/{filename}"
        payloads["uroot/assets/certs/cacert.pem"] = ROOT / "assets/certs/cacert.pem"
        for notice in (payload / "licenses").rglob("*"):
            if notice.is_file():
                payloads["uroot/" + notice.relative_to(payload).as_posix()] = notice
        for relative, original in payloads.items():
            if (extracted / relative).read_bytes() != original.read_bytes():
                raise RuntimeError(f"Package payload mismatch: {relative}")
    elf = args.build / "r2n64"
    if elf.read_bytes()[:4] != b"\x7fELF":
        raise RuntimeError("Missing ELF output")
    dist = ROOT / "dist"
    dist.mkdir(exist_ok=True)
    destination = dist / f"R2RETRO-v{VERSION}-color-stability.pkg"
    shutil.copy2(package, destination)
    shutil.copy2(elf, args.build / "R2RETRO.elf")
    digest = hashlib.sha256(destination.read_bytes()).hexdigest()
    (dist / (destination.name + ".sha256")).write_text(f"{digest}  {destination.name}\n")
    (dist / "build-info.json").write_text(json.dumps({
        "version": VERSION, "milestone": "color-stability-preview", "title": APP_TITLE,
        "gb_color": "optional-four-tone-DMG-palette; live-toggle; persisted; restores-base-palette",
        "video_delivery": "serial-based-texture-reuse; native-mGBA-duplicate-callbacks; separate-delivery-metric",
        "nes_region": "explicit-auto; clean-iNES1-PAL-header-hint; no-overclock; original-sprite-limit",
        "about": "Rtwo-R2; feature-summary; core-credits; https://ko-fi.com/rtwo_",
        "display_shaders": "optional-GLES2-LCD-grid-CRT-scanlines; per-system; default-off; GB-GBC-GBA-NES-SNES",
        "gba_frameskip": "native-mGBA-0-1-2; default-off; per-system",
        "manual_cartridge_save": "paused-SRAM-RTC; atomic-per-file; GB-GBC-GBA-NES-SNES",
        "console_logo_upload": "six-POT-ARGB8888-textures; explicit-checked-upload; vector-fallback",
        "title_id": TITLE_ID, "compatible_data_root": "/data/R2N64",
        "handheld_features": ["fast-forward-2-4-8", "five-state-slots", "gb-palettes", "video-options", "per-system-preferences", "optional-system-overlays", "optional-performance-hud"],
        "app_features": ["paused-png-capture-per-system", "libretro-direct-https-metadata", "offline-library-artwork", "optional-snes-overlay", "r2retro-brand", "ps4-verified-path-file-operations", "scanner-operation-errors", "verified-assets-after-sandbox-transition", "ca-read-diagnostics", "compressed-overlays-before-goldhen", "opaque-xrgb-game-texture", "paused-video-buffer-diagnostic"],
        "library_sources": ["https://github.com/libretro/libretro-database", "https://github.com/libretro-thumbnails/libretro-thumbnails"],
        "library_downloads": "manual-selected-game; verified-TLS; background-worker; local-cache",
        "updater": {"channel_default": "experimental", "automatic_check": True,
                    "dns": "fixed-Cloudflare-DoH; updates-and-libretro-catalog; no-user-override",
                    "diagnostics": "stage-elapsed; cooperative-cancel-45s-check-watchdog",
                    "transport": "HTTPS-stream-to-disk; SHA256; bounded-PKG-SFO",
                    "installation": "BGFT-local-storage; verified-data-user-data-alias; explicit-confirmation; no-uninstall",
                    "ps4_self_update_verified": False},
        "console_folders": True, "n64_automatic_profiles": "experimental-exact-identity-Mario-USA-Zelda-USA1.2",
        "home_background_sha256": hashlib.sha256((ROOT / "assets/background-room.jpg").read_bytes()).hexdigest(),
        "console_logos_sha256": hashlib.sha256((ROOT / "assets/console-logos.png").read_bytes()).hexdigest(),
        "ca_bundle_sha256": hashlib.sha256((ROOT / "assets/certs/cacert.pem").read_bytes()).hexdigest(),
        "core_diagnostics": "aggregate-exact-mgba-dma-info-preserve-warnings-errors",
        "overlay_sha256": {system: hashlib.sha256((ROOT / f"assets/overlays/{filename}").read_bytes()).hexdigest()
                           for system, filename in OVERLAYS.items()},
        "systems": ["Nintendo 64", "Game Boy", "Game Boy Color", "Game Boy Advance", "NES", "SNES"],
        "console_cores": {"fceumm": "7a542dab1e87679921962a9f056186eca425c0c2",
                          "bsnes_mercury": "79d7f9de218b6ffa65a80bbdc5828532bc239232"},
        "console_profile": "FCEUmm/no-hdpack; bsnes-mercury/performance/chip-HLE/SuperFX100%",
        "console_integration_sha256": hashlib.sha256((ROOT / "scripts/prepare-console-cores.py").read_bytes()).hexdigest(),
        "handheld_cores": {"sameboy": "8230189896a8bb6598574d302ba0ad3658f98ab4",
                           "mgba": "26b7884bc25a5933960f3cdcd98bac1ae14d42e2"},
        "icon_sha256": hashlib.sha256((ROOT / "pkg/icon0.png").read_bytes()).hexdigest(),
        "logo_source_sha256": hashlib.sha256((ROOT / "assets/logo.png").read_bytes()).hexdigest(),
        "sfo": sfo, "pkg_bytes": destination.stat().st_size, "sha256": digest,
        "toolchain": str(args.toolchain), "package_validated": True,
        "ps4_hardware_tested": False, "n64_core_integrated": True,
        "last_user_confirmed_boot_version": "0.5.3",
        "core_revision": "12edd2c74a517ff86dfa8cfc71ad75e4c10486d5",
        "core_profile": "guarded_x64_dynarec/angrylion-4-workers/cxd4-sse2-audio-hle",
        "component_profiling_default": False, "gpu_probe_manual_only": True,
        "n64_gpu_renderer_enabled": "optional-experimental-gliden64-gles2-320x240",
        "n64_gpu_default": False, "frontend_gpu_frame_readback": False,
        "n64_gpu_rsp": "recognized-graphics-hle-with-cxd4-fallback; selectable-lle",
        "gpu_boundary_profiling": "opt-in-begin-end-cpu-wall-time",
        "n64_rom_frontend_copy_released": True,
        "overlay_upload_checked": True, "overlay_pixel_check": "once-per-selection",
        "core_patches": {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in sorted((ROOT / "external/patches").glob("*.patch"))},
    }, indent=2) + "\n")
    shutil.copy2(dist / "build-info.json", dist / f"build-info-v{VERSION}.json")
    subprocess.run([sys.executable, str(ROOT / "scripts/prepare-update.py"), "--pkg", str(destination),
                    "--build-info", str(dist / "build-info.json"), "--channel", "experimental",
                    "--notes", "Color GB opcional, ajustes NES/SNES, menos cargas de textura, corrección de ruta del instalador y Acerca de con Ko-fi. Experimental: primera instalación manual; validación PS4 pendiente.",
                    "--output", str(dist / f"update-v{VERSION}-experimental.txt")], check=True)
    print(f"PKG validado: {destination} ({destination.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
