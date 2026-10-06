# R2N64: licencias y procedencia de terceros

Inventario de las ampliaciones GB/GBC/GBA y NES/SNES, revisado el 5 de octubre de 2026. El código
propio usa GPL-3.0-or-later; véase [LICENSE](LICENSE). Cada dependencia conserva
sus autores, licencia y avisos originales. Este inventario identifica el material
utilizado y los textos disponibles; no sustituye esas licencias ni un archivo de
fuentes correspondiente a una distribución concreta.

## Núcleos fijados

| Componente | Fuente exacta | Licencia y ubicación local |
|---|---|---|
| Mupen64Plus-Next / core N64 | [libretro/mupen64plus-libretro-nx, `12edd2c74a517ff86dfa8cfc71ad75e4c10486d5`](https://github.com/libretro/mupen64plus-libretro-nx/tree/12edd2c74a517ff86dfa8cfc71ad75e4c10486d5) | GPL-2.0-or-later en los archivos del core; `external/mupen64plus-next/LICENSE`, `mupen64plus-core/LICENSES` y avisos por archivo. Incluye componentes con otras licencias enumerados allí. |
| RSP HLE | Mismo commit N64, `mupen64plus-rsp-hle/` | GPL-2.0-or-later; `mupen64plus-rsp-hle/LICENSES` y cabeceras de `src/`. |
| CXD4 | Mismo commit N64, `mupen64plus-rsp-cxd4/` | CC0-1.0; `mupen64plus-rsp-cxd4/COPYING`. |
| Angrylion | Mismo commit N64, `mupen64plus-video-angrylion/` | Forma parte del árbol distribuido con la licencia GPL del núcleo. Este directorio del pin no contiene un archivo de licencia independiente; no se le asigna aquí una licencia distinta. |
| GLideN64 y sus auxiliares | Mismo commit N64, `GLideN64/` | `GLideN64/LICENSE` y `GLideN64/licenses/` (incluye avisos de readerwriterqueue, GLSL-FXAA, Glow y gles2n64). Se compila en el árbol upstream, aunque el perfil activo utiliza Angrylion. |
| SameBoy GB/GBC | [LIJI32/SameBoy, v1.0.3-libretro, `8230189896a8bb6598574d302ba0ad3658f98ab4`](https://github.com/LIJI32/SameBoy/tree/8230189896a8bb6598574d302ba0ad3658f98ab4) | Expat, copyright 2015–2026 Lior Halphon; `external/sameboy/LICENSE`. No se compilan los directorios iOS ni HexFiend, que el texto general exceptúa. |
| Boot ROMs de reemplazo SameBoy | Mismo commit SameBoy; `BootROMs/*.asm`, `BootROMs/prebuilt/` | Código abierto del proyecto SameBoy, sujeto a su licencia. Se incrustan estas implementaciones de reemplazo, no BIOS Nintendo. |
| mGBA GBA | [mgba-emu/mgba, 0.10.5, `26b7884bc25a5933960f3cdcd98bac1ae14d42e2`](https://github.com/mgba-emu/mgba/tree/26b7884bc25a5933960f3cdcd98bac1ae14d42e2) | MPL-2.0; `external/mgba/LICENSE` y avisos por archivo. Se compila el core GBA, sin frontend Qt/SDL ni núcleo GB. |
| blip_buf 1.1.0, incluido en mGBA | Mismo commit mGBA; `src/third-party/blip_buf/` | LGPL-2.1-or-later, copyright 2003–2009 Shay Green; `license.txt` y aviso al inicio de `blip_buf.c`. Se enlaza estáticamente. |
| inih, incluido en mGBA | Mismo commit mGBA; `src/third-party/inih/` | BSD-3-Clause, copyright 2009 Ben Hoyt; `LICENSE.txt`. |
| FCEUmm NES | [libretro/libretro-fceumm, `7a542dab1e87679921962a9f056186eca425c0c2`](https://github.com/libretro/libretro-fceumm/tree/7a542dab1e87679921962a9f056186eca425c0c2) | GPL-2.0-or-later; `external/fceumm/Copying`, `Authors` y avisos por archivo. Se compila libretro estático, sin HD packs. |
| Filtro NTSC incluido en FCEUmm | Mismo commit FCEUmm; `src/ntsc/nes_ntsc.c` | LGPL-2.1-or-later, Shay Green; aviso completo conservado en `fceumm-Shay-Green-LGPL-NOTICE.txt`. |
| bsnes-mercury SNES | [libretro/bsnes-mercury, `79d7f9de218b6ffa65a80bbdc5828532bc239232`](https://github.com/libretro/bsnes-mercury/tree/79d7f9de218b6ffa65a80bbdc5828532bc239232) | GPLv3; `external/bsnes-mercury/LICENSE`, `emulator/emulator.hpp` y avisos por archivo. Perfil Performance. |
| DSP de sonido del perfil Performance | Mismo commit bsnes-mercury; `sfc/alt/dsp/SPC_DSP.cpp` | LGPL-2.1-or-later, Shay Green; aviso completo conservado en `bsnes-mercury-Shay-Green-LGPL-NOTICE.txt`. |
| libretro-common FCEUmm y libco bsnes | Commits anteriores; `src/drivers/libretro/libretro-common/` y `libco/`, respectivamente | Avisos por archivo, incluyendo MIT y dominio público; comentarios completos y referencias de commit bajo `licenses/embedded-notices/` en el payload. |
| Cabecera libretro | `external/mupen64plus-next/libretro-common/include/libretro.h`; copias de cada core en sus commits anteriores | MIT/Expat según el aviso de cada cabecera. El aviso principal se reproduce más abajo. |
| libretro-common y libco | Mismo commit N64; `libretro-common/` | Licencias por archivo; utilidades/resampler usan avisos MIT. `libco/libco.c` y `libco/amd64.c` declaran dominio público; amd64 identifica a byuu y fecha 2009-10-12. |

Las rutas abreviadas de la tabla son relativas al submódulo indicado. Los pins son
gitlinks de `.gitmodules`; no se modifica el checkout original durante el build.
No se utiliza RetroArch como frontend. Los backends paraLLEl RSP/RDP están
deshabilitados en el perfil N64; conservar sus avisos en el paquete de fuentes no
significa que se ejecuten.

## Cambios locales y material de reconstrucción

- N64: `scripts/prepare-core.sh` aplica `external/patches/*.patch` a una copia del
  commit fijado. Incluye cierre de sesiones, memoria, comprobación del dynarec,
  audio HLE con retorno a CXD4 y medición de componentes. Los parches conservan
  las licencias de los archivos modificados y se copian junto a los avisos N64.
- Portátiles: `scripts/build-handheld-cores.sh` extrae los commits exactos. Cambia
  la salida de SameBoy a 48000 Hz mediante su API de audio, convierte el target
  libretro de mGBA en archivo estático y fija su metadata al commit/tag oficial.
  Su getter SRAM GBA expone la memoria activa tras la preparación diferida,
  conservando los guardados nativos después de restaurar un estado y cerrar.
  `scripts/namespace-handheld-core.py` separa los símbolos con `sameboy_`/`mgba_`.
  Detalle y comandos: [docs/HANDHELD-CORES.md](docs/HANDHELD-CORES.md).
- NES/SNES: `scripts/build-console-cores.sh` extrae ambos commits y aplica
  `scripts/prepare-console-cores.py` en copias aisladas. Incluye carga desde
  memoria, auxiliares estáticos FCEUmm, lectura limitada de `nes.pal`, limpieza
  de fallos/rutas bsnes, selección HLE conocida, ciclo de vida ST0010 y rechazo
  de estados incompletos DSP1–4 HLE. Los símbolos definidos llevan prefijos
  `fceumm_`/`bsnes_mercury_`. El script de cambios se copia junto a los avisos.
  Detalle: [docs/CONSOLE-CORES.md](docs/CONSOLE-CORES.md).
- Las fuentes completas, submódulos, parches y scripts correspondientes deben
  acompañar la entrega de fuentes del binario. Para componentes enlazados bajo
  LGPL, conservar también el material necesario para reconstruir/reenlazar; un
  enlace a upstream y una copia de la licencia no describen los cambios locales.

## Plataforma y dependencias de la instalación existente

PS4 utiliza `/opt/pacbrew/ps4/openorbis` en WSL Ubuntu-24.04. Las versiones siguientes
son las declaradas por los headers y `.pc` instalados; no identifican por sí solas
el commit del port PacBrew. Desktop utiliza sus bibliotecas Linux separadas.

| Componente | Evidencia/versiones locales | Fuente y avisos |
|---|---|---|
| OpenOrbis / PacBrew | Toolchain existente; Clang 12.0.1 informa `PacBrew/ps4-openorbis` commit `16f27e38760ea21eaf1187d0edbf4342c4a44021` | [OpenOrbis](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain), [licencia general GPL-3.0](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/LICENSE). LLVM, libc, libc++, runtimes y stubs conservan licencias propias; la licencia general no sustituye sus avisos. |
| SDL2 | 2.0.18, `usr/lib/pkgconfig/sdl2.pc`; aviso en `usr/include/SDL2/SDL.h` | zlib; [SDL2](https://github.com/libsdl-org/SDL/blob/SDL2/LICENSE.txt), [port PS4](https://github.com/OpenOrbis/SDL-PS4). |
| SDL2_ttf / SDL2_image | 2.0.15 / 2.0.5, respectivos `.pc` | zlib; [SDL_ttf](https://github.com/libsdl-org/SDL_ttf/blob/SDL2/LICENSE.txt), [SDL_image](https://github.com/libsdl-org/SDL_image/blob/SDL2/LICENSE.txt). |
| libpng / zlib | 1.6.37 / 1.3.1 | Avisos completos en `usr/include/png.h` y `usr/include/zlib.h`; PNG Reference Library License y zlib, respectivamente. |
| libjpeg-turbo | `.pc` indica 2.1.2; `jpeglib.h` remite a README.ijg | Licencias por componente y avisos IJG/libjpeg-turbo; [fuentes 2.1.2](https://github.com/libjpeg-turbo/libjpeg-turbo/tree/2.1.2). No clasificar todo el paquete únicamente como zlib. |
| libwebp | 1.2.1 | Header `usr/include/webp/decode.h` remite a COPYING, PATENTS y AUTHORS; [fuentes v1.2.1](https://github.com/webmproject/libwebp/tree/v1.2.1). |
| FreeType | Header `freetype.h`: 2.10.1; `.pc` informa versión de biblioteca 23.1.17 | Conservar los textos de licencia y avisos del paquete FreeType; [fuentes VER-2-10-1](https://gitlab.freedesktop.org/freetype/freetype/-/tree/VER-2-10-1). La versión `.pc` no es el número de release del proyecto. |
| bzip2/libbzip2 | Header `bzlib.h`: 1.0.6, Julian Seward | Licencia/avisos propios de bzip2; [fuentes upstream](https://sourceware.org/bzip2/). |
| libsamplerate | `.pc`: 0.1.9; `usr/share/doc/libsamplerate0-dev/html/license.html` registra BSD de 2 cláusulas | Avisos propios de libsamplerate; [fuentes 0.1.9](https://github.com/libsndfile/libsamplerate/tree/0.1.9). La copia HTML local contiene metadata histórica y no sustituye el texto completo de COPYING. |
| GoldHEN Plugin SDK | Copia local de R2FPKGI sin revisión Git; identidad en `external/goldhen/SHA256SUMS` | MIT; `external/goldhen/LICENSE`, copyright 2022 GoldHEN; [fuente oficial](https://github.com/GoldHEN/GoldHEN_Plugins_SDK). |
| `libc.prx`, `libSceFios2.prx` | Copias de `samples/piglet/sce_module` de la toolchain instalada | Procedencia del ejemplo público OpenOrbis; conservar los avisos de su distribución. No se atribuye un commit desconocido. |
| NASM | Herramienta localizada por PATH para compilar dynarec N64 | Solo herramienta de construcción, no se incorpora al payload. |
| DejaVu Sans | `assets/fonts/DejaVuSans.ttf` | Licencia Bitstream Vera y cambios DejaVu en dominio público; aviso completo en `assets/fonts/LICENSE.txt`. |

Los textos completos de varias portlibs no están conservados aún en este
repositorio. Registrar esas rutas/versiones no convierte este documento en una
auditoría completa de sus fuentes o de los avisos del binario instalado.

## Aviso de la cabecera libretro utilizada por el frontend

Copyright (C) 2010-2020 The RetroArch team

The following license statement only applies to this libretro API header (libretro.h).

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in the
Software without restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Archivos de avisos preparados para el PKG

La biblioteca en línea v0.4.1 utiliza las portlibs existentes de curl 7.80.0
(licencia curl) y mbedTLS 2.16.6 (Apache-2.0), con avisos en
`assets/licenses/network/`. El conjunto de raíces Mozilla convertido por curl
se conserva en `assets/certs/` con su licencia MPL-2.0 y procedencia fijada.
Los avisos y licencia CA se copian al PKG. Libretro Database y Thumbnails son
proveedores consultados por HTTPS a petición: las bases y carátulas descargadas
se guardan en los datos del usuario, fuera del PKG, y conservan su procedencia;
no adquieren la licencia del código R2N64.

`scripts/build.sh ps4` prepara `build/ps4/payload/licenses/`. Para los portátiles
copia los archivos de `build/handheld-ps4/licenses/`:

- `SameBoy-Expat.txt`.
- `mGBA-MPL-2.0.txt`.
- `mGBA-blip_buf-LGPL-2.1.txt` y `mGBA-blip_buf-NOTICE.txt`.
- `mGBA-inih-BSD-3-Clause.txt`.
- `handheld-versions.txt` y `handheld-sources.md`.

Para NES/SNES, el árbol `build/console-ps4/licenses/` conserva GPL de ambos
núcleos, autores FCEUmm, MIT/libco, LGPL-2.1 y los dos avisos de Shay Green,
procedencia exacta y `R2N64-console-source-changes.py`. El recolector
`scripts/stage-console-licenses.py` incluye los avisos completos por archivo de
libretro-common/libco. El staging del PKG los conserva bajo `licenses/`.

Los avisos N64 se preparan mediante `scripts/stage-core-licenses.py`, conservando
su jerarquía bajo `licenses/Mupen64Plus-Next/`, con parches y `SOURCE.txt`.
Además de LICENSE*/COPYING*, extrae íntegros los comentarios de aviso iniciales
de `libretro-common`/libco y las cabeceras libretro de los tres cores, con URL al
archivo y commit correspondiente, bajo `embedded-notices/`. Incluye avisos del
árbol de utilidades aunque una configuración no compile todos esos archivos;
su presencia no describe qué backend está activo. DejaVu, GoldHEN y R2N64 tienen
copias separadas de licencia. La presencia en staging y la extracción validada
del PKG son comprobaciones distintas.

No se empaquetan cartuchos comerciales, claves ni archivos externos de BIOS o firmware.
El núcleo bsnes-mercury conserva su constante upstream IPL de 64 bytes del SPC700;
no se presenta como una implementación de arranque libre escrita por R2N64.
Los archivos de arranque del directorio `profile/` de su frontend independiente
no se copian al build ni al payload. Véase [docs/CONSOLE-CORES.md](docs/CONSOLE-CORES.md).
`assets/diagnostic.z64` es un programa original de diagnóstico. El fondo y emblema
aportados por el usuario conservan la procedencia de [assets/README.md](assets/README.md);
la licencia del código no los relicencia ni atribuye derechos sobre sus marcas.
