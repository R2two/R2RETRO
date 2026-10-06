# v0.4.0 — NES y SNES

R2N64 conserva su identidad y añade dos sistemas al frontend C++/SDL2. Usa
FCEUmm para NES y bsnes-mercury Performance para SNES, enlazados estáticamente
con símbolos separados. Revisiones, licencias, opciones y cambios locales en
[CONSOLE-CORES.md](CONSOLE-CORES.md). No se cambian las revisiones de los tres
núcleos anteriores ni la secuencia de arranque SDL/Piglet/GoldHEN.

## Juegos y biblioteca

En PS4 se crean `/data/R2N64/roms/nes/` y `/data/R2N64/roms/snes/`. En USB se
usan `R2N64/roms/nes/` y `R2N64/roms/snes/` dentro de las ubicaciones ya
configuradas. En desktop se respeta `R2N64_DATA` y las raíces configuradas.
Biblioteca: Triángulo vuelve a escanear; L1/R1 cambia entre los siete filtros
Todos/GB/GBC/GBA/N64/NES/SNES. X muestra detalles y otro X inicia el juego.

| Sistema | Archivos | Detección |
|---|---|---|
| NES | `.nes` | iNES / NES 2.0, trainer opcional, tamaño PRG/CHR acotado |
| SNES | `.sfc`, `.smc` | Cabeceras LoROM/HiROM/ExHiROM, copier de 512 bytes opcional |

La detección no certifica compatibilidad de un juego. No se admiten archivos
comprimidos, FDS, Super Game Boy, Sufami Turbo, BS-X ni MSU-1 en esta integración.
Los archivos especiales requeridos por ciertos cartuchos SNES no se incluyen;
una carga que los necesita y no dispone de ellos devuelve el diagnóstico del
núcleo. HLE está habilitado para los coprocesadores que lo soportan y SuperFX
permanece al 100 %, sin overclock. No se afirma compatibilidad de todos los
mappers o coprocesadores.

Los estados rápidos se deshabilitan en cartuchos que usan DSP1–4 en HLE,
porque esta revisión upstream omite su estado interno. El guardado normal de
cartucho sigue disponible. ST0010 usa sus hooks de reset/serialización; ST0011
no se ejecuta como ST0010 y requiere su ruta LLE con firmware externo.

## Controles y presentación

| Acción | NES | SNES | Teclado |
|---|---|---|---|
| Cruz | A | B | Z |
| Cuadrado | B | Y | X |
| Círculo | — | A | C |
| Triángulo | — | X | S |
| L1 / R1 | — | L / R | Q / W |
| OPTIONS | Start | Start | Enter |
| Panel táctil | Select | Select | Tab |
| L3 + R3 | Pausa | Pausa | Escape |
| Mantener R2 | Avance rápido | Avance rápido | Espacio |

Cruceta/flechas mueven al jugador. Se admite un mando; no se añade multijugador
ni vibración. Los controles N64 y GB/GBC/GBA se conservan.

NES/SNES usan **4:3 por defecto**, sin marco portátil. La pausa permite elegir
escala entera con píxeles cuadrados, suavizado o píxeles nítidos, estadísticas,
avance rápido 2x/4x/8x, cinco espacios de estado, reinicio y capturas PNG. Los
multiplicadores son objetivos: no prometen una velocidad concreta en consola.
El audio se silencia mientras se mantiene el avance y vuelve al soltar.

SRAM, RTC si el núcleo lo expone, estados, capturas y preferencias se separan
con los IDs `nes` / `snes`. Las opciones se guardan en
`configs/systems/{nes,snes}.json`. Una cabecera copier SNES se elimina antes
del hash y de la carga: versiones equivalentes `.sfc` y `.smc` comparten datos.
Se conservan ordinales de sistemas anteriores para no invalidar sus estados.

## Diagnósticos originales

`scripts/make_console_diagnostics.py` genera programas propios exclusivamente
en `build/`: un NES NROM con patrón de tiles y tono de pulso, y un SNES LoROM
con color de fondo controlado por entrada, un programa SPC700 propio y una onda
BRR propia. Ambos leen registros de mando y escriben firma, contador de arranque
y cuadros en SRAM. No se descargan ROMs de prueba ni se incluyen estos fixtures
en el PKG. El generador incluye una copia SNES con prefijo de 512 bytes.

Los tests de núcleos reales comprueban **4.844 cuadros** acumulados: firma de
CPU, vídeo no negro, audio sostenido con variación, cada botón y desconexión,
estados 0/4 con replay exacto de 60 cuadros de imagen/memoria, rechazo de
corrupción, SRAM tras reset/unload/reload y estado compartido `.sfc`/`.smc`.
Dos ejecuciones frescas comparan 120 observaciones de imagen/memoria con y sin
avance rápido y el PCM completo después de soltar: deben ser idénticos.

El smoke de la aplicación cubre ocho sesiones de 1.200 cuadros, ajustes por
defecto y persistidos, retorno del audio, ausencia de marcos portátiles y
capturas únicas idénticas a la composición del juego sin el menú de pausa.
Detector y scanner prueban cabeceras sintéticas, overflow, truncados, enlaces,
hash de contenido y normalización copier; también pasan ASan/UBSan.

La revisión de FCEUmm detectó que su lector automático de `system/nes.pal`
podía desbordar un buffer fijo. La copia aislada ahora acepta únicamente 192
o 1.536 bytes y exige una lectura completa. El resto de las paletas se ignora.
Los cambios de integración nunca modifican los submódulos upstream.

Un test adicional con metadatos sintéticos ejercita la recuperación tras una
carga que requiere firmware ausente, los hooks de ST0010 y el rechazo de
estados incompletos DSP1–4. No certifica ejecución de juegos que usen esos chips.

Reproducción en WSL:

```bash
bash scripts/build-console-cores.sh linux
cmake -S . -B build/desktop -DCMAKE_BUILD_TYPE=Debug
cmake --build build/desktop -j2
ctest --test-dir build/desktop --output-on-failure
bash scripts/build.sh ps4
```

## Alcance de la validación

Compilaciones desktop y PS4 completadas. El paquete
`dist/R2N64-v0.4.0-nes-snes.pkg` se extrajo y comparó con los archivos de
staging: **61.931.520 bytes**, SFO `00.40`, Title ID `RNTD00064`.
SHA-256: `5056973957762b10f09d85b0b88bf7cc548b204ead773bd7a7d617ddeda0ecd7`.
El hash de v0.3.3 sigue siendo
`6d33f9e2ffa6e7bdeb9b947c0166dc3b41a954af5ee19078a1b48d41b973a7e8`.

Los **33 grupos CTest** quedaron aprobados entre la ejecución completa y una
repetición dirigida. La primera ejecución tuvo 32 aprobados y un fallo de
lanzamiento de `handheld_app_preferences_speed` (`Text file busy`), porque
el enlace final desktop seguía escribiendo el ejecutable. Una vez terminado,
`ctest --rerun-failed` aprobó ese test y su fixture, sin cambios de código.
Evidencia local: `build/v040-ctest.log`, `build/v040-ctest-rerun.log`,
`build/v040-desktop-build.log`, `build/v040-ps4-build.log` y
`dist/build-info-v0.4.0.json`. Detector/scanner también se comprobaron con
ASan/UBSan; las pruebas de ROM comercial anteriores no se repitieron para esta
versión.

Las pruebas son locales Linux/SDL y usan audio dummy; no validan sonido audible,
rendimiento de PS4 ni juegos comerciales NES/SNES. Ninguna ROM comercial se
utilizó en esta integración. Los otros sistemas mantienen sus grupos de
regresión, incluidos entrada N64, CPU/RDP, audio, XMB, estados y preferencias.

La compilación y validación del PKG se registran separadamente de la ejecución
física. Es necesario contrastar el nuevo paquete en la PS4 original/Slim del
usuario para confirmar arranque, sonido, controles y velocidad real.
