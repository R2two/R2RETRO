#!/usr/bin/env python3
"""Prepare a bounded update manifest AFTER local PKG validation. No network/push.

Upload the PKG to its immutable release first; publish the channel pointer last.
The app trusts the HTTPS repository owner. SHA-256 detects corruption but is not
an independent publisher signature. No keys/tokens are embedded in the app.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

TITLE_ID = "RNTD00064"
CONTENT_ID = "IV0001-RNTD00064_00-R2N64APP00000001"


def prepare(pkg, info, channel, notes):
    version = info["version"]
    sfo = info["sfo"]
    if not re.fullmatch(r"(?:0|[1-9][0-9]{0,2})\.(?:0|[1-9][0-9]{0,2})\.(?:0|[1-9][0-9]{0,2})", version):
        raise ValueError("Invalid version")
    if not re.fullmatch(r"[A-Za-z0-9_.-]{1,156}\.pkg", pkg.name) or ".." in pkg.name:
        raise ValueError("Invalid PKG filename")
    if (info.get("package_validated") is not True or info.get("title_id") != TITLE_ID
            or sfo.get("TITLE_ID") != TITLE_ID or sfo.get("CONTENT_ID") != CONTENT_ID
            or not re.fullmatch(r"[0-9]{2}\.[0-9]{2}", sfo["APP_VER"])):
        raise ValueError("PKG validation/identity missing")
    size = pkg.stat().st_size
    if not 4096 <= size <= 512 * 1024 * 1024 or size != info["pkg_bytes"]:
        raise ValueError("PKG size mismatch")
    with pkg.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != info["sha256"]:
        raise ValueError("PKG differs from validated build")
    if channel not in ("stable", "experimental") or len(notes.encode("utf-8")) > 240 or any(ord(c) < 32 or ord(c) == 127 for c in notes):
        raise ValueError("Invalid channel/notes")
    fields = dict(version=version, sfo=sfo["APP_VER"], channel=channel,
                  url=f"https://github.com/R2two/R2RETRO/releases/download/v{version}/{pkg.name}",
                  sha256=digest, size=size, notes=notes, title_id=TITLE_ID, content_id=CONTENT_ID)
    return "R2RETRO-UPDATE-1\n" + "".join(f"{key}={value}\n" for key, value in fields.items())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pkg", required=True, type=Path)
    parser.add_argument("--build-info", required=True, type=Path)
    parser.add_argument("--channel", choices=("stable", "experimental"), default="experimental")
    parser.add_argument("--notes", required=True)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    data = prepare(args.pkg, json.loads(args.build_info.read_text()), args.channel, args.notes)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(data, encoding="utf-8", newline="\n")
    print(f"Prepared locally (not published): {args.output}")


if __name__ == "__main__":
    main()
