# Núcleos portátiles: procedencia y compilación

| Núcleo | Uso | Versión oficial fijada | Commit |
|---|---|---|---|
| [SameBoy](https://github.com/LIJI32/SameBoy/tree/v1.0.3-libretro) | GB/GBC | v1.0.3-libretro | `8230189896a8bb6598574d302ba0ad3658f98ab4` |
| [mGBA](https://github.com/mgba-emu/mgba/tree/0.10.5) | GBA | 0.10.5 | `26b7884bc25a5933960f3cdcd98bac1ae14d42e2` |

Los submódulos `external/sameboy` y `external/mgba` permanecen sin modificaciones.
`bash scripts/build-handheld-cores.sh linux` y `bash scripts/build-handheld-cores.sh ps4`
extraen los commits exactos a `build/handheld-<plataforma>/*-source` y generan:

- `lib/libsameboy_libretro.a`: API `sameboy_retro_*`, vídeo XRGB8888.
- `lib/libmgba_libretro.a`: API `mgba_retro_*`, vídeo RGB565.
- `lib/*.exports` y `lib/*.symbols`: inventario y mapa de símbolos.
- `provenance.txt`, `cores.sha256`, logs y `licenses/`: evidencia de compilación y avisos.

`namespace-handheld-core.py` renombra todas las definiciones globales y sus referencias
internas, conservando las importaciones de libc. Esto también separa auxiliares que
podrían colisionar con Mupen64Plus-Next. Las macros del frontend son
`R2N64_HAS_SAMEBOY` y `R2N64_HAS_MGBA`; no cambian la ABI libretro.

SameBoy utiliza su Makefile libretro con `STATIC_LINKING=1`; conserva los flags
upstream que desactivan debugger, cheats, rewind interno y limitación de tiempo.
La copia aislada adapta una llamada `GB_set_sample_rate` a **48000 Hz**, evitando
emitir PCM a la mitad del reloj GB (~2 MHz). El core sigue administrando su APU.
Sus boot ROMs de reemplazo son código libre de SameBoy; se usan los precompilados
del tag oficial, con fuentes ensamblador incluidas. No se necesita RGBDS ni BIOS Nintendo.

mGBA conserva el listado de fuentes y flags del target libretro; la copia aislada
cambia ese target de `SHARED` a `STATIC` y fija la metadata al commit/tag oficial,
evitando que Git detecte accidentalmente el repositorio padre. Se habilita GBA y se deshabilita
su núcleo GB, frontends Qt/SDL, OpenGL, archivos comprimidos, scripting, debugger y
dependencias opcionales. La API libretro usa `DISABLE_THREADING` y `MINIMAL_CORE=2`.
La ausencia de PNG deshabilita capturas/estados con PNG del frontend propio de mGBA;
la serialización binaria libretro sigue disponible. Se usa la BIOS HLE integrada
cuando no hay una BIOS externa; no se distribuye una BIOS propietaria.

El getter libretro de SRAM GBA expone el buffer activo después de la preparación
diferida: al cargar un estado, mGBA puede usar una máscara temporal de SRAM en
lugar del buffer original. Así se conserva la SRAM restaurada al cerrar el juego.
Antes del primer cuadro se mantiene el buffer original para cargar guardados nativos.

La compilación PS4 usa exclusivamente OpenOrbis/PacBrew instalado y sus pruebas
reales de enlace para detectar funciones. El adaptador CMake selecciona la ruta
POSIX del VFS. `_GNU_SOURCE` expone las declaraciones de la libc musl del SDK.
No se usa `-march=native`, JIT, shaders ni toolchains descargadas.

Licencias incluidas en `licenses/`: SameBoy Expat (el directorio iOS excluido no se
compila), mGBA MPL-2.0, `blip_buf` LGPL-2.1-o-posterior e `inih` BSD-3-Clause.
Conservar estos avisos en el PKG y acompañar las distribuciones con el código fuente
y scripts de construcción correspondientes, incluidos los cambios locales descritos.
El inventario general de SDL/OpenOrbis/Mupen/libretro corresponde al proyecto principal.

`scripts/handheld-link` enlaza ambos archivos completos con `--whole-archive` para
detectar símbolos duplicados o importaciones sin resolver; en Linux también ejecuta
una comprobación de versión ABI. Compilar/enlazar PS4 no demuestra ejecución física.
