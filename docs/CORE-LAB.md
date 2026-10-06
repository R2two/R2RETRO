# Núcleo N64 y laboratorio de validación

Fecha: 5 de octubre de 2026. **El usuario confirmó que v0.2.0 abre Zelda OoT (U) V1.2 y Super Mario 64 en PS4, muy lentamente.** Sus capturas posteriores de v0.2.1 muestran Zelda al 44 % (26,2 VI/s; núcleo 37,0 ms/VI; presentación 0,6 ms/VI) y Mario al 32 % (19,4 VI/s; núcleo 50,9 ms/VI; presentación 0,1 ms/VI). El número de trabajadores elegido no está confirmado. v0.2.2 aprobó las pruebas Linux, la compilación PS4 y la validación del PKG; el arranque y rendimiento físicos siguen pendientes.

## Dependencia y perfil

- Origen: [Mupen64Plus-Next](https://github.com/libretro/mupen64plus-libretro-nx/tree/12edd2c74a517ff86dfa8cfc71ad75e4c10486d5).
- Revisión fijada: `12edd2c74a517ff86dfa8cfc71ad75e4c10486d5`.
- CPU v0.2.2: `WITH_DYNAREC=x86_64`, con NASM y sin `NO_ASM`. El modo Automático comprueba el permiso de ejecución del caché real antes de elegir `NEW_DYNAREC`; su inicialización comprueba de nuevo y retorna al intérprete si se rechaza. El modo Intérprete selecciona explícitamente el intérprete con caché.
- RDP: Angrylion con cuatro trabajadores por defecto (uno opcional), sincronización Low y salida software XRGB8888. RSP: CXD4 SSE2 con puente de audio HLE para tareas reconocidas, activado por defecto; gráficos, tareas desconocidas y audio con handlers incompletos siguen en CXD4. La opción Audio Original desactiva ese puente.
- Un mando N64 conectado. El frontend no solicita rumble.
- libpng y zlib del entorno del destino, compartidas con el frontend.
- Upstream compila GLideN64 aunque se seleccione Angrylion. Linux enlaza libGL; OpenOrbis compila la variante GLES2 y enlaza `ScePigletv2VSH`. El diagnóstico exige cero solicitudes de contexto de GPU del núcleo.

El submódulo no se modifica. `scripts/prepare-core.sh` extrae el commit con `git archive` y aplica estos parches a las copias de `build/core-lab/source` y `build/core-ps4/source`:

| Parche | Motivo |
|---|---|
| `0001-libretro-software-session-lifecycle.patch` | Detener y terminar la corrutina del núcleo, liberar recursos y permitir nuevas sesiones, incluyendo descarga antes del primer cuadro |
| `0002-interpreter-compact-memory.patch` | Usar la reserva compacta existente para el intérprete; evita intentar reservar una ventana contigua de 512 MiB innecesaria para este perfil |
| `0003-angrylion-reset-session-state.patch` | Limpiar el estado de inicialización al cerrar y permitir sesiones sucesivas con distinto número de trabajadores |
| `0004-x64-dynarec-checked-startup.patch` | Usar el mapa compacto con el recompilador x64, alinear su caché PS4, comprobar permisos de ejecución y exponer disponibilidad/backend activo |
| `0005-audio-hle-fallback.patch` | Ejecutar únicamente audio identificado mediante HLE, conservando CXD4 para los demás casos; estado independiente y contador por ROM |
| `0006-core-performance-profile.patch` | Medición opcional inclusiva de retro_run y exclusiva de RSP/RDP/salida de vídeo/audio HLE, sin contabilizar pausas entre llamadas |

El digest de los parches forma parte de la preparación de fuentes. Conservan las licencias de los archivos modificados: GPL-2.0-or-later del núcleo/HLE y CC0 de CXD4. Las comprobaciones descritas se centran en el perfil software; no cubren GLideN64 con renderer en hilo. La CPU activa se consulta después de ejecutar el núcleo; que la preferencia sea Automática no demuestra que el recompilador se haya inicializado.

## Reproducir

v0.2.3 añade `core_component_profile` a CTest y el test aislado
`bash scripts/check-core-profile.sh` (ASan/UBSan, reloj simulado). El profiler usa
`sceKernelGetProcessTime()` en PS4 y el reloj monotónico de libretro en desktop.
No se usa el identificador POSIX de musl directamente con `clock_gettime`
importado de libkernel: las numeraciones no coinciden. La prueba sintética de
RDP compara tres sesiones off/on/off y confirma imagen idéntica, componentes
reales, reinicio y exclusión del tiempo entre llamadas. Las mediciones suman
tiempo de pared del hilo de emulación, incluidas esperas; no uso total de CPU.

`gpu_probe_gles2_restore` comprueba GLSL ES 1.0 y GLES 2.0 en Mesa, con dos
contextos de prueba sucesivos y una textura SDL preexistente que debe sobrevivir.
`gpu_probe_software_fallback` comprueba el rechazo controlado de un renderer sin
OpenGL. `gpu_probe_app_gles2` recorre la pantalla de resultado real y guarda una
captura. Son pruebas desktop; no confirman soporte de GLSL en Piglet.

El banco de ROMs optativo acepta `--profile-core`, que registra componentes
acumulados y rechaza intervalos no medidos, errores del reloj, scopes perdidos o
sumas exclusivas mayores que el total. Sin esa opción exige contadores vacíos.
La API directa de `rom_probe` acepta un argumento final `off|profile`.

Desde PowerShell en la raíz del proyecto:

```powershell
git submodule update --init
.\scripts\build.bat test
wsl -d Ubuntu-24.04 -- bash scripts/build-core.sh
wsl -d Ubuntu-24.04 -- bash scripts/check-core-ps4.sh
.\scripts\build.bat
```

El primer build de pruebas compila la biblioteca Linux y las pruebas del frontend. `build-core.sh` también permite ejecutar por separado el fixture y el host de laboratorio `core_probe`. `check-core-ps4.sh` compila la biblioteca estática y un ELF de comprobación; por sí solo no crea un PKG. El último comando construye la app PS4 completa y su paquete.

El paralelismo predeterminado del núcleo es ocho; puede cambiarse mediante `R2N64_BUILD_JOBS`. Se utiliza OpenOrbis/PacBrew existente en `/opt/pacbrew/ps4/openorbis`, sin instalar otra toolchain. El backend x64 añade **NASM** como dependencia en el PATH de WSL; los scripts comprueban su presencia y registran su versión en la identidad del perfil de compilación. No lo instalan automáticamente. El nombre `.a` se fija explícitamente porque el Makefile upstream conserva por error el nombre `.so` en este perfil estático.

Para probar dos sesiones a través de la aplicación y capturar el framebuffer:

```bash
SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=opengles2 LIBGL_ALWAYS_SOFTWARE=1 \
  SDL_AUDIODRIVER=dummy R2N64_DATA=build/desktop/emulation-smoke-data \
  ./build/desktop/r2n64 --emulation-smoke --accelerated \
  --screenshot build/desktop/emulation-preview.png
```

## Diagnóstico original de 4 KiB

`scripts/make_diagnostic_rom.py` genera `assets/diagnostic.z64`, incluido en el PKG y accesible mediante **Acerca de → Prueba Nintendo 64**. Es código MIPS original ejecutado desde IPL3/DMEM bajo el arranque HLE de Mupen. No contiene bootcode de Nintendo ni contenido de juegos y no pretende arrancar en una N64 física.

El programa escribe un framebuffer RGBA5551 de 320 × 240 en RDRAM `0x100000` y configura VI para mostrar bandas roja, verde y azul. La captura visible del perfil mide 320 × 237. Un marcador de 16 × 16 píxeles en (16,16) es negro en reposo y blanco ante actividad del mando.

| Dirección RDRAM | Valor comprobable |
|---|---|
| `0x400` | Firma MIPS `0x52324E36` (`R2N6`) |
| `0x404` | Contador de bucle, utilizado para verificar avance y restauración de estado |
| `0x408` | Respuesta Joybus: botones en bits 31–16, X con signo en 15–8, Y con signo en 7–0 |
| `0x600` | Buffer DMA de SI de 64 bytes; respuesta alineada en `0x604` |
| `0x200000` | Buffer PCM estéreo de 1024 frames |

**Audio AI.** El propio MIPS genera una onda cuadrada estéreo de ±2048, con periodo de 64 frames. Configura control=1, DACRATE=1520 y bitrate=15. El reloj NTSC dividido por 1521 produce aproximadamente 32006 Hz de muestreo nativo y un tono de 500,1 Hz. Alimenta el FIFO de dos entradas comprobando su bit de lleno. El núcleo remuestrea a 44100 Hz; la salida SDL de la aplicación convierte a 48000 Hz.

**Entrada SI/PIF.** El programa escribe el buffer de formato Joybus `FF 01 04 01 [respuesta de cuatro bytes] FE ...`, con flag de formato en el byte 63. Utiliza DMA SI de escritura y después lecturas repetidas desde PIF `0x1FC007C0`, esperando los bits de ocupado y reconociendo las interrupciones. Copia la respuesta a `0x408`; no se sustituye este recorrido por valores escritos desde el host.

El host verifica reposo, A, combinación B/Z/Start/Arriba, signos de ambos ejes y liberación. A produce `0x80000000`; B/Z/Start/Arriba produce `0x78000000`. También comprueba que el marcador se vuelve blanco y regresa a negro.

## Evidencia Linux y compilación

Con los parches finales de v0.2.2 pasaron **14/14 pruebas desktop y 2/2 del laboratorio**, la compilación/enlace del núcleo estático PS4 y la prueba aislada HLE con ASan/UBSan. Las sesiones del puente alternan intérprete/recompilador y 4/4/1/1 trabajadores; también verifican retorno real al intérprete cuando se deniega memoria ejecutable. Se generaron el ELF/SELF de la aplicación PS4 y el PKG; firmas, hashes, SFO y contenido extraído validados. El rechazo de permisos durante un reinicio duro solo se revisó en el código; esa acción no está expuesta en la interfaz y no se presenta como prueba de recuperación completa.

`cpu_recompiler_output` ejecutó cuatro muestras alternadas intérprete/JIT después del calentamiento: media 65,259 ms / 11,598 ms, razón 5,627×. Cada muestra completó 149 lotes, con resultado y checksum idénticos al cálculo entero independiente del host. Esta carga MIPS original se genera mediante `scripts/make_cpu_benchmark_rom.py` y valida también ramas con delay slots. Evidencia: `build/desktop/cpu-benchmark/cpu-benchmark.json`. Reproducción: `ctest --test-dir build/desktop -R cpu_ --output-on-failure`.

La comparación RDP de la misma validación final obtuvo 1017,910 ms con un trabajador y 310,483 ms con cuatro, razón 3,278× y píxeles equivalentes; evidencia en `build/desktop/rdp-benchmark/rdp-benchmark.json`. Las dos razones proceden de cargas desktop separadas y **no se suman ni predicen velocidad de juegos en PS4**.

Resultados históricos de v0.2.0/v0.2.1, anteriores a los parches 0004 y 0005; se conservan para distinguir la evidencia de cada versión:

| Comprobación | Resultado |
|---|---|
| Pruebas desktop del frontend y RDP | 12/12 aprobadas (diez frontend, fixture RDP y comparación de salida) |
| Fixture y `core_probe` | 2/2 aprobadas |
| Firma MIPS y bandas RGB | Verificadas |
| Cuadros del host aislado | 74, software, 320 × 237 |
| Audio del host aislado | 55.028 frames estéreo; 55.026 no nulos |
| Señal recibida | 44100 Hz; tono medido 500,071 Hz; pico 2533, canales idénticos |
| Entrada del host aislado | 76 polls, 241.460 consultas; botones, stick y liberación verificados mediante SI/PIF |
| Contextos de GPU solicitados por el núcleo | 0 |
| Estado en memoria | Serializar, avanzar, restaurar y reanudar aprobados |
| Puente del frontend | Sesiones repetidas, descarga temprana, audio, entrada y persistencia de saves aprobados |
| App GLES2 | Dos sesiones completas y vuelta al frontend; captura inspeccionada |
| Núcleo estático y ELF de comprobación OpenOrbis | Compilación y enlace aprobados |
| Aplicación PS4 y nuevo PKG v0.2.0 | ELF/SELF compilados; firmas, hashes y contenido extraído aprobados |
| Actualización PS4 v0.2.1 | ELF/SELF y PKG compilados; firmas, hashes, SFO y contenido extraído aprobados |
| Núcleo ejecutado en PS4 física | v0.2.0 arranca Zelda/Mario; capturas de v0.2.1 muestran 44 % / 32 %, respectivamente |

El guardado del frontend utiliza CRC1/CRC2 más una huella FNV64 de la ROM normalizada, para separar contenido distinto y compartir saves entre órdenes de bytes equivalentes. Las pruebas cubren persistencia y conservación de un archivo incompatible. Esto no demuestra compatibilidad de guardados de otros emuladores.

Las pruebas SDL usan audio dummy: comprueban conversión, colas y muestras sin verificar una salida audible física. Linux/Mesa no ejecuta Piglet. Los estados probados son una comprobación del núcleo en memoria; la app no ofrece savestates.

## Archivos de evidencia

| Archivo | Contenido |
|---|---|
| `build/core-lab/source/mupen64plus_next_libretro.so` | Núcleo Linux |
| `build/core-lab/host/core_probe` | Host aislado libretro |
| `build/core-lab/host/diagnostic.z64` | Fixture regenerado |
| `build/core-lab/host/results/diagnostic.png` | Framebuffer emulado capturado |
| `build/core-lab/host/results/result.json` | Métricas y comprobaciones del diagnóstico |
| `build/core-lab/host/Testing/Temporary/LastTest.log` | Log del laboratorio |
| `build/desktop/Testing/Temporary/LastTest.log` | Log de la última ejecución de pruebas del frontend |
| `build/desktop/emulation-preview.png` | Captura real de la app con GLES2 |
| `build/core-ps4/source/mupen64plus_next_libretro.a` | Núcleo estático PS4 |
| `build/core-ps4/link/core_link_check` | ELF de comprobación de enlace |
| `build/core-lab/core.sha256`, `build/core-ps4/core.sha256` | Hashes de los núcleos locales |
| `dist/build-info.json` | Versión, revisión del núcleo, hashes de parches y PKG validado |
| `dist/R2N64-v0.2.0-emulation-alpha.pkg` | PKG anterior validado, 18.808.832 bytes; Zelda/Mario arrancan muy lentos |
| `dist/R2N64-v0.2.0-emulation-preview.png` | Copia de la captura de emulación inspeccionada |
| `dist/R2N64-v0.2.1-performance.pkg` | Actualización validada, 18.874.368 bytes, SFO 00.21 |
| `dist/R2N64-v0.2.1-settings-preview.png` | Captura inspeccionada del selector de hilos |
| `build/audio-hle-test/result.txt` | Resultado aislado del puente HLE con comandos originales |
| `build/desktop/cpu-benchmark/cpu-benchmark.json` | Comparación intérprete/JIT con código original y cálculo entero independiente |
| `dist/R2N64-v0.2.2-jit-performance.pkg` | PKG validado, 19.333.120 bytes, SFO 00.22; evidencia y SHA-256 en STATUS.md |

Los archivos generados permanecen fuera de Git. Las fuentes, parches, scripts y revisión del submódulo permiten repetir las pruebas.

## Prueba física y límites

La prueba de v0.2.2 en consola debe comparar las mismas escenas fotografiadas de Zelda/Mario, anotando porcentaje de velocidad, tiempos de núcleo/presentación, CPU realmente activa, tareas HLE y número de trabajadores. Las opciones de CPU automática/intérprete y audio acelerado/original se aplican a la siguiente ROM y permiten comparar una variable por vez. También debe comprobar el diagnóstico incluido, pausa y carga repetida. Ante un fallo deben conservarse los logs descritos en [STATUS.md](STATUS.md).

Las fotografías aportan mediciones de v0.2.1, pero no detalles de sonido, controles, persistencia o estabilidad prolongada. Tampoco se informó modelo, firmware o GoldHEN. No hay todavía evidencia física sobre disponibilidad de JIT ni rendimiento de v0.2.2.

## Medición de las optimizaciones v0.2.1

`ctest --test-dir build/desktop -R rdp_ --output-on-failure` genera un programa MIPS original que envía 27 rectángulos RDP por intervalo de vídeo. Ejecuta cuatro sesiones alternando 1/4/1/4 trabajadores, verifica la firma de CPU, avance del programa, bandas RGB y píxeles idénticos. Resultado en `build/desktop/rdp-benchmark/rdp-benchmark.json`: 40 intervalos, media de 1082,927 ms con uno y 364,027 ms con cuatro, razón 2,975× en este PC.

`bash scripts/check-rsp-simd.sh` compila las fuentes vectoriales CXD4 con el mismo compilador y opciones en modo scalar y SSE2. Verifica 163.840 operaciones de 40 instrucciones, con acumulador y flags, y referencias independientes para suma/resta saturadas y lógica. Digest idéntico `84c10e5fbb1bfabd`; mediana de cinco muestras de 3.200.000 operaciones: 24,032209 ms scalar / 7,254598 ms SSE2 (3,313×). Evidencia en `build/rsp-simd/scalar.json` y `sse2.json`.

Estas cargas originales no contienen datos de juegos y no se empaquetan. Sus factores son independientes y no predicen FPS de PS4. v0.2.1 conservaba el intérprete. El nuevo parche 0004 gestiona explícitamente el permiso de ejecución y los errores de `mprotect` del caché antes de entrar al recompilador; habilitarlo en el menú no demuestra que pueda ejecutarse en una consola concreta.

## Validación del puente de audio v0.2.2

`bash scripts/check-audio-hle.sh` compila una copia aislada de HLE/CXD4 con el parche 0005 y ejecuta `tests/audio_hle_tests.c`. Por defecto utiliza AddressSanitizer y UndefinedBehaviorSanitizer. No modifica el submódulo ni las copias activas de compilación del núcleo.

Las listas originales ejercitan carga, mezcla y guardado de PCM en ABI1 y ABI2 de Zelda, con muestras esperadas calculadas de forma independiente. Comprueban los bits de finalización, interrupciones, contador real, 600 tareas con direcciones/contenidos cambiantes y reinicio entre sesiones. Un microprograma RSP original comprueba que el camino CXD4 se ejecuta realmente cuando HLE está desactivado, la tarea es gráfica, no está identificada o corresponde a los handlers incompletos MATS/EFZ. Se comprueba que el intento HLE fallido no cambia memoria ni registros; se rechazan desalineación, tamaños inválidos y rangos de detección/lista fuera de RDRAM.

El contador `retro_r2n64_audio_hle_tasks()` cuenta tareas terminadas por el puente y se reinicia al iniciar cada ROM. El diagnóstico incluido utiliza AI directamente y no mide esta optimización. Estas listas sintéticas verifican comportamiento concreto, no compatibilidad completa ni ganancia de velocidad de audio en Zelda/Mario. Los resultados finales de la actualización se registran en [STATUS.md](STATUS.md).

La referencia [ps4-retrobox](https://github.com/danyboy666/ps4-retrobox) utiliza Linux sobre PS4. No se usa como prueba de un port nativo OpenOrbis.
