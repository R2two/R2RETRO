# NES/SNES: fidelidad y referencia local — 7 de octubre de 2026

> Actualización v0.5.6: cambios compilados y pruebas completadas con autorización.
> [Resultados, paquete y límites](releases/v0.5.6.md). Las menciones a fuentes
> sin compilar que siguen registran el estado anterior a esta autorización.

## Alcance

El usuario autoriza pruebas locales con SMB3 USA Rev 1 y Super Mario World U.
Se leyeron directamente en Downloads, sin copiar, parchear ni distribuir ROMs.
No se compiló código C/C++, no se generaron nuevas ROMs diagnósticas, no se
empaquetó ni publicó otra versión. Las ejecuciones usan binarios y fixtures
existentes; **no validan los cambios fuente de este documento**.

Fidelidad NES/SNES significa conservar relojes, reglas de PPU/APU, DMA, IRQ y
entrada del hardware emulado. Rendimiento en PS4 significa cumplir su presupuesto
de tiempo real en esa máquina. Una prueba Linux no certifica ninguna consola
física, ni igualar frecuencia de CPU o limitar hilos convierte el PC en PS4.
bsnes-mercury Performance sigue siendo el perfil empleado; no se presenta como
Accuracy, ni el HLE de chips especiales como equivalencia ciclo a ciclo.

## Mejoras fuente preparadas

- FCEUmm fija explícitamente región Auto, límite normal de sprites y overclock
  desactivado. Se conserva el comportamiento predeterminado de la revisión
  actual, evitando depender de futuros cambios de defaults. SNES explicita
  región auto y conserva SuperFX 100%; no cambia HLE/DSP ni exige firmware nuevo.
- Corrección de región NES: el cargador en memoria usa un nombre interno fijo,
  mientras FCEUmm deduce PAL por nombre en iNES1. Ahora una cabecera iNES1 con
  byte9=1 y padding10–15 limpio selecciona PAL explícitamente. NES2, cabeceras
  ambiguas y otros juegos conservan detección del core. No se adivina por el
  nombre comercial ni se considera este indicador suficiente para todos los
  dumps antiguos. Las nuevas fixtures PAL ejercitan precisamente esa ruta.
- NES NROM/MMC3 añaden lecturas reales de los tres espejos de RAM de CPU; sus
  resultados se comparan con el valor escrito. MMC3 conserva bancos PRG/CHR,
  inversión, mirroring e IRQ ya existentes.
- SNES añade DMA ROM→WRAM con readback, alias de WRAM en ambos sentidos y DMA
  ROM→VRAM de una baldosa 4bpp original. BG1 modo 1 y CGRAM producen dos colores
  comprobables, reemplazando la prueba visual que sólo tenía un fondo plano.
  Se inicializa el tilemap completo en forced blank, sin depender de VRAM cero.
- Se corrige el programa diagnóstico SNES para esperar **inicio y fin** de
  autolectura del mando: el inicio de VBlank no implica que el bit busy ya sea
  uno. Esto es una corrección del diagnóstico, no del núcleo del juego comercial.
- Variantes NTSC/PAL de NROM, MMC3 y LoROM. El banco comprueba las frecuencias
  declaradas por las revisiones fijadas, 120 pasos contra el contador VBlank del
  programa y muestras PCM según sampleRate/fps. Se permite ±1 tick por frontera
  de callback y dos cuadros de buffering de audio. No son pruebas de cada ciclo.
- El banco ROM añade una traza `console` que separa NES/SNES, usa cruceta digital
  y registra la entrada exacta en el informe. No aplica movimiento analógico
  de N64 a estas consolas. Llegar a una escena debe confirmarse en capturas.

Las fuentes de reglas son [mirroring NES](https://www.nesdev.org/wiki/Mirroring),
[temporización PPU](https://www.nesdev.org/wiki/PPU_frame_timing),
[lectura de mandos SNES](https://snes.nesdev.org/wiki/Controller_reading) y
[registros PPU SNES](https://snes.nesdev.org/wiki/PPU_registers), además de los
cores fijados en `external/`. El generador y sus nuevas expectativas todavía
deben ejecutarse juntos tras autorizar compilación; no son un oráculo validado
en cartucho/flashcart físico.

## Ejecuciones efectivamente realizadas

| Prueba con binario existente | Resultado | Límite |
|---|---|---|
| NROM/MMC3/LoROM originales anteriores | Tres diagnósticos aprobados: mandos, desconexión, estados 0/4, corrupción, SRAM, reinicio, audio y avance rápido | No incluye aún las nuevas variantes PAL, DMA/BG1 y cadencia |
| SMB3 neutral, 2×3.600 cuadros | Imagen y replays correctos; PCM cero | El requisito inicial de audio falló; no se oculta ni se cambia el core para forzar sonido |
| SMB3 traza genérica, 2×3.600 | Imagen repetible, PCM cero, dos replays correctos | La pulsación temprana de Start no alcanza la secuencia con audio |
| SMB3 traza tardía, 2×6.000 | Mapa con imagen y audio; secuencia visual idéntica; dos replays de 60 cuadros correctos | PCM distinto entre las dos sesiones, igual que la investigación v0.4.5 |
| Super Mario World, 2×3.600 | Imagen/audio, escena jugable, dos replays de 60 cuadros correctos | Secuencia visual difiere sólo en ventana 361–480; PCM difiere en 6/30 ventanas; captura final idéntica |

SMW produjo 3.235 cuadros no negros por sesión; los intervalos negros no se
consideran automáticamente fallo porque existen transiciones. Su causa exacta
de variación entre sesiones sigue abierta. No se certifica determinismo PCM,
todos los niveles ni guardado/Continue desde los menús de los juegos.

Se inspeccionaron capturas: SMB3 en la introducción y el mapa; SMW con Mario en un nivel.
La secuencia tardía SMB3 usa el banco anterior conservado, cuya fuente describe
Start 1800–1811/2000–2011, A periódico desde 2200 y derecha 2800–3999. Su informe
antiguo tiene un `input_trace` genérico incorrecto; el `evidence.json` nuevo lo
explicita. Este defecto motivó la traza `console` en el banco fuente actual.

Identidades de los archivos intactos:

- SMB3: 393.232 bytes, mapper4/MMC3, SHA256
  `4377a7f5e6eb50bdd2ac6f249bf1a7085500aca8eb41f38545c3a2731c51a579`.
- SMW: 524.800 bytes, prefijo copier de 512 bytes, LoROM; SHA256 del archivo
  `d70c9c7716ad12c674fc7dd744736aa48d4d7b4237f58066be620fda26024872`.

Evidencia local excluida del PKG:

- `build/console-existing-diagnostics-20261007/{evidence.json,console.log}`.
- `build/nes-snes-baseline-20261007/results.json`: conserva el fallo inicial
  de la expectativa de audio para SMB3 neutral; no llegó a ejecutar SMW.
- `build/nes-snes-scripted-20261007/results.json`: ambos juegos con el probe
  existente SHA256 `6d84b6205b24fd782baa33dc32ddab4b305c1b94096322dca1accdae0e6a50c6`.
- `build/nes-smb3-late-input-20261007/evidence.json` y `run-*/report.json`:
  probe previo SHA256 `d75c98d60856c2683a496a8fcc609577954bb778b0def1501c51a940cbdeb3e4`.

`scripts/run-console-baseline.py` no compila ni copia ROMs. Exige un ejecutable
existente y una carpeta nueva, registra sus hashes, sesiones, audio observable y
diferencias; no convierte hashes distintos o silencio en una equivalencia AV.
La opción `console` necesita el nuevo ejecutable una vez autorizado.

## Validación pendiente

Sintaxis Python y `git diff --check` comprobados. Fuentes C++, ensamblado de los
diagnósticos nuevos, enlace y regresiones nuevas **sin ejecutar**. No se usó el
CTest que regeneraría fixtures ni se tocó el PKG publicado.

Tras autorizar: compilar diagnósticos/frontend, contrastar PAL/NTSC y DMA en
ambos cores, repetir ROMs con la traza documentada y capturas en GLES2. Para
fidelidad física hace falta además ejecutar diagnósticos en NES/SNES o comparar
con resultados de suites previamente validadas en hardware; para PS4 hace falta
repetir las mismas escenas con marco/shader off/on y registrar velocidad, audio,
controles y guardados en original/Slim. Interlace/hires, sprite overflow/zero hit,
DMC DMA, revisiones MMC3 y chips SNES siguen siendo cobertura pendiente.
