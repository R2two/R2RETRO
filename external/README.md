# Dependencias externas

`goldhen/` contiene la copia MIT utilizada por R2FPKGI, con procedencia y hashes documentados. SDL2, SDL2_ttf, SDL2_image y la toolchain se reutilizan desde PacBrew en WSL; sus binarios no se duplican en Git.

`mupen64plus-next/` es un submódulo de [libretro/mupen64plus-libretro-nx](https://github.com/libretro/mupen64plus-libretro-nx), fijado en `12edd2c74a517ff86dfa8cfc71ad75e4c10486d5`. Incluye la cabecera libretro y conserva los avisos y licencias de sus componentes.

Desde v0.2.0, el núcleo se enlaza al frontend y al PKG PS4. v0.2.1 mantuvo el intérprete con caché y activó cuatro trabajadores Angrylion (uno opcional) y SSE2 en CXD4. v0.2.2 compila el recompilador x64, comprueba los permisos de su caché real antes de usarlo y conserva el intérprete si se rechazan; añade audio HLE para tareas reconocidas, con CXD4 para gráficos/audio desconocido. CPU Automática y audio acelerado son los valores predeterminados, con controles para compararlos desde Ajustes. Desktop utiliza la biblioteca compartida Linux y PS4 el archivo estático compilado con OpenOrbis. v0.2.2 tiene pruebas Linux y PKG validados; la ejecución y rendimiento físicos siguen pendientes.

El submódulo permanece intacto. `scripts/prepare-core.sh` obtiene el commit mediante `git archive` y aplica, solamente en `build/core-lab/source` y `build/core-ps4/source`:

- `patches/0001-libretro-software-session-lifecycle.patch`: cierre completo de la sesión software, liberación de su corrutina y reapertura posterior; incluye descarga antes del primer cuadro.
- `patches/0002-interpreter-compact-memory.patch`: utiliza la reserva compacta ya presente en el núcleo cuando no hay dynarec, evitando intentar reservar una ventana contigua de 512 MiB.
- `patches/0003-angrylion-reset-session-state.patch`: restablece la inicialización del renderer al cerrar, permitiendo cambiar de trabajadores en la siguiente sesión.
- `patches/0004-x64-dynarec-checked-startup.patch`: conserva la memoria compacta con `NEW_DYNAREC` x64, alinea su caché a páginas PS4 de 16 KiB, comprueba permisos de ejecución y expone el backend realmente activo. Si se rechazan los permisos, se selecciona el intérprete.
- `patches/0005-audio-hle-fallback.patch`: utiliza la síntesis HLE upstream únicamente para tareas de audio identificadas; mantiene CXD4 para el resto, incluidos los handlers incompletos MATS/EFZ. Estado independiente por ROM y contador de tareas efectivamente aceleradas.

Los cambios conservan las licencias originales: GPL-2.0-or-later del núcleo/HLE y CC0 de CXD4. El digest de los parches forma parte de la preparación del build y obliga a regenerar las copias cuando cambia. libpng y zlib se toman del entorno del destino, compartidas con las dependencias del frontend. Desde v0.2.2 se requiere NASM en el PATH de WSL para ensamblar el backend x64: es una dependencia adicional del build existente, no una sustitución de OpenOrbis/PacBrew. Los scripts comprueban su presencia y no lo instalan automáticamente.

El laboratorio Linux verifica CPU, RGB, PCM, SI/PIF y estados en memoria; hay pruebas adicionales de RDP paralelo, RSP SIMD y el puente de audio con comandos originales. El usuario confirmó que v0.2.0 abre Zelda OoT y Mario 64 lentamente y aportó capturas de v0.2.1: Zelda 44 % (26,2 VI/s, núcleo 37,0 ms/VI, presentación 0,6 ms/VI) y Mario 32 % (19,4 VI/s, núcleo 50,9 ms/VI, presentación 0,1 ms/VI). El número de trabajadores no está confirmado. **No hay evidencia física de disponibilidad de JIT ni rendimiento de v0.2.2.** Ver [CORE-LAB.md](../docs/CORE-LAB.md) y [STATUS.md](../docs/STATUS.md).

## NES y SNES

`fceumm/` es el submódulo de [libretro/libretro-fceumm](https://github.com/libretro/libretro-fceumm/tree/7a542dab1e87679921962a9f056186eca425c0c2),
fijado en `7a542dab1e87679921962a9f056186eca425c0c2`, GPL-2.0-or-later.
`bsnes-mercury/` corresponde a [libretro/bsnes-mercury](https://github.com/libretro/bsnes-mercury/tree/79d7f9de218b6ffa65a80bbdc5828532bc239232),
fijado en `79d7f9de218b6ffa65a80bbdc5828532bc239232`, GPLv3. Los componentes
incluidos conservan sus avisos individuales, incluyendo LGPL-2.1-or-later de
Shay Green y MIT/dominio público de utilidades.

`scripts/build-console-cores.sh linux|ps4` verifica esos pins, extrae fuentes
aisladas bajo `build/console-*/` y aplica `scripts/prepare-console-cores.py`.
Se compila FCEUmm sin HD packs y bsnes con `PROFILE=performance`; todos los
símbolos propios quedan separados con prefijos `fceumm_`/`bsnes_mercury_`.
No se modifica el checkout original. Los avisos completos y el script de
adaptaciones se conservan en `build/console-*/licenses/`.

La integración carga cartuchos desde memoria, limita la paleta NES externa,
limpia fallos de carga SNES y selecciona HLE solo para programas conocidos.
Corrige los hooks de ST0010 y deshabilita estados incompletos DSP1–4 HLE.
bsnes conserva su constante SPC700 IPL de 64 bytes en el código; el directorio
`profile/` con archivos de arranque del frontend independiente se excluye del
build y del payload. No se añaden archivos externos de firmware o juegos.
Reproducción, límites de firmware y pruebas: [CONSOLE-CORES.md](../docs/CONSOLE-CORES.md).
