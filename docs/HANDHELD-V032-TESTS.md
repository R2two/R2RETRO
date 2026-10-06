# v0.3.2 — marcos GB/GBC/GBA y puente de emulación

Los tres overlays proporcionados por el usuario se incorporan a la aplicación
normal, no al laboratorio Pokémon 3D. Se conservan las revisiones fijadas de
SameBoy, mGBA y Mupen64Plus-Next. La validación de escritorio no demuestra
arranque, sonido audible ni velocidad en PS4 física.

## Marcos por sistema

Durante un juego portátil, **L3+R3 → Marco → X** activa o desactiva su marco.
En escritorio, Escape abre la pausa. La preferencia `overlay` se guarda en
`configs/systems/{gb,gbc,gba}.json`, junto al escalado, filtro, velocidad,
espacio de estado y paleta. Es opcional en el formato v1; las configuraciones
anteriores siguen siendo válidas y usan el marco activado por defecto.
La aplicación no reescribe las preferencias solo por abrir un juego.

Las imágenes aportadas se copian byte por byte a `assets/overlays/`; su
procedencia y hashes constan en `assets/overlays/README.md`. Los botones
dibujados son decorativos. Se dibuja primero el marco, después un fondo negro
en su apertura y finalmente el juego. Así no hace falta modificar la imagen
original ni recortar su pantalla.

| Sistema | Apertura a 1920 × 1080 | Juego con escala entera | Juego con ajuste proporcional |
|---|---|---|---|
| GB / GBC | x552, y166, 816 × 748 | x560, y180, 800 × 720 (5×) | x552, y173, 816 × 734 |
| GBA | x310, y106, 1300 × 868 | x360, y140, 1200 × 800 (5×) | x310, y107, 1300 × 866 |

La proporción nativa se conserva con redondeo a píxeles enteros y márgenes
negros donde hacen falta. Desactivar el marco recupera el área de juego
anterior. Nearest y bilineal siguen siendo opciones independientes. N64
mantiene su salida 4:3 y no ofrece marcos portátiles.

Cada marco se decodifica al seleccionarlo y conserva una textura reutilizable;
no se lee el PNG ni se crea esa textura en cada cuadro. La carga se produce
al abrir el juego o desde la pausa. Cada textura RGBA a resolución completa
requiere aproximadamente 7,91 MiB antes de detalles internos del driver;
las tres suman 23,73 MiB si se han usado todos los sistemas. Al faltar un
archivo o ser inválido, se registra el diagnóstico y se continúa sin marco.

## Avance rápido y audio

El puente informa del avance rápido portátil al núcleo y evita copiar al
vector de salida el PCM que el frontend va a descartar. Al soltar R2/Espacio
vuelve la ruta normal. Se siguen ejecutando todos los pasos de emulación;
el cambio no pretende acelerar N64 ni prometer una mejora de FPS en PS4.
Los límites acumulados de audio se siguen comprobando aunque la salida esté
silenciada. Los núcleos fijados mantienen su CPU/APU en marcha; no se cambian
su revisión, frecuencia ni algoritmos de emulación.

## Cobertura de las pruebas

- Configuración: activado/desactivado por sistema, formato anterior sin la
  nueva clave, tipos inválidos, duplicados y persistencia atómica.
- Renderer SDL software y GLES2: apertura completa, proporción, escala
  entera, desactivación, cambio entre sistemas, caché, errores de carga y
  N64 después de un juego portátil. Se inspeccionan píxeles de capturas y
  llamadas reales de creación de texturas y carga de PNG.
- Frontend completo: seis combinaciones de GB/GBC/GBA con el marco activado
  y desactivado, configuración heredada, avance 2x/4x/8x, pausa y reapertura.
  Las capturas de humo incluyen `pause.png` y `pause.png.game.png`, esta última
  antes de dibujar el menú. Los cartuchos automáticos son diagnósticos
  originales generados dentro de `build/`.

## Resultados comprobados

**28/28 pruebas CTest aprobadas**, incluyendo regresión N64, imagen software
y GLES2, puente libretro, SRAM/RTC, estados, audio, entrada y frontend.
Registro: `build/desktop/Testing/Temporary/LastTest.log`.

Las seis configuraciones del frontend completaron dos sesiones de 1.200
cuadros cada una, con todos los marcos en modo activado y desactivado.
Evidencia: `build/desktop/handheld-app-tests/sessions-53cvv682/`.

Para la optimización del audio se ejecutaron dos sesiones nuevas de 480
cuadros por sistema: una normal y otra con avance rápido en los cuadros
360–419. Las 120 observaciones finales de imagen y memoria del diagnóstico
son idénticas. Las muestras de audio posteriores a soltar el avance
(cuadros 420–479) coinciden exactamente:

| Núcleo / sistema | Muestras PCM intercaladas idénticas tras soltar |
|---|---:|
| SameBoy / GB | 96.438 |
| SameBoy / GBC | 96.436 |
| mGBA / GBA | 65.838 |

Se usan sesiones nuevas para esta comparación: los estados de emulación no
necesariamente incluyen el historial de los filtros o remuestreadores del
anfitrión. No se afirma igualdad PCM después de restaurar cualquier estado.

### Pokémon Red autorizado

ROM leída desde Downloads, sin copiarla al proyecto ni al PKG. Su SHA-256
permanece `5ca7ba01642a3b27b0cc0b5349b52792795b62d3ed977e98a09390659af96b7b`.
Dos sesiones Release de **6.000 cuadros** con estado y replay de 60 cuadros
aprobados. Los hashes en paleta Gris conservan los resultados de v0.3.1:
video `c917c6a42cf8cb94`, PCM `eac83c2346310960`.
Informe: `build/v032-pokemon-tests/20261005T175017Z-2573/matrix.json`.

Además, dos sesiones del frontend SDL/GLES2 de 1.200 cuadros con marco GB,
paleta Verde clásico, escala entera y avance 4x comprobaron reapertura y
reanudación del audio. Guardados aislados en `build/v032-pokemon-app-data/`.
Capturas revisadas: `build/v032-pokemon-preview.png.game.png` (juego) y
`build/v032-pokemon-preview.png` (pausa). Se revisaron también los tres marcos
con un patrón de prueba original en
`build/previews/handheld-video-gles2/{gb,gbc,gba}-artwork.png`.

Estas pruebas utilizan audio SDL dummy; comprueban datos y colas, no sonido
audible. No son pruebas de toda la biblioteca ni medidas de PS4.

## Paquete

- `dist/R2N64-v0.3.2-handheld-overlays.pkg`, **50.921.472 bytes**.
- SHA-256: `747d6401f1b8cf6a0f5012472fcdefbf9c1819494ad7f9490a363a76d28db208`.
- SFO `00.32`, título R2N64, Title ID `RNTD00064`.
- Compilación y enlace con OpenOrbis/PacBrew existentes; registro
  `build/v032-ps4-build.log`. Firmas y hashes del paquete aprobados, y contenido
  extraído cotejado con ejecutable, fondo, icono, fuentes, avisos y los tres PNG.
- Metadatos: `dist/build-info-v0.3.2.json`. Se conserva el PKG v0.3.1 con
  SHA-256 `df669d30c24b100362cf8e3364f002dd1cdd81d2fa1d837279050451148bfb52`.
- No se empaquetan cartuchos comerciales, pruebas de usuario ni datos del
  laboratorio Pokémon 3D. El único cartucho incluido es el diagnóstico N64
  original de R2N64.

**Prueba física pendiente:** instalar en PS4, abrir GB/GBC/GBA, alternar Marco
desde L3+R3 y comprobar que la preferencia continúa al cerrar/reabrir. Mantener
y soltar R2 para comprobar el regreso del audio. El arranque y el rendimiento
de v0.3.2 todavía no tienen confirmación del usuario en consola.
