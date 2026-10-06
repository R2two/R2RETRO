# Vídeo, marcos y núcleos — v0.4.5

## Informe de la consola

El usuario comunicó pantalla negra en **Super Mario Bros. 3 / FCEUmm**,
con unos 60,1 FPS y 100 % en la pausa, marcos que no cargan, y lentitud en
Super Mario 64: 49 %, 25,5 VI/s, núcleo 29,6 ms/VI, recompilador x64,
RDP de cuatro hilos y audio RSP original (LLE). Son datos de una PS4 física
original/Slim; no se confirmó el número de versión ni los sistemas concretos
de los marcos. No se convierten estas cifras de escenas distintas en un
benchmark controlado ni se equipara 100 % del bucle con imagen correcta.

## Marcos

Después del primer cuadro de SDL, antes de habilitar acceso mediante GoldHEN,
se leen los cuatro originales comprimidos: **1.549.492 bytes** en total. Cada
lectura está limitada a 2 MiB, debe corresponder a un archivo regular no vacío,
sin enlaces ni escapes de ruta, y completarse íntegramente. Sus bytes viven
en `Video`; no se necesitan cuatro texturas HD al arrancar.

La primera selección de cada marco usa `SDL_RWFromConstMem` / `IMG_Load_RW`,
comprueba 1920×1080 y conserva la textura. No hay lectura, decodificación ni
creación de textura por cuadro. Cambiar la ruta de assets tras GoldHEN, incluso
dejarla vacía porque no se confirmó el montaje, no borra este caché. Si la
precarga falla, se intenta la ruta resuelta al activar el marco y se conserva
la presentación sin marco ante un error. Los motivos aparecen en logs y en
la pausa; una preferencia activada con recurso fallido dice «no disponible».

Se respetan `overlay:false` y los encuadres GB/GBC/GBA/SNES previos. NES y
N64 no tienen un marco suministrado. El CA y su verificación TLS siguen la
ruta independiente de v0.4.4: este cambio no desactiva certificados ni modifica
el resolver. `startup.log` añade resultado/título de GetAppInfo e identidad
del directorio previo para investigar futuros fallos físicos.

## Vídeo de NES

`CoreFrame` transmite XRGB de 32 bits. La textura anterior lo declaraba ARGB;
la nueva usa `SDL_PIXELFORMAT_RGB888` (XRGB8888 en SDL) y comprueba la
configuración de mezcla. El byte alto sin significado ya no puede convertirse
en transparencia del juego. Los buffers del núcleo no se alteran.

La pausa informa dimensiones y píxeles RGB distintos de negro, con una
inspección al entrar en ella, sin recorrer la imagen cada cuadro durante la
emulación. Esto permite contrastar el núcleo con lo que se ve en la consola.
El fallo físico de SMB3 **no se reprodujo en Linux**; la corrección de contrato
de textura no basta para declarar resuelta la causa física.

Las pruebas de la ROM local y el diagnóstico MMC3 original están en
[NES-SMB3-V045.md](NES-SMB3-V045.md). No se añadió ni distribuyó ninguna ROM
comercial, partida o captura del juego en el paquete.

Una comprobación negativa adicional comparó la textura anterior ARGB8888 con
la nueva RGB888 mediante copias aisladas. Ambas pasaron GLES2/Mesa y produjeron
la misma captura, con alfa 255 en los 1.128.960 píxeles del juego. No se observó
transparencia con la variante anterior en este entorno. Evidencia:
`build/video-opacity-v045/capture-comparison.json`.

## Rendimiento N64

Se comparó una variante de sincronización RDP y selección SIMD del RSP contra
el núcleo actual, con ocho procesos de 6.000 VI e imagen/PCM idénticos por modo
de audio. El cambio redujo 2,6 % el tiempo con LLE y aumentó 2,9 % con HLE en
las medianas locales. **No se incorpora al PKG**: no hay una mejora consistente.
Los parches y revisiones del núcleo permanecen intactos. Dos oráculos originales
añaden cobertura del selector RSP y de las barreras de trabajadores, con
ASan/UBSan; siguen disponibles para futuros experimentos.

La pausa ahora indica cómo seleccionar HLE cuando está activo LLE. El valor
predeterminado ya era HLE; se conservan las opciones de compatibilidad. Esto
no es una nueva aceleración del núcleo ni garantiza velocidad normal en PS4.
Mediciones, distribución de costes y candidatos descartados en
[N64-PERFORMANCE-V045.md](N64-PERFORMANCE-V045.md).

## Validación local

- **45/45 CTest aprobados**, 167,42 segundos, con el diagnóstico MMC3 ampliado,
  render software/GLES2, caché y preferencias, vídeo/audio, estados y resto de
  núcleos. Log: `build/v045-ctest.log`.
- Precarga sin decodificar ni crear texturas, borrado de las fuentes, cambio a
  raíz vacía y posterior decodificación desde memoria de los cuatro marcos.
  Los cambios de sistema y activado/desactivado reutilizan las texturas.
- Los cuatro originales PNG/JPEG pasan además `--artwork` desde memoria con
  ruta vacía, en software y GLES2; paneles SNES idénticos al original y sin halo
  blanco dentro del mate. Logs: `build/v045-overlay-originals-*.log`.
- Lector de assets probado nativo y con operaciones PS4 simuladas, ASan/UBSan,
  y comprobación sintáctica PacBrew. Fuentes originales intactas.
- Super Mario Bros. 3 completa 4.800 cuadros adicionales por el frontend final,
  distribuidos entre software/GLES2 y dos sesiones por caso, con imagen visible.
- PKG compilado, extraído y validado con OpenOrbis/PacBrew; conserva Title ID,
  Content ID, rutas y datos. CA, originales de marcos y parches N64 coinciden
  con v0.4.4. ELF sin `openat/_openat/mkdirat/renameat/unlinkat/fstatat`.
  Prueba física pendiente; el apartado siguiente permite contrastar los fallos.

Artefacto: `dist/R2RETRO-v0.4.5-video-core-fixes.pkg`, 63.963.136 bytes, SFO
`00.45`, SHA-256
`446bd9ea738f191523834dacf0eae8745d12fb5cdce4c7b5d3516ab212f40be2`.
ELF: `a20e32c165ddf92e4a906dfefd483b9b953e928812b592299d792c8d57e71fd9`.
La v0.4.4 conserva SHA-256
`724a8544117e788b3542a47a05bc967919579d739ca644119fe7e52f624d4140`.
Logs del empaquetado: `build/v045-package-build.log`; manifiesto:
`dist/build-info-v0.4.5.json`.

## Contraste en PS4

1. Instalar la v0.4.5 conservando los datos. Abrir Super Mario Bros. 3, esperar
   la introducción y usar OPTIONS como Start. Si sigue negro, abrir pausa
   con L3+R3 y registrar «Imagen … · No negros …», además de la versión y
   los logs `startup.log` / `logs/r2n64.log` bajo la raíz configurada.
2. En GB/GBC/GBA/SNES abrir L3+R3 → Marco → X. Comprobar activado/desactivado
   y volver a cargar el juego. Si indica «no disponible», registrar el motivo
   mostrado. No borrar preferencias ni partidas para probarlo.
3. Antes de abrir Mario 64, ir a Ajustes → Procesamiento de audio y elegir
   **acelerado (HLE)**. En pausa debe aparecer el contador de tareas aceleradas.
   LLE se mantiene seleccionable; no se cambia de modo dentro de una sesión.
4. Comparar en la misma escena, con CPU automático, cuatro hilos RDP y perfil
   de componentes desactivado. Anotar VI/s, núcleo y presentación después de
   estabilizarse. El perfil es una prueba adicional, no el ajuste cotidiano.

Ninguna prueba Linux ni la compilación del PKG demuestra por sí misma velocidad,
sonido, marcos o ausencia de pantalla negra en PS4 física.
