#!/usr/bin/env python3
"""Run an EXISTING Linux probe on local NES/SNES ROMs. Never builds or copies ROMs."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--nes', required=True, type=Path)
    parser.add_argument('--snes', required=True, type=Path)
    parser.add_argument('--probe', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--frames', type=int, default=3600)
    parser.add_argument('--scenario', choices=('intro','scripted','console'), default='intro',
                        help='console requires the new probe after an authorized build')
    args = parser.parse_args()
    if not 600 <= args.frames <= 36000:
        parser.error('frames must be 600..36000')
    probe = args.probe.resolve(strict=True)
    if not probe.is_file() or not os.access(probe, os.X_OK):
        parser.error('An existing executable is required; this script does not build it')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    result = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'probe_sha256': digest(probe), 'compiled_this_run': False,
              'ps4_tested': False, 'original_hardware_tested': False,
              'scope': 'Existing binary, two fresh sessions per ROM; not full gameplay or pending source validation',
              'scenario': args.scenario,
              'cases': []}
    try:
        for system, supplied in [('nes', args.nes), ('snes', args.snes)]:
            rom = supplied.resolve(strict=True)
            before = digest(rom)
            case_dir = output / system
            case_dir.mkdir()
            command = [str(probe), str(rom), str(case_dir), 'auto', 'hle', '4',
                       str(args.frames), args.scenario, '2', 'fresh', 'off']
            # auto/hle/4 are unused N64 arguments in this older shared probe.
            with (case_dir/'probe.log').open('w') as log:
                proc = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=600)
            reports = list(case_dir.glob('run-*/report.json'))
            case = {'system': system, 'rom': str(rom), 'bytes': rom.stat().st_size,
                    'rom_sha256': before, 'rom_unchanged': digest(rom) == before,
                    'exit_code': proc.returncode, 'command': command}
            result['cases'].append(case)
            if proc.returncode or len(reports) != 1:
                raise RuntimeError(f'{system}: probe failed; inspect {case_dir}')
            report = json.loads(reports[0].read_text())
            sessions = report['sessions']
            if len(sessions) != 2 or any(s['system'] != system for s in sessions):
                raise RuntimeError(f'{system}: missing sessions or wrong core')
            keys = ['video_sequence_fnv64', 'last_frame_fnv64', 'audio_fnv64', 'audio_samples']
            case.update(report=str(reports[0]), sessions=sessions,
                        repeatable={k: sessions[0][k] == sessions[1][k] for k in keys},
                        audio_observed=[s['nonzero_audio_observed'] for s in sessions],
                        state_replays=report['portable_state_checks'])
            if (not case['rom_unchanged'] or
                    len(case['state_replays']) != 2 or
                    not all(c['passed'] for c in case['state_replays']) or
                    not all(s['nonblack_video_observed'] for s in sessions)):
                raise RuntimeError(f'{system}: integrity, video or state check failed')
            # Silence can be legitimate in attract mode (SMB3). Never report it
            # as audible output, or hide unequal fresh-session hashes as a pass.
            print(f'{system}: 2 x {args.frames}; repeatability={case["repeatable"]}; audio={case["audio_observed"]}', flush=True)
    finally:
        result['probe_unchanged'] = digest(probe) == result['probe_sha256']
        (output/'results.json').write_text(json.dumps(result, indent=2)+'\n')


if __name__ == '__main__':
    main()
