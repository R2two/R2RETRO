"""SDL NES/SNES app checks using only generated original diagnostic cartridges."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

app, fixtures, output = (Path(arg).resolve() for arg in sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
run = Path(tempfile.mkdtemp(prefix="sessions-", dir=output))
for system, extension, factor, slot, integer, linear, stats, overlay, write_config in (
    ("nes", "nes", 2, 0, False, False, True, False, False),
    ("snes", "sfc", 2, 0, False, False, True, True, False),
    ("nes", "nes", 4, 3, True, True, False, False, True),
    ("snes", "sfc", 8, 4, True, False, False, False, True),
    ("snes", "sfc", 4, 2, True, True, True, True, True),
):
    data = run / f"{system}-{factor}"
    settings = data / "configs/systems" / f"{system}.json"
    data.mkdir()
    before = None
    if write_config:
        settings.parent.mkdir(parents=True)
        settings.write_text(json.dumps(dict(version=1, fastForward=factor,
            stateSlot=slot, integerScaling=integer, linearFilter=linear,
            gbPalette=0, overlay=overlay, showStats=stats)), encoding="utf-8")
        before = settings.read_bytes()
    env = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_RENDER_DRIVER="opengles2",
        LIBGL_ALWAYS_SOFTWARE="1", SDL_AUDIODRIVER="dummy", R2N64_DATA=str(data))
    result = subprocess.run([str(app), "--rom-smoke", str(fixtures / f"diagnostic.{extension}"),
        "--accelerated", "--screenshot", str(data / "pause.png")], env=env,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
    (data / "console.log").write_bytes(result.stdout)
    assert result.returncode == 0, f"{system}/{factor}: {result.stdout.decode(errors='replace')}"
    log = (data / "logs/r2n64.log").read_text(encoding="utf-8")
    expected = (f"Preferencias {system}: avance={factor}x; espacio={slot+1}; "
        f"entero={int(integer)}; suavizado={int(linear)}; paleta=0; marco={int(overlay)}; estadísticas={int(stats)}")
    assert log.count(expected) == 2, "Console preferences/defaults did not survive both sessions"
    assert "Marco no disponible" not in log, "Console artwork could not load"
    if system == "snes":
        assert log.count(f"Marco snes: preferencia={int(overlay)}; activo={int(overlay)}") == 2, \
            "SNES default/saved artwork preference was not applied in both sessions"
    else:
        assert f"Marco {system}:" not in log, "NES must not enable unsupported artwork"
    assert log.count(f"Velocidad solicitada: {factor}x;") == 2
    assert log.count("Velocidad solicitada: 1x; audio normal") == 2
    assert log.count("transiciones 2; audio reanudado=1; audio limpio al cerrar") == 2
    assert "Emulation smoke passed: two sessions, video and audio" in log
    if before is None:
        assert not settings.exists(), "Playing created unsolicited console preferences"
    else:
        assert settings.read_bytes() == before, "Playing rewrote console preferences"
    saved_lines = [line.split("Captura guardada: ", 1)[1].strip()
                   for line in log.splitlines() if "Captura guardada: " in line]
    assert len(saved_lines) == 2 and saved_lines[0] != saved_lines[1]
    captures = list((data / "screenshots" / system).glob("*.png"))
    assert len(captures) == 2, "Captures must remain distinct and separated by system"
    saved = Path(saved_lines[-1])
    assert saved.parent.resolve() == (data / "screenshots" / system).resolve()
    png = saved.read_bytes()
    assert len(png) > 1000 and png[:8] == b"\x89PNG\r\n\x1a\n"
    assert struct.unpack(">II", png[16:24]) == (1920, 1080)
    assert png == (data / "pause.png.game.png").read_bytes(), "Capture contains pause UI or advances gameplay"
    print(f"PASS {system} {factor}x integer={integer} overlay={overlay}: defaults/settings, audio resume, clean capture, two sessions")
print(f"Evidence: {run}")
