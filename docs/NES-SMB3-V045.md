# NES / Super Mario Bros. 3: investigación v0.4.5

El usuario reportó pantalla negra en PS4 con Super Mario Bros. 3 mientras la
pausa indicaba aproximadamente 100 % y 60,1 cuadros/s. Esa cadencia indica que
el bucle sigue ejecutándose; por sí sola no demuestra que el núcleo produzca
una imagen correcta o que el renderer la presente.

La pantalla negra **no se ha reproducido en Linux**. Tampoco se ha comprobado
la corrección en una PS4 física. La revisión del núcleo y sus opciones no se
actualizaron para esta investigación.

## Copia local y pruebas del núcleo

Se leyó directamente, sin copiarla al repositorio ni al PKG:

`C:/Users/R2A/Downloads/Super Mario Bros. 3 (USA) (Rev 1).nes`

- Tamaño: 393.232 bytes; cabecera iNES, mapper 4 / MMC3, 256 KiB de PRG y
  128 KiB de CHR, sin trainer.
- SHA-256 antes/después:
  `4377a7f5e6eb50bdd2ac6f249bf1a7085500aca8eb41f38545c3a2731c51a579`.
- La región/revisión y los bytes de la copia del USB en PS4 no se han
  comparado con esta copia local.
- Datos, estados y capturas de prueba quedan aislados en
  `build/nes-black-audit/`; no se distribuyen.

| Secuencia local | Resultado por dos sesiones de 6.000 cuadros |
|---|---|
| Entrada neutral | Intro y título visibles; PCM cero durante estas sesiones; no negro |
| Traza genérica anterior, Start en cuadro 600 | Título visible; PCM cero; no negro |
| Start en 1.800–1.811 y 2.000–2.011; A intermitente desde 2.200; derecha en 2.800–3.999 | Mapa del Mundo 1 visible, audio activo y dos replays de estado de 60 cuadros aprobados |

La secuencia tardía produjo 6.000 cuadros no negros por sesión. Sus hashes de
secuencia de imagen coincidieron (`2ac84cc6dc0b7959`), al igual que el replay
(`94dbb3230b1687fc`). Se midieron 6.699.478 y 6.734.206 muestras PCM no cero,
respectivamente. El PCM **no coincidió exactamente entre sesiones**; estas
pruebas no establecen determinismo de audio. Tampoco validan una partida
completa, todos los niveles, ni guardados de progreso de este juego.

La exigencia `--require-av` falló en las dos trazas anteriores sólo por el PCM
cero. La traza con Start tardío sí pasó. No se modificó la emulación para
obtener audio: cambió únicamente la entrada del banco de pruebas.

Evidencia:

- `build/nes-black-audit/baseline/20261005T220023Z-301/matrix.json`
- `build/nes-black-audit/scripted/20261005T220136Z-281/matrix.json`
- `build/nes-black-audit/late-start/20261005T220449Z-487/matrix.json`
- `build/nes-black-audit/smb3_probe.cpp`: copia local del banco con la traza
  tardía explícita; el `input_trace` genérico del informe no describe esa
  modificación, por lo que debe consultarse esta secuencia y ese fuente.

## Diagnóstico MMC3 original y regresión

`scripts/make_console_diagnostics.py` añade `diagnostic-mmc3.nes`, escrito
desde cero, sin datos ni código de juegos comerciales. El NROM y los dos
formatos de diagnóstico SNES anteriores conservan exactamente sus bytes.

El nuevo cartucho tiene 256 KiB PRG/128 KiB CHR. Ejecuta su programa desde el
banco fijo final y guarda las lecturas reales en SRAM: bancos PRG altos y modo
invertido, banco fijo, bancos CHR y su inversión, mirroring vertical. Además,
arma y reconoce interrupciones de scanline, mantiene un contador de IRQ,
dibuja cuatro colores y produce el tono original del diagnóstico.

`tests/console_tests.cpp` incluye este cartucho en las pruebas existentes de
arranque, entradas y desconexión, slots 0/4, repetición de imagen/memoria,
rechazo de estados corruptos, reset, persistencia SRAM y avance rápido.
Las observaciones de estado abarcan ahora 16 bytes para comprobar también
las lecturas de bancos y el contador de IRQ. La prueba del patrón de cuatro
colores impide aceptar una imagen uniforme como resultado correcto.

La validación aislada final pasó los tres cartuchos; resultado conservado en
`build/nes-black-audit/mmc3-fixed.log`, con datos bajo
`build/nes-black-audit/mmc3-fixed/data-KAqUys`. El SHA-256 del diagnóstico
MMC3 final es
`fee4a7e7bc7d3e5c154cba07d4386babad52d8f10e390b4ba721f0b14bc57931`.
La primera versión del programa sintético no restauraba los bits de
nametable después de escribir la paleta. La comprobación de cuatro colores
detectó ese error; se corrigió en el generador mediante PPUCTRL, sin cambiar
la implementación del núcleo.

Esta cobertura comprueba el comportamiento del núcleo con un programa
controlado; no constituye una prueba exhaustiva de las revisiones MMC3 o de
temporización de todos sus casos límite.

## Auditoría de compilación y carga

`build/nes-black-audit/compiler-audit.txt` conserva los comandos efectivos y
macros preprocesadas de los perfiles actuales Linux y PS4. Ambos usan
XRGB8888 (`platform=unix` selecciona `WANT_32BPP=1`), little endian, `char` con
signo, punteros/`long` de 64 bits y SSE2. No hay una rama de implementación
específica PS4 dentro de la CPU, PPU o MMC3 de esta revisión. Esto no equivale
a ejecutar el binario PacBrew en Linux.

La integración conserva `info->data/size` y pasa ese buffer a FCEUmm.
`FCEU_fopen` toma `MakeMemWrapBuffer`; `FCEU_fread` copia PRG/CHR mediante
`memcpy`. La inspección del ELF PS4 confirma esa ruta: no depende de
`fopen/stat/openat` para leer los bloques del cartucho dentro del núcleo.
La paleta opcional `system/nes.pal` es una lectura separada y su ausencia
mantiene la paleta interna. No se encontró un stub de archivos que explique
este síntoma después de una carga aceptada.

Como contraste del compilador, se construyó otra copia aislada de FCEUmm con
el mismo Clang 12.0.1 de PacBrew, pero usando explícitamente
`--target=x86_64-pc-linux-gnu --gcc-toolchain=/usr`. La biblioteca usa ABI y
libc Linux; **no se intentó ejecutar objetos de PS4 en Linux**. No reemplaza
los archivos de núcleos del build normal.

La copia Clang pasó NROM/MMC3/SNES y otras dos sesiones SMB3 de 6.000 cuadros
con la traza tardía. Todos los cuadros fueron no negros, hubo PCM activo y
los replays de estado pasaron. El hash de secuencia de imagen fue el mismo
que con GCC (`2ac84cc6dc0b7959`). Esto no aporta indicios de un fallo MMC3
dependiente de GCC/Clang en esas trazas, pero no verifica el ABI ni la
ejecución del binario PS4.

Evidencia: `build/nes-clang-linux/summary.json`, `console-tests.log`,
`build.log` y `command.txt`. SHA-256 de la biblioteca FCEUmm Clang aislada:
`93362db1d2533f5f438aa4713f447b94a5d3d031da6fd72fc6ca3f487641b07d`.

## Contrato de presentación

`CoreFrame` contiene RGB sin canal alfa: el byte alto de XRGB8888 no tiene
significado de transparencia y las conversiones de 16 bits pueden dejarlo
en cero. La nueva textura `SDL_PIXELFORMAT_RGB888` declara ese contrato
explícitamente y conserva `SDL_BLENDMODE_NONE`; no modifica los píxeles ni
los hashes del puente.

La textura anterior ya usaba `SDL_BLENDMODE_NONE`. Por eso el byte alto no
puede presentarse como causa confirmada de la pantalla negra. Las pruebas de
presentación y la comprobación física deben distinguirse de las pruebas del
núcleo descritas arriba.

Con el binario desktop v0.4.5 se ejecutó `--rom-smoke` contra la misma ROM
local, tanto con renderer software como con GLES2/Mesa. Cada caso completó
dos sesiones de 1.200 cuadros, retorno a la interfaz, avance rápido/audio y
dos capturas únicas: 4.800 cuadros en total. Se inspeccionaron las capturas
de juego de 1920×1080; ambos renderers muestran la introducción correctamente.
La prueba smoke verifica transferencia/cola de audio, no que la música sea
audible en esta fase; la comprobación PCM activa corresponde a la traza
tardía anterior.

Evidencia del binario y ROM intactos, logs y capturas:
`build/nes-black-audit/frontend-v045/sessions-dmrbhxb2/summary.json`.
Los PNG de software y GLES2 no coinciden byte a byte; presentan diferencias
menores de muestreo. GLES2/Mesa en Linux no valida Piglet en PS4.
