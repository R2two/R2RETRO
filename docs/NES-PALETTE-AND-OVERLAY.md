# NES en PS4: causa del negro, arreglo y marco de consola

Fecha: 7 de octubre de 2026. Cambios compilados (desktop + PS4) y PKG local
validado; **sin publicar en GitHub**.

## El negro de NES en PS4: causa raíz encontrada

Reporte físico: audio correcto y pantalla negra con R2RETRO 0.5.6 en PS4. El
diagnóstico de pausa mostró `No negros: 0`, es decir el núcleo entregaba
cuadros negros, no un fallo de presentación.

Se añadió un diagnóstico temporal de una sola línea al núcleo FCEUmm para
separar paleta de PPU. Resultado en la consola:

```
R2N64-NES-video-diag pixfmt=1 256x240 pitch=1024
  pal[0..3]=00000000,00000000,00000000,00000000
  pal[128..131]=00000000,00000000,00000000,00000000
  gfx100[0..7]=80 80 80 80 80 80 80 80
```

Comparado con Linux (`pal[128..131]=00757575,0024188E,…, gfx100=80 80 …`):

- La **PPU renderiza bien** (`gfx` idéntico a Linux).
- La **paleta está a cero** en PS4.

Un segundo diagnóstico sobre las escrituras de paleta lo confirmó:

```
R2N64-NES-pal-call #0 idx=0 rgb=0,0,0
R2N64-NES-pal-call #1 idx=1 rgb=0,0,0      <- en Linux sería 255,255,211
R2N64-NES-pal-call #7 idx=7 rgb=205,205,205 <- literal del código, sí pasa
```

El literal `205,205,205` (en `.text`) sale bien, pero los valores de
`unvpalette`/`palette` (arrays globales inicializados, sección `.data`) llegan
a cero. El ELF sí contenía la paleta correcta; el problema era de carga.

**Causa: `-fdata-sections`.** Ese flag divide los datos en secciones con nombre
no estándar (`.data.palette`, `.data.unvpalette`, …) que el cargador SELF de
PS4 **no mapea**, por lo que se leen como cero. Afectaba a los núcleos
compilados con ese flag (NES/SNES y portátiles), no al núcleo N64.

**Arreglo:** se quitó `-ffunction-sections -fdata-sections` de
[`build-console-cores.sh`](../scripts/build-console-cores.sh) y
[`build-handheld-cores.sh`](../scripts/build-handheld-cores.sh). No aportaban
nada porque el enlace no usa `--gc-sections`. Verificado en el ELF: `.data.palette`
ya no existe y la paleta está en `.data` con valores correctos. El usuario
confirmó en PS4 que **la imagen ya se ve**.

El diagnóstico temporal se retiró del núcleo tras confirmar el arreglo.

## Marco de consola de NES

El usuario aportó una imagen de una NES (adjunto `0VrdXzi.png`, 1920×1080, con
el hueco de pantalla **transparente**). Se conserva sin recortes ni recoloreado
en [`assets/overlays/nes.png`](../assets/overlays/nes.png) (SHA-256
`b4cc1692fdc5ba3facbf31427e9e710777bb468cde41032dc7f6f32b8e6871ba`).

Medidas del hueco transparente (esquina redondeada): x 258..1661, y 18..1061.

- Apertura (área de juego): `{258,18,1404,1044}`.
- Mate (fondo negro bajo el juego): `{258,18,1404,1044}`.

Registrado en [`src/video.cpp`](../src/video.cpp) (`handheldOverlays`) y activo
por defecto para NES, igual que SNES, respetando siempre un `overlay:false`
explícito en las preferencias existentes. La opción **Marco** aparece en la
pausa (L3+R3) y se guarda por sistema.

Ajustes de soporte: `include/video.h` (arrays de overlays 4→5),
`src/handheld_settings.cpp` (default del marco para NES), `src/play.cpp`
(NES entra en `overlaySystem`), `assets/overlays/README.md`, `scripts/build.sh`
y `scripts/package.py` (copia/validación del asset).

## Validación

- Desktop: **52/52 CTest** aprobados.
- Vista previa del overlay compuesto: `build/desktop/previews/handheld-video-software/nes-artwork.png`
  (marco gris de consola alrededor, juego dentro de la apertura; 4/4 muestras
  visibles en la verificación de composición).
- PS4: PKG `dist/R2RETRO-v0.5.6-color-stability.pkg`, 70.582.272 bytes, SHA-256
  `bb06fc10baeddb58bbc3beb09ebd4a9f60373eca09483823c6b0f1e0257ce20b`, con
  `assets/overlays/nes.png` incluido. Subido por FTP a
  `/data/pkg/R2RETRO-v0.5.6-nes-diag.pkg`.

No hay publicación remota ni manifest actualizado. La versión del PKG sigue
siendo `00.56`.
