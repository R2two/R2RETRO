# R2RETRO v0.5.1 — GLideN64 y RSP híbrido

## Motivo y alcance

El usuario confirmó GLideN64 GPU en la pausa de v0.5.0 en PS4, sin mejora
percibida de velocidad. La prioridad es evitar interpretar en CXD4 las tareas
gráficas que GLideN64 puede procesar directamente. No se cambia a Vulkan ni
se aumenta el número de hilos. El resultado sigue siendo experimental.

## Integración

- Ajustes → Gráficos de Nintendo 64 recorre Angrylion CPU, GLideN64 GPU +
  HLE gráfico y GLideN64 GPU + RSP LLE. Se aplica al abrir la próxima ROM.
  Angrylion sigue como valor inicial; esta selección dura la sesión de la app.
- El parche 0008 admite tareas gráficas nuevas y acotadas mediante la tabla
  CRC de microcódigos que ya contiene GLideN64 o el perfil textual F3DZEX
  contrastado con Zelda. No se deduce compatibilidad del nombre del juego.
- La inspección ocurre antes de alterar SP. Tareas desconocidas, reanudadas
  y no gráficas ejecutan CXD4 real. El despachador no usa `hle_execute` completo
  ni confía en el callback upstream `HleForwardTask`, que sigue sin implementar.
- Audio conserva el puente HLE reconocido anterior, su interruptor y respaldo
  CXD4. El modo gráfico nuevo no fuerza otros algoritmos de audio/JPEG.
- La pausa muestra tareas gráficas HLE y despachos gráficos de respaldo LLE.
  Los contadores se reinician al cargar; en el modo RSP LLE completo no se usa
  el despachador híbrido. Un contador de respaldo no equivale a cuadros completos.
- Con Medir rendimiento activado, se mide por separado la entrada/salida del
  contexto entre SDL y núcleo. Son tiempos transcurridos en el hilo anfitrión,
  incluidos posibles bloqueos, no tiempos de ejecución de la GPU. PS4 usa
  `sceKernelGetProcessTime`, evitando la discrepancia CLOCK_MONOTONIC de musl.
  Con el perfil apagado no se consulta ese reloj.
- Se conserva la protección del estado GL, textura/FBO reutilizables, ausencia
  de lectura del framebuffer por cuadro en el frontend y liberación de su copia
  de ROM N64. GLideN64 puede realizar sus propias copias de compatibilidad.

No cambian arranque SDL/GoldHEN, identificadores, rutas, guardados, controles,
marcos ni los núcleos de otras consolas. Los estados rápidos de la ruta GPU
siguen deshabilitados. No se añaden ROMs, BIOS ni microcódigo comercial.

## Validación

- Compilación y enlace de núcleo/aplicación en Linux y OpenOrbis PS4.
- 49/49 CTest aprobados (`build/v051-ctest.log`). Tras ajustar el reloj PS4
  y el tamaño de fuente de los contadores, 10/10 pruebas focalizadas de XMB,
  ciclo de vida hardware, composición y sesiones GPU aprobadas
  (`build/v051-focused.log`).
- `scripts/check-graphics-hle.sh`: ASan/UBSan, fixtures originales de audio
  y RSP escalar, interrupciones, ejecución LLE real, cambios de reconocimiento
  en la misma dirección, límites, audio activado/desactivado y reinicialización.
  La admisión se simula en esta prueba; la clasificación real se contrasta con
  las ROMs autorizadas. Evidencia: `build/v051-hybrid-tests.log`.
- Mario y Zelda: dos modos × dos sesiones × 1.200 VI por juego en el frontend,
  9.600 VI en total. Se verificaron imagen GPU, audio, captura, cierre y vuelta
  a XMB. Las sesiones HLE reconocieron 353/376 tareas en Mario y 523/523 en Zelda, sin
  despachos gráficos de respaldo en esos tramos. Los modos LLE no aceleraron
  tareas gráficas. `build/v051-rom-smoke/results.json` y logs/capturas por caso.
- ROMs leídas desde Downloads, hashes antes/después idénticos y datos aislados
  bajo build/. Estos tramos iniciales no prueban partidas completas.
- Revisión final de la UI: otras dos sesiones Mario de 1.200 VI, contadores
  visibles y línea SDL/núcleo legible, salida normal. Evidencia:
  `build/v051-ui-final/pause.png` y `process.log`.

## Comparación local repetida

`scripts/run-n64-rsp-comparison.py --label v051` con las dos ROMs autorizadas:
dos repeticiones por modo/juego, invirtiendo el orden en la segunda pareja.
Cada proceso ejecuta 300 VI de calentamiento y 1.800 medidos sin input:
16.800 VI totales. Núcleo x64, GLideN64 GLES2 320×240, audio HLE activado,
Linux WSL/Mesa llvmpipe, sin caché de shaders en disco. No es hardware PS4.

| Juego | GPU + RSP LLE, ms/VI | GPU + HLE gráfico, ms/VI | Reducción del tiempo |
|---|---:|---:|---:|
| Mario 64 USA | 2,974 | 1,863 | 37,4 % |
| Zelda OoT U V1.2 | 4,302 | 3,791 | 11,9 % |

Son medias de dos ejecuciones, incluyen una espera de finalización al terminar
el tramo medido y no incluyen presentación SDL. No equivalen a porcentajes de
velocidad PS4 ni permiten atribuir el coste restante exclusivamente a CPU o GPU.
HLE reconoció 888 tareas Mario y 823 Zelda por proceso, sin respaldo gráfico
en esos tramos. Todos devolvieron salida hardware y cero errores GL.

Hashes FNV-1a64 del PCM completo (incluido calentamiento), iguales entre modos
y ambas repeticiones: Mario `14704114601772646918`, Zelda `9788169262844029702`.
Esto comprueba repetibilidad de esas muestras, no calidad auditiva en consola.
Las capturas se repiten exactamente dentro de cada modo, pero difieren entre
modos: se aprecia una diferencia de niebla/fondo en la pantalla de Zelda y
variaciones de bordes en Mario. No se afirma equivalencia visual con LLE ni
se designa un modo como referencia exacta de hardware original.

Resultados: `build/rsp-comparison/v051/results.json`, capturas por caso,
`image-comparison.json` (comparación de SHA-256 de los PNG) y
`build/v051-comparison.log`. El hash del núcleo se comprueba antes/después;
los hashes de las ROMs permanecieron intactos. Esta comparación usa el puente
de audio compartido del parche 0008 y reemplaza, para esta versión, las cifras
exploratorias del HLE completo anterior.

## Prueba en PS4 original/Slim

1. Instalar `R2RETRO-v0.5.1-gpu-hle.pkg`.
2. Seleccionar GLideN64 GPU + HLE gráfico antes de abrir Mario o Zelda.
   Conservar el mismo ajuste de audio en ambas comparaciones.
3. En una escena repetible, abrir OPTIONS y registrar porcentaje, VI/s,
   núcleo ms/VI, nombre del RDP y tareas Gráficos HLE / respaldo LLE.
4. Para localizar esperas, volver al menú, activar Ajustes → Medir rendimiento,
   abrir la misma escena y registrar perfil RSP y SDL ↔ núcleo. La medición
   agrega instrumentación; comparar ambos modos con la misma opción.
5. Repetir con GLideN64 GPU + RSP LLE. Ante artefactos, regresar a ese modo
   o Angrylion y conservar el log y una captura.

La línea RSP incluye llamadas GL y posibles esperas; «CPU y resto» no mide
exclusivamente el R4300. Ni los resultados Mesa ni la compilación del PKG
confirman velocidad, audio o compatibilidad en hardware PS4. Esta versión
no tiene un perfil físico registrado. Tras entregar el PKG, el usuario indica
que el rendimiento sigue igual. No se dispone aún del backend efectivo,
contadores HLE/LLE ni desglose de tiempos de esa ejecución; no extrapolar las
ganancias locales a su consola.

## Paquete

`dist/R2RETRO-v0.5.1-gpu-hle.pkg`: 64.159.744 bytes, SFO `00.51`.
SHA-256: `4889669f6e556fc8b229111be2f5a1c4f650ecbbdb66025bdfacf990f8bb4483`.
El empaquetador extrajo y comparó los payloads, SFO y avisos de licencia.
Metadatos: `dist/build-info-v0.5.1.json`. Se conserva v0.5.0, SHA-256
`0ea552579f5acbdc2fd8636db4f33dc9aae1fb77228172cc7da441e74788cb15`.
