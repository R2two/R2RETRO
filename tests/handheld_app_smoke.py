"""Full SDL app smoke using only the generated original portable cartridges."""
import json
import os
from pathlib import Path
import subprocess
import struct
import sys
import tempfile

app, fixtures, output = map(lambda arg: Path(arg).resolve(), sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
run = Path(tempfile.mkdtemp(prefix="sessions-", dir=output))
for system, factor, slot, integer, linear, palette, overlay, stats in (
    ("gb", 2, 0, True, False, 0, None, None),  # Legacy v1 defaults frame and HUD on.
    ("gbc", 4, 2, False, True, 0, False, False),
    ("gba", 8, 4, False, False, 0, True, False),
    ("gb", 8, 3, False, True, 1, False, False),
    ("gbc", 2, 1, True, False, 0, True, True),
    ("gba", 2, 0, True, False, 0, False, None),
):
    data = run / f"{system}-{factor}"
    settings = data / "configs/systems" / f"{system}.json"
    settings.parent.mkdir(parents=True)
    values = dict(version=1, fastForward=factor, stateSlot=slot,
        integerScaling=integer, linearFilter=linear, gbPalette=palette)
    if overlay is not None:
        values["overlay"] = overlay
    if stats is not None:
        values["showStats"] = stats
    enabled = True if overlay is None else overlay
    stats_enabled = True if stats is None else stats
    settings.write_text(json.dumps(values), encoding="utf-8")
    before = settings.read_bytes()
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_RENDER_DRIVER="opengles2",
        LIBGL_ALWAYS_SOFTWARE="1", SDL_AUDIODRIVER="dummy", R2N64_DATA=str(data))
    result = subprocess.run([str(app), "--rom-smoke", str(fixtures / f"diagnostic.{system}"),
        "--accelerated", "--screenshot", str(data / "pause.png")], env=env,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=45)
    (data / "console.log").write_bytes(result.stdout)
    assert result.returncode == 0, f"{system}/{factor} failed: {result.stdout.decode(errors='replace')}"
    log = (data / "logs/r2n64.log").read_text(encoding="utf-8")
    expected = f"Preferencias {system}: avance={factor}x; espacio={slot+1}; entero={int(integer)}; suavizado={int(linear)}; paleta={palette}; marco={int(enabled)}; estadísticas={int(stats_enabled)}"
    assert log.count(expected) == 2, "Settings did not survive two complete core sessions"
    assert log.count(f"Marco {system}: preferencia={int(enabled)}; activo={int(enabled)}") == 2, "Frame preference was not applied in both core sessions"
    assert log.count(f"Velocidad solicitada: {factor}x;") == 2, "Hold did not activate chosen speed"
    assert log.count("Velocidad solicitada: 1x; audio normal") == 2, "Release did not restore normal mode"
    assert log.count("transiciones 2; audio reanudado=1; audio limpio al cerrar") == 2, "Audio or transitions failed"
    assert "Emulation smoke passed: two sessions, video and audio" in log
    assert settings.read_bytes() == before, "Merely playing rewrote preferences"
    assert (data / "pause.png").stat().st_size > 1000
    assert (data / "pause.png.game.png").stat().st_size > 1000
    captures = list((data / "screenshots" / system).glob("*.png"))
    assert len(captures) == 2, "The two paused captures must have unique filenames"
    capture_lines = [line.split("Captura guardada: ", 1)[1].strip()
                     for line in log.splitlines() if "Captura guardada: " in line]
    assert len(capture_lines) == 2 and capture_lines[0] != capture_lines[1]
    saved = Path(capture_lines[-1])
    assert saved.parent.resolve() == (data / "screenshots" / system).resolve(), "Capture ignored the configured data path"
    png = saved.read_bytes()
    assert png[:8] == b"\x89PNG\r\n\x1a\n" and struct.unpack(">II", png[16:24]) == (1920, 1080)
    assert png == (data / "pause.png.game.png").read_bytes(), "Capture includes pause UI or changed the composed frame"
    print(f"PASS {system} {factor}x overlay={enabled} HUD={stats_enabled}: preferences, hold/release, audio clear/resume, clean capture, two sessions")
print(f"Evidence: {run}")
