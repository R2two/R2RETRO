#!/usr/bin/env python3
"""Preserve complete third-party notices for the pinned NES/SNES archives."""
from pathlib import Path
import re
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
destination = Path(sys.argv[1])
destination.mkdir(parents=True, exist_ok=True)
entries = (
    ('fceumm', 'libretro/libretro-fceumm', 'src/drivers/libretro/libretro-common'),
    ('bsnes-mercury', 'libretro/bsnes-mercury', 'libco'),
)
for name, repository, folder in entries:
    upstream = root / 'external' / name
    revision = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    for path in sorted((upstream / folder).rglob('*')):
        if not path.is_file() or path.suffix.lower() not in {'.h', '.c', '.cpp', '.s'}:
            continue
        source = path.read_text(encoding='utf-8-sig')
        position, comments = 0, []
        while match := re.match(r'\s*(/\*.*?\*/|(?://[^\n]*(?:\n|$))+)', source[position:], re.DOTALL):
            comments.append(match.group(1))
            position += match.end()
        notice = '\n\n'.join(comments)
        if not re.search(r'copyright|licen[cs]e|public domain|permission is hereby', notice, re.IGNORECASE):
            continue
        relative = path.relative_to(upstream)
        target = destination / 'embedded-notices' / name / (relative.as_posix() + '.txt')
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(f'Source: https://github.com/{repository}/blob/{revision}/{relative.as_posix()}\n\n{notice}\n')
# These LGPL notices occur below include directives rather than at file start.
for name, relative in (('fceumm', 'src/ntsc/nes_ntsc.c'), ('bsnes-mercury', 'sfc/alt/dsp/SPC_DSP.cpp')):
    source = (root / 'external' / name / relative).read_text()
    notice = next(m.group(0) for m in re.finditer(r'/\*.*?\*/', source, re.DOTALL)
                  if 'Copyright (C)' in m.group(0) and 'Lesser' in m.group(0))
    (destination / (name + '-Shay-Green-LGPL-NOTICE.txt')).write_text(f'Source: {name}/{relative}\n\n{notice}\n')
shutil.copyfile(root / 'external/mgba/src/third-party/blip_buf/license.txt', destination / 'LGPL-2.1.txt')
