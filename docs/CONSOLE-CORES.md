# Núcleos NES y SNES

La ampliación v0.4.0 integra dos bibliotecas libretro estáticas, con símbolos
separados de N64, SameBoy y mGBA. Se conserva el frontend SDL2 y la toolchain
OpenOrbis/PacBrew existente. Esta integración no utiliza RetroArch como frontend.

## Fuentes y perfil

| Sistema | Núcleo y revisión exacta | Perfil | Licencia principal |
|---|---|---|---|
| NES | [FCEUmm `7a542dab1e87679921962a9f056186eca425c0c2`](https://github.com/libretro/libretro-fceumm/tree/7a542dab1e87679921962a9f056186eca425c0c2) | C, estático, HD packs deshabilitados | GPL-2.0-or-later; `Copying`, `Authors` y cabeceras |
| SNES | [bsnes-mercury `79d7f9de218b6ffa65a80bbdc5828532bc239232`](https://github.com/libretro/bsnes-mercury/tree/79d7f9de218b6ffa65a80bbdc5828532bc239232) | C/C++, `PROFILE=performance`, estático | GPLv3; `LICENSE` y avisos por archivo |

FCEUmm incluye el filtro NTSC de Shay Green; el perfil Performance de bsnes
incluye su DSP de sonido. Ambos conservan los avisos LGPL-2.1-or-later de esos
componentes. libretro-common y libco mantienen sus avisos específicos MIT o
dominio público. El inventario completo está en
[THIRD_PARTY_LICENSES.md](../THIRD_PARTY_LICENSES.md).

Se eligió bsnes-mercury frente a Snes9x por el encaje de su licencia con el código
GPL-3.0-or-later de R2N64; el [texto de Snes9x consultado](https://github.com/snes9xgit/snes9x/blob/master/LICENSE)
incluye restricciones de uso comercial. La elección de Performance es un punto
de partida que todavía requiere medición en PS4 original/Slim.

## Construcción reproducible

Desde WSL Ubuntu-24.04, en la raíz del repositorio:

```sh
git submodule update --init external/fceumm external/bsnes-mercury
bash scripts/build-console-cores.sh linux
bash scripts/build-console-cores.sh ps4
```

El script exige los commits fijados, obtiene copias mediante `git archive` y
modifica únicamente `build/console-{linux,ps4}/*-source/`. Los submódulos originales
permanecen intactos. `scripts/namespace-handheld-core.py` separa todas las
definiciones y sus referencias internas con los prefijos `fceumm_` y
`bsnes_mercury_`; las importaciones de libc permanecen compartidas.

Cada destino produce:

- `lib/libfceumm_libretro.a` y `lib/libbsnes_mercury_libretro.a`.
- `fceumm.log`, `bsnes-mercury.log` y `link.log`.
- `provenance.txt`, con pin, compilador y opciones, y `cores.sha256`.
- `licenses/`, con licencias, autores, fuentes exactas y el script de cambios
  locales. `scripts/stage-console-licenses.py` conserva los comentarios completos
  de licencia de libretro-common/libco y los avisos LGPL de Shay Green.

La prueba de enlace usa ambos archivos con `--whole-archive`, de modo que no
oculta dependencias sin resolver en objetos todavía no usados. Linux comprueba
además que ambas APIs libretro reportan versión 1. PS4 utiliza Clang/OpenOrbis
para `x86_64-pc-freebsd12-elf`, sin `-march=native`, AVX ni otro requisito SIMD
añadido. No se añaden trabajadores ni afinidades de CPU.

La corrutina libco de bsnes se compila como C. En el objeto PS4, `co_switch`
pertenece a `.text`; sus únicas referencias externas son `malloc`, `free` y
`__assert_fail`. No usa el camino alternativo de `mprotect` sobre un bloque de
código generado. La inspección está en `build/console-ps4/libco-audit.txt`; esto
es evidencia del binario compilado, no de su ejecución física.

## Adaptaciones locales y acceso a archivos

`scripts/prepare-console-cores.py` aplica estos cambios sobre el pin:

- FCEUmm incorpora sus auxiliares libretro-common al archivo estático y acepta el
  buffer de contenido comprobado por el frontend (`need_fullpath=false`). El
  nombre interno `R2N64-memory.nes` no abre un archivo ni identifica un guardado.
  Se conserva la detección por cabecera/base de datos; no se usan etiquetas de
  región del nombre original del archivo.
- La paleta NES opcional `nes.pal`, dentro del directorio de sistema configurado,
  debe contener exactamente 64 o 512 colores RGB (192 o 1536 bytes). Se rechazan
  otros tamaños y lecturas incompletas, evitando el desbordamiento del buffer
  fijo de upstream y el uso de bytes sin inicializar. Se conserva la paleta
  predeterminada si la paleta externa es inválida o no existe.
- bsnes reinicia los indicadores de fallo y las rutas en cada carga desde
  memoria. Un cartucho que falla por firmware ausente no bloquea la siguiente
  carga. Las solicitudes auxiliares en este modo buscan únicamente en el
  directorio de sistema configurado, sin tomar archivos del directorio de trabajo.
- El HLE NEC de bsnes se limita a los programas conocidos DSP1–4 y ST0010.
  ST0011 comparte el modelo uPD96050, pero utiliza el camino LLE existente y no
  se interpreta incorrectamente como ST0010. La marca de ST0010 activa sus
  funciones existentes de reinicio, encendido y serialización.
- Upstream omite la serialización de DSP1–4 HLE. En esos cartuchos se devuelve
  tamaño de estado cero y las funciones de guardar/restaurar estado retornan
  fallo. Esto no deshabilita la SRAM del cartucho. Los estados de cartuchos
  normales y el estado del ST0010 conservan sus mecanismos existentes.

En la ruta NES usada por R2N64, los auxiliares de nombres solo contemplan
`nes.pal`, `gamegenie.nes` y `disksys.rom` bajo el directorio de sistema; no se
abren archivos de cheats o SRAM junto al cartucho. HD packs están deshabilitados.
Los guardados los gestiona R2N64 mediante la API de memoria libretro, con su
identificador por contenido y directorio separado por sistema.

En SNES, el frontend entrega `path=null` y `meta=null`. El núcleo deriva la
descripción del cartucho de sus bytes; no descubre automáticamente un BML
adyacente ni carga `save.ram` desde el directorio de trabajo. Las solicitudes de
memoria guardable exponen buffers al frontend. La ruta upstream que guarda
archivos por su cuenta requiere el modo manifest, que R2N64 no activa.

## Coprocesadores y firmware

El puente configura `bsnes_violate_accuracy=enabled` y `bsnes_chip_hle=HLE`.
La primera opción es la compuerta upstream que permite aplicar la segunda;
omitirla deja LLE efectivo. Se mantiene `bsnes_superfx_overclock=100%`,
`bsnes_gamma_ramp=disabled` y `bsnes_crop_overscan=disabled`. Región y proporción
conservan los valores automáticos del núcleo.

El HLE upstream cubre DSP1, DSP2, DSP3, DSP4, CX4 y ST0010. No representa una
validación de todos sus juegos. ST0011/ST0018 y otras solicitudes LLE que
necesiten firmware externo fallan si este no está disponible en el directorio
de sistema configurado. El log identifica el archivo solicitado y `load_game`
retorna fallo. No se sustituyen estos datos por ceros para continuar silenciosamente.
No se incluye ni se ha probado firmware externo de estos chips.

El código estándar de bsnes-mercury **sí contiene una constante IPL de 64 bytes
del SPC700**, en `target-libretro/libretro.cpp`; se conserva sin alterarla y forma
parte del núcleo enlazado. Por tanto, «sin archivos externos de firmware» no
significa «sin código de arranque en el núcleo». El directorio `profile/` de su
frontend independiente contiene otros archivos de arranque, no se necesita en
libretro y queda excluido de las copias de construcción y del payload.

FDS, Game Genie, BML externos, MSU1, Super Game Boy y los modos multicartucho no
están expuestos ni validados por esta integración inicial. GB/GBC siguen usando
SameBoy. No se añaden juegos comerciales, claves ni archivos externos de BIOS
al PKG.

## Comprobaciones y límites

El enlace completo pasa en Linux y PS4. La prueba Linux contra FCEUmm real en
`scripts/console-link/palette.cpp` cubre tamaños válidos de 192/1536 bytes,
tamaños inválidos hasta 65536 bytes, una lectura corta simulada, desaparición de
la paleta y recuperación posterior.

`scripts/console-link/snes-chips.cpp` usa un pequeño programa 65816 y descripciones
de cartucho originales para comprobar la selección HLE, el rechazo de estados
DSP1–4, el fallo de ST0011 sin firmware y la recuperación de otra carga. El
contador ST0010 permite contrastar su RAM al guardar, restaurar y reiniciar.
Estos datos de prueba se generan bajo `build/` y no se incluyen en el PKG.

El banco integrado del frontend usa diagnósticos NES/SNES originales, con
evidencia en `build/nes-snes-v040/`. Estas pruebas no demuestran compatibilidad
con un catálogo de juegos, equivalencia de todos los coprocesadores, sonido
audible ni velocidad en una PS4 física. La instalación y ejecución del PKG
siguen siendo una comprobación separada.
