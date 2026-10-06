# Pruebas de imagen, paletas y estados de v0.3.1

Este documento registra las comprobaciones dirigidas del renderer y de los
núcleos portátiles. Amplía [HANDHELD-TESTS.md](HANDHELD-TESTS.md), que describe
los diagnósticos originales y la cobertura de CPU, audio, entrada y SRAM.
Los resultados siguientes corresponden a escritorio Linux; no confirman el
comportamiento de Piglet ni de una PS4 física.

## Tamaño y filtro de imagen

`Video::gameFrame` conserva sus argumentos anteriores y añade `linearFilter`
y `nativeAspect`, ambos `false` por defecto. Los portátiles activan
`nativeAspect`; N64 conserva la presentación 4:3 anterior, incluso cuando su
cuadro de origen está recortado a 320 × 224.

La salida lógica de 1920 × 1080 produce estos rectángulos centrados:

| Sistema | Modo | Imagen | Esquina superior izquierda |
|---|---|---|---|
| GB/GBC, 160 × 144 | Entero, 7× | 1120 × 1008 | 400, 36 |
| GB/GBC, 160 × 144 | Ajustar | 1200 × 1080 | 360, 0 |
| GBA, 240 × 160 | Entero, 6× | 1440 × 960 | 240, 60 |
| GBA, 240 × 160 | Ajustar | 1620 × 1080 | 150, 0 |
| N64, configuración anterior | 4:3 | 1440 × 1080 | 240, 0 |

Si un cuadro supera la salida disponible, el modo entero pasa a un ajuste
proporcional que cabe completo. La prueba usa una imagen de 2048 × 2048 y exige
un resultado de 1080 × 1080, sin recortes.

`tests/handheld_video_tests.cpp` usa el renderer SDL real y vuelve a leer sus
capturas PNG para comprobar cada píxel del rectángulo y de sus márgenes negros.
Una imagen de franjas alternas comprueba que Nearest conserva exclusivamente
los colores originales, que Bilineal produce colores interpolados y que volver
a Nearest recupera exactamente la imagen inicial. También se comprueba que un
hint global de SDL no cambia la selección explícita de Nearest.

Los wrappers del enlazador observan las llamadas reales a `SDL_CreateTexture`
y `SDL_SetTextureScaleMode`: cambiar escala o filtro no recrea la textura;
cuadros sucesivos sin cambios no vuelven a configurar el filtro. Cambiar las
dimensiones o liberar la textura sí crea una nueva e invalida la caché del
filtro. Los cuadros inválidos se rechazan antes de asignar una textura.

## Paletas de Game Boy y cinco espacios de estado

`tests/handheld_tests.cpp` ejecuta los programas originales con SameBoy y mGBA
a través de `Emulator`. Las comprobaciones adicionales cubren:

- Las cuatro paletas GB: Gris, Verde clásico, Oliva y Turquesa. Cada opción
  genera cuatro colores efectivos diferentes; cambiarla no reinicia el
  cartucho y volver a Gris recupera sus colores exactos.
- Selección de Turquesa antes de cargar la ROM. Una carga de estado y un
  reinicio posterior conservan la paleta vigente del usuario.
- Rechazo de índices de paleta inválidos y de la opción GB en GBC/GBA.
- Los cinco espacios de la API, `0..4`, en GB/GBC/GBA. Cada uno guarda un
  contador diferente del programa y recupera su propia SRAM al cargarse.
- Escrituras de los espacios adicionales sin alterar el archivo histórico
  `.slot0.state`; rechazo de espacios ausentes y del índice fuera de rango `5`.

Se mantienen las pruebas anteriores de reproducción de 17 cuadros con video y
progreso idénticos, estados corruptos o de otro cartucho/sistema, persistencia
de SRAM al cerrar y al reiniciar, y cambios entre GB, GBC, GBA y N64. Estas
pruebas no exigen audio PCM idéntico después de restaurar un estado.

## Ejecución y evidencia

Desde WSL, con las bibliotecas de los núcleos preparadas:

```sh
cmake -S . -B build/desktop
cmake --build build/desktop --target handheld_video_tests handheld_tests --parallel 4
ctest --test-dir build/desktop \
  -R '^handheld_(video_software|video_gles2|fixtures|core_sessions)$' \
  --output-on-failure
```

El 5 de octubre de 2026 aprobaron las cuatro comprobaciones dirigidas:

| Prueba | Resultado | Tiempo de ejecución del test |
|---|---|---:|
| `handheld_video_software` | PASS | 0,64 s |
| `handheld_video_gles2` | PASS | 0,92 s |
| `handheld_fixtures` | PASS | 0,04 s |
| `handheld_core_sessions` | PASS, incluidas paletas y espacios | 4,26 s |

El registro `build/v031-handheld-render-check.log` corresponde a una selección
inicial más amplia (`-R handheld`): también incluyó
`handheld_settings_persistence`, cuyo ejecutable no se había compilado con los
dos targets anteriores, por lo que quedó **Not Run**. No debe presentarse ese
registro como una suite completa aprobada. La suite general se registra por
separado. Los tiempos de la tabla no son medidas de rendimiento de juegos.

Las capturas están en
`build/desktop/previews/handheld-video-software/` y
`build/desktop/previews/handheld-video-gles2/`. Se inspeccionaron visualmente
`gb-fit.png` del renderer software y `gba-integer.png` y `bilinear.png` de
GLES2: el centrado, los márgenes y la interpolación coinciden con las
comprobaciones de píxeles. Las imágenes son patrones sintéticos de prueba.

Además, `src/video.cpp` compiló individualmente para OpenOrbis, con objeto en
`build/handheld-video-v031.ps4.o`. Esto verifica la compilación de ese archivo,
sin sustituir el enlace completo, la comprobación del PKG o una prueba física.

## Suite completa, preferencias y avance rápido

La ejecución final `bash scripts/build.sh test` aprobó **28/28 pruebas** en
48,90 segundos. Registro: `build/v031-tests.log`. Incluye la prueba de
persistencia que no se había compilado en la selección inicial descrita
arriba, además de la regresión N64 y la validación del árbol GP4.

`tests/handheld_settings_tests.cpp` verifica separación GB/GBC/GBA, valores
predeterminados, escritura atómica, fallo de escritura sin perder el archivo
anterior y rechazo de archivos inválidos o rutas mediante enlaces. No se
crean archivos al consultar preferencias ausentes. También pasó ASan/UBSan
y el enlace aislado de esta función con OpenOrbis.

`tests/handheld_app_smoke.py` ejecuta el frontend completo con SDL/GLES2:
cuatro combinaciones de GB/GBC/GBA y velocidades 2x/4x/8x, cada una durante
dos sesiones de 1.200 cuadros. Comprueba preferencias cargadas al reabrir,
entrada y salida del avance rápido, cola de audio vacía durante el avance,
reanudación posterior de PCM y captura del menú de pausa. Evidencia final:
`build/desktop/handheld-app-tests/sessions-z_n74__v/`.

El avance rápido ejecuta todos los pasos del núcleo y limita cada lote a
un presupuesto de 8 ms antes de volver a atender el frontend. La presentación
usa el último cuadro del lote; el audio del núcleo se sigue generando, pero
se descarta durante el avance. Los multiplicadores son objetivos, no una
promesa de rendimiento. La medición cuenta los pasos realmente completados.

## Pokémon Red aportado por el usuario

Se leyó la ROM desde Downloads, sin copiarla al repositorio ni al paquete.
SHA-256: `5ca7ba01642a3b27b0cc0b5349b52792795b62d3ed977e98a09390659af96b7b`.
La prueba Release completó dos sesiones de 6.000 cuadros, con carga de estado
y replay de 60 cuadros aprobados. Con la paleta Gris predeterminada, ambas
sesiones conservan los hashes de v0.3.0:

| Resultado | Hash |
|---|---|
| Secuencia de video | `c917c6a42cf8cb94` |
| PCM | `eac83c2346310960` |

Evidencia:
`build/v031-pokemon-tests/20261005T153731Z-266/matrix.json` y
`1-handheld/run-356-K3ueaE/report.json` dentro de ese directorio.

Además, el frontend SDL/GLES2 completó dos sesiones de 1.200 cuadros con
paleta Verde clásico, escalado entero, filtro nearest, espacio 5 y avance
8x. Se comprobaron 240 pasos acelerados por sesión, ambas transiciones de
velocidad, reanudación de audio y reapertura de SRAM. Captura inspeccionada:
`build/v031-pokemon-preview.png`; log en
`build/v031-pokemon-app-data/logs/r2n64.log`. La imagen está centrada y las
opciones del menú son legibles. Estas pruebas usan audio SDL dummy: verifican
PCM y colas, no sonido audible. Tampoco equivalen a una partida completa.

## Paquete y comprobación en consola pendiente

El enlace PS4 y el empaquetado completos finalizaron correctamente con la
toolchain existente. Se validaron firmas, hashes, SFO y el contenido extraído,
incluidos ejecutable, imágenes y avisos de licencia.

- Archivo: `dist/R2N64-v0.3.1-handheld-upgrades.pkg`.
- Tamaño: 49.217.536 bytes; SFO `00.31`; Title ID `RNTD00064`.
- SHA-256: `df669d30c24b100362cf8e3364f002dd1cdd81d2fa1d837279050451148bfb52`.
- Registro: `build/v031-ps4-build.log`; metadatos: `dist/build-info-v0.3.1.json`.
- El PKG anterior v0.3.0, fondo original, logo e icono conservan sus hashes.

El usuario indicó que todavía no ha probado v0.3.0. **v0.3.1 no se ha ejecutado
en una PS4 física**; ninguna cifra de estas pruebas representa su velocidad.

Para comprobar las mejoras en consola: abrir un juego portátil, pulsar
**L3+R3** y recorrer las opciones con arriba/abajo; X cambia la selección.
Comprobar guardar/cargar en dos espacios distintos, cambiar imagen y paleta
GB, cerrar y reabrir para verificar preferencias. Mantener **R2** durante
la partida y soltar para comprobar avance y retorno del audio. OPTIONS sigue
siendo Start; el panel táctil sigue siendo Select. Cerrar desde el menú para
persistir los guardados nativos.
