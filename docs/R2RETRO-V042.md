# R2RETRO v0.4.2 — nombre, logo y marco Super Nintendo

## Uso

La aplicación pasa a llamarse **R2RETRO** en PS4, en la ventana desktop, en el
XMB y en los avisos de arranque. El icono utiliza el logo neón aportado por el
usuario, completo y sin deformar, convertido a PNG RGB de 512 × 512 para PS4.
El encabezado del XMB sigue siendo texto; el fondo anterior se conserva.

Durante un juego SNES, abrir **L3+R3**, seleccionar **Marco** y pulsar **X**.
El cambio queda guardado para SNES. El marco está
activado en las preferencias nuevas. Un archivo existente con `overlay:false`
se respeta: puede activarse desde esta opción sin borrar la configuración.

SNES conserva su proporción 4:3 por defecto. La escala entera opcional usa
píxeles cuadrados. Ambas caben dentro del marco; los botones dibujados son
decorativos. Los controles reales del mando no cambian.

## Compatibilidad y recursos

- Title ID `RNTD00064` y Content ID `IV0001-RNTD00064_00-R2N64APP00000001`
  conservados para la continuidad de instalación. SFO `00.42`, título `R2RETRO`.
- Datos en `/data/R2N64`; USB `R2N64/roms`; desktop `R2N64_DATA` y ejecutable
  interno `r2n64`. No hay traslado ni modificación de partidas por el renombre.
- IDs de ROM, estados `R2STATE`, caché `R2META1`, JSON v1 y revisiones de los
  seis núcleos intactos. Las nuevas capturas usan el prefijo `R2RETRO-` dentro
  de las mismas carpetas; las capturas antiguas se conservan.
- `assets/logo.png` y `assets/overlays/snes.jpg` son copias exactas de los
  adjuntos. Procedencia y hashes en los README de `assets/` y `assets/overlays/`.
- JPEG SNES cargado al iniciar el juego o cambiar la opción estando en pausa.
  Se reutiliza la textura; no se lee ni decodifica en cada cuadro. Ante error
  de carga se conserva la presentación del juego sin marco.
- Un mate negro cubre la pantalla blanca del original al componer. La zona
  segura de juego es `(260,24,1400,1032)` dentro de 1920 × 1080; en 4:3 ocupa
  `(272,24,1376,1032)`. No se modifica el JPEG.
- GB/GBC/GBA conservan sus marcos. NES/N64 no usan el marco SNES; el renderer
  comprueba el sistema efectivo al componer para evitar heredar otra selección.

## Verificación

Compilaciones desktop y OpenOrbis/PacBrew aprobadas. **38/38 grupos CTest**
pasaron en 115,05 segundos (`build/v042-ctest.log`), incluidos los seis sistemas,
preferencias, fallos de arranque, bibliotecas y render software/GLES2.

Las pruebas de vídeo verifican encuadre completo SNES normal/alta resolución,
escala entera, caché, cambio entre sistemas y fallo de carga sin marco obsoleto.
Cuatro ejecuciones focalizadas adicionales cubrieron imágenes sintéticas y
originales en software/GLES2; la comparación de todos los píxeles exteriores
confirma los laterales originales y la ausencia de blanco en el mate central.

La prueba frontend ejecutó diez sesiones con diagnósticos originales
(12.000 cuadros en total): NES, SNES 4:3 con marco por defecto, SNES con marco
desactivado en configuración previa y SNES con marco/escala entera/suavizado.
Comprueba preferencias sin escrituras espontáneas, audio al salir del avance
rápido y capturas de juego+marco sin UI ni avance del núcleo. Evidencia:
`build/desktop/console-app-tests/sessions-x1iy8xb5/`.

Capturas inspeccionadas: `dist/R2RETRO-v0.4.2-xmb.png`,
`dist/R2RETRO-v0.4.2-snes-overlay-preview.png` (patrón original de prueba)
y `dist/R2RETRO-v0.4.2-snes-pause.png` (diagnóstico original SNES).
Son pruebas desktop, no medidas de rendimiento ni capturas de PS4.

PKG: **`dist/R2RETRO-v0.4.2-snes-overlay.pkg`**, **63.963.136 bytes**.
SHA-256: `ae1e02977cb368cada13344f31def7d79f66dddd23d79c7907dda3f39feccd11`.
Se validaron firmas/hashes y contenido extraído, incluidos SFO, ejecutable,
icono y cuatro marcos. Metadatos: `dist/build-info-v0.4.2.json`.
El PKG v0.4.1 mantiene su SHA-256 original; fondo y marcos GB/GBC/GBA intactos.

No se ejecutaron ROMs comerciales para esta actualización. La instalación,
ejecución, rendimiento y presentación en PS4 física siguen pendientes.
