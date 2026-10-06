# Pruebas locales con Mario 64 — 5 de octubre de 2026

La validación posterior con Zelda OoT U V1.2 está en [ZELDA-TESTS.md](ZELDA-TESTS.md),
con configuración equivalente del núcleo, límites frente a PS4 y diferencias observadas.

Para **v0.2.2** se ejecutaron **17 sesiones automatizadas** con el archivo proporcionado por el usuario, sin cierres ni timeouts. Son pruebas de integración con una ROM real y entradas sintéticas; se mantienen separadas de los programas MIPS originales del laboratorio y de las pruebas físicas PS4.

En esa validación no se encontró un error reproducible del emulador que justificara modificar el núcleo. Se conserva el PKG v0.2.2. Se añadieron un banco de pruebas optativo, comprobaciones de imagen/audio/backend y evidencia reproducible.

## Regresión de v0.2.3 y reparto del tiempo

Con el parche final del profiler se ejecutaron **dos sesiones adicionales de
6000 VI** con CPU automática/JIT, audio HLE y cuatro trabajadores, una sin medir
y otra con `--profile-core`. Ambas usan datos nuevos y la misma traza de
controles descrita abajo. No hubo cierres ni timeouts; la ROM original mantuvo
su hash y no se copió al proyecto.

La secuencia de imagen fue idéntica (`9fe391f782a5a5cb`) y el PCM también
(`e083c11542dddf9e`, 8.764.734 muestras), tanto entre ambas sesiones como frente
al perfil equivalente de v0.2.2. Cada sesión procesó 5961 tareas HLE. El profiler
contó los 6000 VI sin errores de reloj ni scopes perdidos.

| Componente medido | ms/VI | Parte del tiempo del núcleo |
|---|---:|---:|
| RSP, excluidos RDP y audio HLE | 0,860 | 30,7 % |
| RDP Angrylion, incluida espera de trabajadores | 1,595 | 57,0 % |
| Salida de vídeo | 0,094 | 3,4 % |
| Audio HLE | 0,027 | 1,0 % |
| CPU y resto, incluidos servicios/callbacks/instrumentación | 0,224 | 8,0 % |

Estos porcentajes pertenecen al **PC y a este recorrido**, no a PS4. La media
externa del núcleo fue 2,806 ms/VI sin profiler y 2,803 con profiler: dos muestras
no permiten cuantificar su sobrecoste ni afirmar una mejora. Las diferencias
frente al total interno incluyen el código del puente y resolución del reloj.
El reparto orienta la investigación de RDP/RSP; debe confirmarse en la consola.

Evidencia:

- `build/v023-mario-off/20261005T130419Z-2814/matrix.json`
- `build/v023-mario-profile/20261005T130451Z-2808/matrix.json`
- `build/v023-mario-comparison.json`
- Biblioteca Linux final: SHA-256 `1dfc838462b596dea9d8d1d05fb526cce72364a60e3353fc46781818dfecd8ff`.

El resto del documento conserva la validación histórica de v0.2.2.

## Entorno y aislamiento

- ROM: `Super Mario 64 (USA).z64`, 8.388.608 bytes.
- SHA-256: `17ce077343c6133f8c9f2d6d6d9a4ab62c8cd2aa57c40aea1f490b4c8bb21d91`.
- Lectura en su ubicación original; el hash se comprobó de nuevo al terminar. No se copió la ROM al repositorio ni al PKG.
- Linux/WSL Ubuntu-24.04, Intel Core Ultra 9 275HX, 24 CPU lógicas visibles.
- Núcleo correspondiente a v0.2.2; SHA-256 de la biblioteca Linux: `00fb32d895570e304196e7dd9331be22d82b6769a72b9c77c2ca330b5eed9d8b`.
- Cada perfil usa un proceso y directorio de datos nuevos. Los guardados, logs, PNG y WAV generados están exclusivamente en `build/local-rom-tests/`, excluido de Git y del empaquetado.
- Los tiempos miden `Emulator::run`, sin espera de tiempo real, ventana SDL ni coste de capturar imágenes, guardar WAV o calcular huellas. No son FPS del juego ni medidas de PS4.

## Recorrido y resultados

Ocho sesiones compararon los primeros 600 VI, con dos pasadas en orden inverso: intérprete/JIT y audio LLE/HLE. Cinco sesiones adicionales ejecutaron **6000 VI por perfil**, equivalentes a 100 segundos emulados nominales. La traza pulsa Start en VI 600–611, A desde VI 840 cada 120 VI, y añade movimiento/cámara desde VI 4800.

Las capturas inspeccionadas muestran título, selección de archivo, introducción y exterior del castillo con Mario y Lakitu. El recorrido no completa un nivel ni demuestra compatibilidad con todo el juego.

| CPU | Audio | Hilos RDP | Media núcleo ms/VI | p95 ms/VI | Tareas HLE |
|---|---|---:|---:|---:|---:|
| Intérprete | LLE | 4 | 3,951 | 9,521 | 0 |
| Recompilador x64 | LLE | 4 | 2,824 | 7,652 | 0 |
| Intérprete | HLE | 4 | 3,842 | 9,703 | 5961 |
| Recompilador x64 | HLE | 4 | 2,703 | 7,505 | 5961 |
| Recompilador x64 | HLE | 1 | 4,365 | 12,284 | 5961 |

En esta secuencia y PC, el perfil predeterminado JIT/HLE/4 redujo el tiempo medio del núcleo un **31,6 %** frente a intérprete/LLE/4 (1,46× en rendimiento del núcleo). La diferencia HLE/LLE aislada es pequeña y esta única pasada larga no establece significación estadística. La configuración de cuatro hilos fue más rápida que uno en este recorrido. No se cambian los valores predeterminados a partir de estos datos.

Las cinco ejecuciones tuvieron la misma huella de toda la secuencia de imágenes, `9fe391f782a5a5cb`, y la misma cantidad de muestras de audio: 8.764.734 valores de 16 bits, estéreo a 44,1 kHz. La salida PCM coincide entre intérprete/JIT y entre uno/cuatro hilos cuando se utiliza el mismo modo de audio. La CPU consultada fue realmente JIT en los perfiles automáticos; el contador demuestra trabajo HLE efectivo.

HLE y LLE no generan PCM idéntico. En la muestra inicial de 9,606 segundos, la correlación fue 0,999972, sin desfase medido y con error RMS de 24,254 unidades PCM; el 95 % de las diferencias fue de hasta 8 unidades. Existen diferencias puntuales mayores. En el recorrido largo hubo 11 muestras en los extremos del rango con LLE y 14 con HLE, sobre 8.764.734 muestras. Esto no demuestra audio perceptualmente idéntico ni una salida física correcta.

## Reapertura y comprobaciones del banco de pruebas

Dos sesiones de 3600 VI compartieron sus datos temporales: ambas terminaron, pero el segundo arranque tuvo una secuencia inicial diferente tras restaurar el guardado creado por el primero. Se investigó antes de atribuirlo a memoria residual.

El control con **dos sesiones de 600 VI y datos vacíos independientes dentro del mismo proceso** produjo imagen y PCM idénticos, 577 tareas HLE por sesión y el mismo número de muestras. La diferencia anterior queda asociada a los datos persistentes del juego; no se confirmó un defecto de reinicio del núcleo. En total, las 17 sesiones sumaron 43.200 VI, distribuidos en recorridos cortos; no constituyen una prueba prolongada continua.

El nuevo runner distingue la finalización de un proceso de la validación solicitada. Puede exigir imagen visible, PCM no nulo, recompilador activo y tareas HLE efectivas. Un control negativo con el diagnóstico original —que produce audio por AI y no utiliza tareas RSP de audio— terminó el proceso correctamente, pero fue rechazado al exigir HLE. Así se evita marcar como validado un modo que no está trabajando.

Las **15 pruebas desktop de CTest pasaron**, incluida `local_rom_probe_synthetic`, que comprueba este banco con el diagnóstico propio. Ninguna prueba automática de CTest necesita la ROM del usuario.

## Reproducción

Desde WSL, tras compilar con `bash scripts/build.sh test`:

```bash
python3 scripts/run-rom-matrix.py \
  --rom '/mnt/c/Users/R2A/Downloads/Super Mario 64/Super Mario 64 (USA).z64' \
  --frames 6000 --scenario scripted \
  --profiles cached-lle-4,auto-lle-4,cached-hle-4,auto-hle-4,auto-hle-1 \
  --require-av --require-jit --require-hle
```

Para repetir dos cargas con datos vacíos en el mismo proceso, usar `--frames 600 --scenario intro --profiles auto-hle-4 --sessions 2 --session-data fresh`. `--session-data shared` conserva los datos entre ambas cargas. `--repetitions 2` repite la matriz invirtiendo el orden de perfiles. El timeout es por proceso y queda registrado como fallo, no como una medición válida.

## Evidencia local

Rutas relativas a la raíz del proyecto, fuera de Git:

| Ruta | Contenido |
|---|---|
| `build/local-rom-tests/20261005T114748Z-446/matrix.json` | Ocho arranques, 600 VI por perfil y repetición |
| `build/local-rom-tests/20261005T114748Z-446/audio-comparison.json` | Comparación numérica de WAV HLE/LLE |
| `build/local-rom-tests/20261005T114951Z-280/matrix.json` | Dos sesiones de título con datos compartidos; traza inicial de control |
| `build/local-rom-tests/20261005T115112Z-427/matrix.json` | Cinco recorridos de 6000 VI; tabla de rendimiento anterior |
| `build/local-rom-tests/20261005T115543Z-413/matrix.json` | Dos sesiones con datos independientes; igualdad de imagen/PCM |
| `build/probe-negative-check/20261005T115823Z-252/matrix.json` | Control negativo de HLE, rechazo esperado |

Cada caso contiene `report.json`, `phases.jsonl` por bloques de 120 VI, capturas PNG y los últimos diez segundos de PCM en WAV. No se han ejecutado estos recorridos en PS4. Zelda, audio físico, guardados de progreso avanzado y estabilidad prolongada quedan fuera de esta evidencia.
