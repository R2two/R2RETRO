# Dependencias y procedencia

v0.4.1 añade consultas directas a [Libretro Database](https://github.com/libretro/libretro-database)
y [Libretro Thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails).
Los datos e imágenes se descargan a petición y permanecen fuera del PKG.
El transporte usa curl 7.80.0/mbedTLS 2.16.6 de PacBrew y certificados públicos
Mozilla convertidos por curl; no se desactiva TLS. Fuentes, avisos y hash del
conjunto CA en `assets/licenses/network/` y `assets/certs/`.
Detalles de transporte y pruebas: [NETWORK.md](NETWORK.md).

La ampliación v0.3.0 conserva el frontend, Mupen64Plus-Next y una ROM de diagnóstico original generada desde código propio; añade SameBoy para GB/GBC y mGBA para GBA. No incluye RetroArch ni ROMs comerciales. El perfil N64 conserva el recompilador x64 con comprobación de memoria ejecutable y retorno al intérprete, Angrylion con cuatro trabajadores (uno opcional) y CXD4 SSE2 con audio HLE para tareas reconocidas. Las cargas sintéticas de rendimiento y las listas originales de validación se utilizan solo en pruebas, fuera del PKG. El inventario de licencias, pins y avisos está en [THIRD_PARTY_LICENSES.md](../THIRD_PARTY_LICENSES.md).

| Componente | Procedencia | Uso |
|---|---|---|
| OpenOrbis / PacBrew | `/opt/pacbrew/ps4/openorbis` en Ubuntu-24.04 | LLVM, libc/libc++, SDL2, portlibs y stubs PS4; generación SELF y PKG |
| NASM | Ejecutable del entorno WSL, localizado mediante PATH | Nueva dependencia de compilación v0.2.2 para el ensamblador del backend x64, tanto Linux como PS4; complementa la toolchain existente y no se empaqueta |
| SDL2, SDL2_ttf y SDL2_image | Portlibs de PacBrew | Renderer GLES2/Piglet, texto, JPG/PNG y salida de audio; licencias zlib y avisos de sus dependencias |
| libpng, zlib, libjpeg y libwebp | Portlibs PacBrew en PS4; bibliotecas del sistema en desktop | Dependencias gráficas; núcleo y frontend comparten libpng/zlib del entorno para evitar duplicaciones incompatibles |
| Mupen64Plus-Next | `libretro/mupen64plus-libretro-nx`, revisión `12edd2c74a517ff86dfa8cfc71ad75e4c10486d5` | Núcleo N64 integrado: recompilador x64 o intérprete con caché / Angrylion / CXD4 y audio HLE reconocido; conserva GPL y avisos específicos de sus componentes |
| SameBoy | `LIJI32/SameBoy`, v1.0.3-libretro, revisión `8230189896a8bb6598574d302ba0ad3658f98ab4` | Core libretro GB/GBC y boot ROMs de reemplazo libres; licencia Expat, sin frontend iOS/HexFiend |
| mGBA | `mgba-emu/mgba`, 0.10.5, revisión `26b7884bc25a5933960f3cdcd98bac1ae14d42e2` | Core libretro GBA y BIOS HLE; MPL-2.0, con blip_buf LGPL-2.1-or-later e inih BSD-3-Clause |
| FCEUmm | `libretro/libretro-fceumm`, revisión `7a542dab1e87679921962a9f056186eca425c0c2` | Core libretro NES estático, GPL-2.0-or-later; filtro NTSC de Shay Green LGPL-2.1-or-later; HD packs deshabilitados |
| bsnes-mercury | `libretro/bsnes-mercury`, revisión `79d7f9de218b6ffa65a80bbdc5828532bc239232` | Core libretro SNES, GPLv3, perfil Performance y HLE; DSP de sonido de Shay Green LGPL-2.1-or-later, libco con avisos propios |
| Mupen64Plus RSP HLE y CXD4 | Fuentes incluidas en el mismo commit fijado | Síntesis HLE existente para microcódigos identificados; CXD4 conserva gráficos/audio desconocido. HLE conserva GPL-2.0-or-later y CXD4 su dedicación CC0 |
| Cabecera libretro y libretro-common | Incluidas en el mismo submódulo | ABI de callbacks y utilidades del núcleo; se conservan avisos MIT y demás licencias originales |
| Parches locales del núcleo | `external/patches/` | Sesiones software, memoria compacta, cierre de Angrylion, arranque comprobado del recompilador y puente HLE con retorno a CXD4; conservan las licencias GPL-2.0-or-later o CC0 de los archivos modificados |
| Diagnóstico N64 | `scripts/make_diagnostic_rom.py` | Programa MIPS original: RGB, audio AI y mando SI/PIF; genera `assets/diagnostic.z64`, sin bootcode de Nintendo |
| Fondo Nintendo 64 | JPG adjuntado por el usuario | Copia íntegra en `assets/background.jpg`; procedencia en `assets/README.md` |
| DejaVu Sans | `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` | Fuente incluida; licencia en `assets/fonts/LICENSE.txt` |
| GoldHEN Plugin SDK | Copia de R2FPKGI: `/home/r2a/r2build/third_party/goldhen` | `GoldHEN.c`, `Syscall.c` y headers originales; licencia MIT conservada |
| libc.prx y libSceFios2.prx | `samples/piglet/sce_module` de OpenOrbis | Módulos del ejemplo público, copiados al staging del PKG |

El submódulo de Mupen64Plus-Next permanece sin modificaciones. `scripts/prepare-core.sh` extrae la revisión fijada y aplica los parches de `external/patches/` a las copias de compilación. Los parches y su documentación se conservan junto a los scripts; no se presenta el binario modificado como upstream sin cambios. El audio acelerado reutiliza handlers de ese commit, no incorpora microcódigo extraído de juegos; las tareas desconocidas y los handlers incompletos MATS/EFZ se ejecutan mediante CXD4. El perfil GLideN64 con renderer en hilo no está habilitado ni cubierto por la validación local.

Los submódulos portátiles también permanecen intactos. `scripts/build-handheld-cores.sh` usa copias aisladas, salida SameBoy de 48000 Hz, target estático de mGBA y metadata fijada; separa todos los símbolos definidos como `sameboy_*` y `mgba_*`. Versiones, fuentes y siete archivos de avisos se generan en `build/handheld-ps4/licenses/`, que `scripts/build.sh` copia al payload. Detalles y límites de validación: [HANDHELD-CORES.md](HANDHELD-CORES.md). Los avisos de las portlibs instaladas conservan licencias propias; no están todos recopilados en el repositorio.

v0.4.0 añade FCEUmm y bsnes-mercury manteniendo sus submódulos intactos.
`scripts/build-console-cores.sh` genera archivos estáticos con prefijos
`fceumm_*`/`bsnes_mercury_*` y registra las revisiones y opciones de cada destino.
Los cambios de integración se aplican en `scripts/prepare-console-cores.py` y se
conservan junto a las licencias en `build/console-ps4/licenses/`. El recolector de
avisos incluye libretro-common, libco y los textos LGPL de los filtros/DSP de
Shay Green. bsnes mantiene su constante IPL del SPC700 de 64 bytes incluida en
el código upstream; no se añaden archivos externos de firmware al PKG ni se
copian los archivos de arranque de `profile/`. Perfil HLE, limitaciones de
coprocesadores/estados y reproducción: [CONSOLE-CORES.md](CONSOLE-CORES.md).

`scripts/stage-core-licenses.py` conserva también los comentarios de licencia completos al inicio de las fuentes libretro-common/libco y las cabeceras libretro de los tres cores, bajo `licenses/Mupen64Plus-Next/embedded-notices/`, con referencias a cada commit. Esto cubre los avisos MIT y dominio público que no disponen de archivos LICENSE/COPYING separados.

La copia local de GoldHEN no conserva revisión Git. `external/goldhen/SHA256SUMS` identifica los archivos reutilizados; no se atribuye una revisión desconocida. Se compilan sus dos unidades originales, aunque R2N64 solamente llama `sys_sdk_jailbreak` para acceder a sus directorios propios.

El SDL2 de PacBrew inicializa `USER_SERVICE` y carga Piglet utilizando la ruta del sandbox. Se conserva el orden de arranque de v0.1.2 confirmado por el usuario: gráficos y primer cuadro antes de `sys_sdk_jailbreak`, sin segunda inicialización manual de `USER_SERVICE`; `scePadInit` después de SDL video. R2FPKGI se consultó como referencia y sus archivos no se modificaron.

Referencias oficiales:

- [OpenOrbis](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)
- [Port SDL2](https://github.com/OpenOrbis/SDL-PS4)
- [Ejemplo PacBrew](https://github.com/PacBrew/ps4-openorbis-sample)
- [GoldHEN SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK)
- [Mupen64Plus-Next: revisión fijada](https://github.com/libretro/mupen64plus-libretro-nx/tree/12edd2c74a517ff86dfa8cfc71ad75e4c10486d5)
- [FCEUmm: revisión fijada](https://github.com/libretro/libretro-fceumm/tree/7a542dab1e87679921962a9f056186eca425c0c2)
- [bsnes-mercury: revisión fijada](https://github.com/libretro/bsnes-mercury/tree/79d7f9de218b6ffa65a80bbdc5828532bc239232)
- [ps4-retrobox: distribución Linux para PS4](https://github.com/danyboy666/ps4-retrobox)

`ps4-retrobox` ejecuta RetroArch dentro de Linux sobre PS4. Su funcionamiento no demuestra compatibilidad de una aplicación nativa OpenOrbis. Compilación, pruebas Linux y evidencia física se registran por separado en [CORE-LAB.md](CORE-LAB.md) y [STATUS.md](STATUS.md).

El código propio de R2N64 usa GPL-3.0-or-later (`LICENSE`). Las dependencias conservan sus respectivas licencias y avisos; la imagen del usuario no se relicencia. Este artefacto es una compilación local de desarrollo. Una distribución pública debe acompañarse de las licencias y el código correspondiente a las bibliotecas enlazadas, incluidas las de la instalación PacBrew y los cambios locales del núcleo.
