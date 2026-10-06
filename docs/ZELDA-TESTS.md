# Zelda OoT U V1.2 — 5 de octubre de 2026

**16 sesiones completadas sin cierres ni timeouts**, con imagen y PCM generados,
backend efectivo, trabajo HLE y reapertura comprobados. Son pruebas Linux/WSL
con **PS4 original/Slim como referencia de configuración**; no son mediciones
ni validación de ejecución en consola.

## Correspondencia con PS4

Referencia: `R2N64-v0.2.4-neon-logo.pkg`. Ambas copias del núcleo usan revisión
`12edd2c74a517ff86dfa8cfc71ad75e4c10486d5` y los mismos seis parches. El banco
llama al puente `Emulator::load/run/unload`, compilado en Release, con Angrylion,
CXD4 SSE2, sincronización Low, VI Unfiltered, mapa compacto y JIT comprobado
con respaldo a intérprete. Se contrastaron HLE/LLE y uno/cuatro trabajadores
RDP de forma secuencial, sin cambiar afinidad ni frecuencia del PC.

Anfitrión: Intel Core Ultra 9 275HX, Ubuntu 24.04/WSL2 y GCC 13.3. PS4 utiliza
Clang 12/OpenOrbis, otra ABI, CPU, planificación y permisos JIT. El probe omite
presentación SDL/Piglet, salida física de audio y limitador de `App::play`.
Los tiempos incluyen núcleo y callbacks; excluyen PNG, WAV y cálculo de hashes.
Limitar los hilos o frecuencia del PC no reproduce el hardware de PS4.

## ROM y recorrido

Archivo autorizado: `Legend of Zelda, The - Ocarina of Time (U) (V1.2) [!].z64`,
33.554.432 bytes, identificador `693BA2AE-B7F14E9F`.
SHA-256: `49acd3885f13b0730119b78fb970911cc8aba614fe383368015c21565983368d`.
Leído en Downloads, sin copiarlo al repositorio ni al PKG. Su hash permaneció
intacto en todas las suites. Datos de prueba aislados en `build/zelda-validation/`.

Entrada neutra: introducción, título y demostración automática. Las capturas
se inspeccionaron; no se comprueba una partida controlada ni progreso avanzado.

- Una sesión de 6000 VI con medición de componentes.
- Diez de 6000 VI: cinco perfiles, dos pasadas en orden inverso.
- Dos reaperturas de 6000 VI con datos nuevos por sesión, dentro del mismo proceso.
- Dos reaperturas de 6000 VI compartiendo los datos de prueba.
- Una sesión continua de 18.000 VI, cinco minutos emulados nominales.

Total: **108.000 VI**, treinta minutos nominales acumulados; el máximo continuo
es de cinco minutos emulados. No constituye una prueba prolongada de horas.

## Rendimiento del PC

Medias de dos pasadas de 6000 VI, sin instrumentación de componentes:

| CPU efectiva | Audio | RDP | Media ms/VI | Rango de medias | Media del p95 ms/VI |
|---|---|---:|---:|---:|---:|
| Intérprete con caché | LLE | 4 | 4,078 | 4,026–4,131 | 10,772 |
| Recompilador x64 | LLE | 4 | 2,474 | 2,445–2,502 | 7,427 |
| Intérprete con caché | HLE | 4 | 3,726 | 3,703–3,748 | 10,277 |
| Recompilador x64 | HLE | 4 | 2,280 | 2,277–2,284 | 7,502 |
| Recompilador x64 | HLE | 1 | 4,480 | 4,401–4,559 | 15,963 |

En este PC y recorrido, el perfil predeterminado reduce el coste medio un
44,1 % frente a intérprete/LLE/4. **No predice una mejora del 44,1 % en PS4**.
La pasada instrumentada atribuye 64,5 % a RDP, 18,9 % a RSP, 3,8 % a salida
de vídeo, 2,0 % a audio HLE y 10,7 % a CPU/resto. Incluye espera de trabajadores,
no suma de CPU de todos ellos; no hubo errores de reloj ni ámbitos perdidos.

## Fidelidad y estabilidad

Cada perfil repite los hashes de sus 50 bloques de 120 VI en ambas pasadas.
Intérprete y JIT conservan imagen y PCM al mantener audio y trabajadores.
Las cuatro reaperturas también conservan ambas secuencias.

**Los perfiles no son todos idénticos píxel a píxel:**

- HLE/4 frente a HLE/1 diverge desde VI 481–600. En VI 600 cambian 40 de
  75.840 píxeles, máximo 16 niveles por canal. Angrylion usa semilla de ruido
  por trabajador y reparte filas según su cantidad: explicación compatible
  con estas diferencias deterministas. Véanse `n64video/rdp.c:564–566`,
  `rdp/rasterizer.c:2177` y `rdp/dither.c:68,115–128` en el backend Angrylion.
- HLE/4 frente a LLE/4 diverge dentro de VI 5281–5400, en cinco de cincuenta
  bloques. Las capturas VI 600, 3000, 5400 y 5700 son idénticas; en VI 6000
  cambian siete píxeles, máximo 136 niveles por canal y RMS de imagen 0,516.
  **La causa sigue pendiente**: no se atribuye sin evidencia al ruido, a una
  carrera ni al menor tiempo real que consume HLE.

Cada sesión de 6000 VI produce 8.771.216 muestras PCM intercaladas a 44,1 kHz;
HLE procesa 5726 tareas. Dentro de cada modo de audio, PCM idéntico al variar
CPU, trabajadores y repetición. HLE/LLE difieren: en los últimos diez segundos,
correlación sin desplazar muestras 0,9184, RMS de diferencia 2808,11 unidades PCM y p95
absoluto 6671. No se afirma equivalencia perceptual ni salida física correcta.

Los recorridos de 6000 VI no alcanzan extremos PCM. El extendido registra 25
muestras extremas entre 26.412.540; no basta para atribuir distorsión audible.
Completa 18.000 VI, 17.702 tareas HLE y media de 2,278 ms/VI.

No se confirmó un defecto que justifique modificar el núcleo. Se documentan
las diferencias pendientes sin declarar equivalencia total. El PKG conserva
su hash; estas pruebas no generan otra versión.

## Reproducción y evidencia

```bash
bash scripts/build-core.sh build-only
cmake -S . -B build/ps4-parity-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/ps4-parity-linux --target rom_probe --parallel 4
python3 scripts/run-rom-matrix.py \
  --rom '/mnt/c/Users/R2A/Downloads/Legend of Zelda, The - Ocarina of Time (U) (V1.2) [!]/Legend of Zelda, The - Ocarina of Time (U) (V1.2) [!].z64' \
  --probe build/ps4-parity-linux/rom_probe \
  --output build/zelda-validation/comparison --frames 6000 --scenario intro \
  --profiles cached-lle-4,auto-lle-4,cached-hle-4,auto-hle-4,auto-hle-1 \
  --repetitions 2 --require-av --require-jit --require-hle --timeout 240
```

Componentes: una pasada `auto-hle-4 --profile-core`. Reapertura: una pasada
`--profiles auto-hle-4 --sessions 2`, con `--session-data fresh` y luego
`--session-data shared`, en salidas separadas. Extendido: una pasada de
`--frames 18000 --profiles auto-hle-4`.

El [protocolo PS4](PS4-ZELDA-CHECK.md) permite contrastar escenas, CPU efectiva,
hilos, velocidad y log conservando guardados. shadPS4 no llegó al XMB en las
pruebas previas y no se utiliza como evidencia de rendimiento de PS4.

Evidencia local: `build/zelda-validation/summary.json`, `environment.json`,
`pixel-comparison.json` y los cinco manifiestos referenciados por el resumen.
SHA-256 de biblioteca Linux:
`1dfc838462b596dea9d8d1d05fb526cce72364a60e3353fc46781818dfecd8ff`.
SHA-256 del probe Release:
`4b78a45529a380639f9e8783d8fc76a0a8f2a39479477d73d446f3d13773a405`.
