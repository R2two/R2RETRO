# Estado de desarrollo — 7 de octubre de 2026

## v0.5.5 — empaquetado autorizado

El usuario autorizó compilar, empaquetar y publicar. Compilaciones desktop y PS4
terminadas; 52 grupos CTest aprobados (51 en pasada completa y actualizador
repetido tras adaptar su fixture para que represente una versión posterior).
PKG extraído y validado, SHA-256 `1f6fef9555d84a13baba7f91c48b7bb09d6c5782b9cbb80ac174fadea6bbf6c9`,
65994752 bytes, APP_VER00.55. Cliente verifica el manifiesto y el PKG real.
Sin prueba física ni autoactualización física confirmada; no mejora N64 medida.
[Notas completas](releases/v0.5.5.md). Las secciones «sin compilar» inferiores
conservan el estado histórico anterior a esta autorización.

## Cambios pendientes de autorización para compilar

Fuentes ampliadas con shaders LCD/CRT opcionales para GB/GBC/GBA/NES/SNES,
guardado SRAM/RTC desde pausa y salto nativo mGBA 0/1/2. Preferencias antiguas
mantienen ambos efectos desactivados. Pruebas fuente ampliadas, sin ejecutar.
Cloudflare ya cubre RDB y carátulas en el transporte compartido. No se habilita
frameskip N64: omitir presentación no elimina su coste interno. Revisiones
upstream intactas, sin promesa de rendimiento nuevo ni PKG.
[Alcance y validación pendiente](CORE-DISPLAY-IMPROVEMENTS.md).

El usuario confirma Cloudflare operativo en PS4. Código ajustado a Cloudflare
DoH fijo en actualizador y catálogo, sin selector ni preferencia desactivable;
DNS global de consola intacto. Migración de preferencias y fuentes de pruebas
adaptadas, sin ejecutarlas. Logos: seis texturas pequeñas con subida explícita
pendientes de prueba; nueva inspección del PNG confirma color, transparencia y
recortes válidos para los seis sistemas. No se confirma aún la solución física
de los cuadros blancos. **No se compiló ni se generó PKG** para estos cambios.

## v0.5.4 — diagnóstico de red y DNS HTTPS opcional

Responde al bloqueo de búsqueda reportado por el usuario en v0.5.3 física.
Modo DNS Sistema/HTTPS (Cloudflare) solo del actualizador, TLS intacto,
bootstrap del resolver sin tocar DNS PS4. Etapas/segundos, cancelación mediante
curl multi y watchdog cooperativo a 45 s. Círculo vuelve al menú; preferencias
DNS y búsqueda automática editables durante consultas. No se afirma causa
física corregida: el bloqueo original necesita nueva prueba en consola.

52/52 CTest, diez casos DoH y validadores/adaptador con ASan/UBSan aprobados.
Consulta GitHub real por DoH finaliza en 404: canal aún no publicado.
PKG local compilado/extraído/validado `dist/R2RETRO-v0.5.4-updater.pkg`,
65.994.752 bytes, SFO00.54, SHA256
`0b0ea347ff591c5bfddaf78651f5f90b1087e3cafa8d2daebe39921a366f356b`.
v0.5.3 intacta. Instalar por USB para probar la corrección; sin publicación
remota ni autoactualización física confirmada. [Detalles](UPDATER-NETWORK-V054.md).

## v0.5.3 — actualizador experimental, PKG local

Implementado Ajustes → Actualizaciones: canales estable/experimental, búsqueda
opcional al iniciar, descarga HTTPS a disco, SHA-256/identidad/SFO, confirmación
e instalación local mediante BGFT seguida de salida. Sin desinstalar contenido.
51/51 CTest, ASan/UBSan nativo/backend PS4 y adaptador BGFT simulado aprobados.
Descarga real del PKG público v0.5.1 desde GitHub con el transporte nuevo y
verificación completa correcta en Linux. No equivale a instalación PS4.

PKG local `dist/R2RETRO-v0.5.3-updater.pkg`, 65.994.752 bytes, SFO00.53,
SHA256 `a896d970d3226a2b39f680b8bbe725b9bb6f601b4de2f7e64a94426665e51be6`.
Compilado/extraído/validado; v0.5.2 preservada. Incluye también los cambios de
biblioteca, perfiles y arte descritos abajo, que antes estaban sin empaquetar.

El manifiesto queda preparado en `dist/update-v0.5.3-experimental.txt`.
**Sin publicación remota, sin instalación física ni autoactualización confirmada.**
El usuario informa firmware13.52, GoldHEN posiblemente2.4b18.6 y elige USB para
la instalación inicial. Después hará falta una versión posterior publicada
para probar el reemplazo desde el menú. [Implementación y prueba](UPDATER-V053.md).

## Historial de cambios posteriores a v0.5.2 — incluidos ahora en v0.5.3

Logos transparentes N64/NES/SNES/GB/GBC/GBA asociados a sus consolas, en lista,
selección ampliada y cabecera. Atlas cacheado al inicio, sin lecturas por cuadro,
iconos vectoriales como respaldo. Tres pruebas de interfaz aprobadas y ejecutable
PS4 compilado; sin nuevo PKG ni prueba física. [Detalle](CONSOLE-LOGOS.md).

Fondo definitivo elegido por el usuario: `assets/background-room.jpg`, habitación
Nintendo copiada íntegra del nuevo adjunto. Sustituye el PNG generado al iniciar;
contraste aplicado en renderer y JPG anterior preservado como respaldo.

Inicio XMB renovado: nuevo fondo generado `assets/background-home.png`, original
JPG preservado, selección cian de alto contraste e iconos de portátiles/GBA.
7/7 regresiones de vídeo/XMB/GPU y compilación del ejecutable PS4 aprobadas.
Sin nuevo PKG ni prueba física. [Vista y prompt](HOME-ART-DIRECTION.md).

Perfiles N64 automáticos para las revisiones exactas Mario USA y Zelda USA 1.2;
otros juegos mantienen ajustes base. Modo manual y etiqueta en pausa, respaldo
CPU ante fallo inicial GPU. Reserva PCM, restauración GL diferencial y diagnóstico
de driver/picos de tiempo. 50/50 CTest, 11 regresiones finales, 9.600 VI Mario/Zelda
en GLES2 con capturas iguales a v0.5.2; ejecutable PS4 compilado. Sin ganancia
PS4 medida, nuevo PKG, Vulkan ni caché shader en disco. GLES3.2 falla en Mario
también con restauración completa. [Detalles y límites](N64-AUTOMATIC-PROFILES.md).

Biblioteca organizada por carpetas de consola, sin vista «Todos» inicial:
N64, NES, SNES, GB, GBC y GBA. Filtro exacto por sistema, contadores, regreso a
carpetas y selección independiente por consola. 49/49 CTest aprobados en
Linux/Mesa (124,83 s); compilación del ejecutable PS4 correcta. Sin prueba física.
[Comportamiento y pruebas](CONSOLE-LIBRARY.md).

El usuario aplaza la integración GameCube/Wii y devuelve la prioridad a N64.
DolphinPS4 queda como referencia técnica, no como nuevo core en desarrollo activo.
DolphinPS4 auditado en `6b0c6bdfde673c214245250181b1584b2c914787`;
GameCube/Wii **todavía no integrados**. Su RADV/CRT/libc++ requieren un proceso
separado y un adaptador de lanzamiento/retorno. Bootstrap detenido por clang 21
ausente; inventario de otras dependencias con `scripts/check-dolphinps4-build.py`.
No se sustituye OpenOrbis ni se incorporan binarios/ROMs externos al paquete.
[Auditoría y trabajo pendiente](DOLPHIN-PS4-INTEGRATION.md).

## v0.5.2 — compatibilidad N64 GPU

Parche 0009: las tareas gráficas suspendidas pasan a CXD4 incluso con PC de
arranque; GLideN64 recorta nombres internos vacíos sin leer fuera del buffer.
Ambos fallos reproducidos antes del cambio y corregidos con pruebas ASan/UBSan.
No se amplían perfiles HLE ni se desactivan efectos de framebuffer.

49/49 CTest y ocho sesiones Mario/Zelda (9.600 VI) aprobados en Linux/Mesa,
capturas finales idénticas a v0.5.1 por modo, ROMs intactas. PKG compilado,
extraído y validado: `dist/R2RETRO-v0.5.2-compatibility.pkg`, 64.225.280 bytes,
SFO `00.52`, SHA-256
`7cbd17dbfc17081df8077fc6a84cc3e7a09c7de95bf39ebddea4b3f216344691`.
Avisos/parches se actualizan también en empaquetado incremental.
v0.5.1 preservada; sin publicación remota, prueba física ni ganancia de velocidad
certificada. [Evidencia y límites](N64-COMPATIBILITY-V052.md).

## v0.5.1 — GPU y HLE gráfico con respaldo CXD4

PKG: `dist/R2RETRO-v0.5.1-gpu-hle.pkg`, **64.159.744 bytes**, SFO `00.51`.
SHA-256: `4889669f6e556fc8b229111be2f5a1c4f650ecbbdb66025bdfacf990f8bb4483`.
Compilado, extraído y validado; v0.5.0 preservada.

GLideN64 admite tareas gráficas reconocidas mediante el despachador híbrido;
desconocidas/no gráficas conservan CXD4 y el audio HLE anterior. GPU + RSP LLE
queda seleccionable para comparar. Contadores en pausa y medición optativa del
intercambio SDL/núcleo; reloj PS4 de tiempo transcurrido, sin nuevas esperas GL.

49/49 CTest, ASan/UBSan del despachador y 10/10 regresiones focalizadas finales
aprobados. Mario/Zelda completaron 9.600 VI en frontend; 2.400 VI adicionales
de Mario verificaron los contadores visibles. Ocho procesos comparativos
(16.800 VI con calentamiento) redujeron el tiempo local por VI 37,4 % / 11,9 %
con HLE gráfico. PCM repetible e idéntico entre modos en esos tramos;
diferencias visuales entre HLE/LLE. Linux/Mesa, no medidas de PS4.
ROMs intactas, datos de prueba aislados, sin contenido comercial en el PKG.
[Integración, límites y prueba física pendiente](GPU-HLE-V051.md).

Seguimiento del usuario: el rendimiento continúa igual después de la entrega
de v0.5.1. Las ganancias locales no están confirmadas en PS4. Se solicitaron
backend efectivo, contadores HLE/LLE y perfil RSP/SDL para esa ejecución.
La selección GPU no persiste al reiniciar la aplicación. La revisión del código
también confirma valores GLES2 de copia de color a RDRAM `Sync` y profundidad
`Software`; son candidatos a medir, no causas demostradas. No se deshabilitan
globalmente porque afectan compatibilidad y efectos del juego.

## Investigación N64 tras prueba GPU física

El usuario confirma que la pausa de v0.5.0 muestra GLideN64 GPU en PS4,
pero no aprecia una mejora de rendimiento. Se revisó RetroArch/Orbis y
vulkan-ps4: la revisión Vulkan examinada no expone el almacenamiento de
16 bits obligatorio para nuestro ParaLLEl-RDP. Ocho sesiones locales
Mario/Zelda (16.800 VI con calentamiento) compararon RSP CXD4 y HLE:
HLE redujo tiempo local por VI 38,1 % / 14,5 %, con diferencias gráficas
y sin equivalencia PCM certificada. Son escenas iniciales en Mesa llvmpipe,
no velocidades PS4. Banco optativo nuevo, sin cambios de producción ni PKG.
[Fuentes, resultados y trabajo de integración pendiente](N64-RETROARCH-VULKAN-AUDIT.md).

## v0.5.0 — GLideN64 GPU opcional y composición de marcos

PKG: `dist/R2RETRO-v0.5.0-gpu-preview.pkg`, **64.094.208 bytes**, SFO `00.50`.
SHA-256: `0ea552579f5acbdc2fd8636db4f33dc9aae1fb77228172cc7da441e74788cb15`.
Compilado, extraído y validado; v0.4.5 intacta.

Modo GPU experimental en Ajustes → Gráficos de Nintendo 64, aplicado a la
próxima ROM. GLideN64 GLES2 comparte textura/contexto con SDL, con respaldo
Angrylion si falla la inicialización. Conserva CXD4/recompilador/audio HLE y
libera la copia de ROM del frontend. Angrylion sigue por defecto.

Marcos con subida RGB888 comprobada y cuatro muestras una vez por selección;
la pausa distingue una textura seleccionada de una composición visible.
49/49 CTest aprobados, regresiones GLSM/host con ASan/UBSan y sintaxis PS4.
Mario 64 completó 2×1.200 VI con salida GPU real; FireRed 2×1.200 cuadros con
marco GBA visible y 4/4 muestras. Imagen/audio/capturas y regreso a XMB
comprobados en escritorio, ROMs intactas y datos de prueba aislados.
Confirmación posterior del usuario: la pausa indica GLideN64 GPU en PS4,
sin mejora de velocidad percibida. Visibilidad del marco y perfil detallado
en consola pendientes. [Cambios, evidencia y prueba física](GPU-OVERLAY-V050.md).

## v0.4.5 — marcos precargados y diagnóstico de vídeo

PKG: `dist/R2RETRO-v0.4.5-video-core-fixes.pkg`, **63.963.136 bytes**,
SFO `00.45`. SHA-256:
`446bd9ea738f191523834dacf0eae8745d12fb5cdce4c7b5d3516ab212f40be2`.

Los marcos GB/GBC/GBA/SNES conservan sus originales comprimidos antes de
GoldHEN y se decodifican desde memoria al primer uso, sin depender de la ruta
posterior. La textura de juego declara RGB opaco; la pausa informa el estado
real del marco y la imagen recibida del núcleo. Preferencias, datos, CA/TLS,
identificadores y revisiones de núcleos conservados.

**45/45 CTest aprobados**, diagnóstico MMC3 ampliado y SMB3 con imagen visible
en núcleo GCC/Clang Linux y frontend software/GLES2. No se reprodujo el negro
de PS4; ambas variantes de textura pasan opacidad en Mesa. N64 se comparó
durante 48.000 VI: se descartó la variante RDP/RSP porque no mejoró de forma
consistente. No se publica una ganancia de velocidad ni se alteran los parches
del núcleo. El usuario reportó LLE; la pausa indica cómo activar HLE.

PKG compilado, extraído y validado; arte y CA idénticos a v0.4.4 y esa versión
intacta. ELF sin símbolos `*at` incompatibles. Prueba física pendiente.
[Evidencia y pasos de contraste](VIDEO-CORE-V045.md).

## v0.4.4 — acceso al CA y recursos después de GoldHEN

PKG: `dist/R2RETRO-v0.4.4-ps4-ca-assets-fix.pkg`, **63.963.136 bytes**,
SFO `00.44`. SHA-256:
`724a8544117e788b3542a47a05bc967919579d739ca644119fe7e52f624d4140`.

Tras el aviso de CA inválido, se verificó que el certificado del PKG anterior
estaba presente e íntegro. El mensaje ocurría antes de curl/TLS y no distinguía
apertura/lectura fallida de archivo vacío. La plataforma conserva el descriptor
de assets antes de GoldHEN y comprueba su ubicación después; si `/app0` dejó
de servir, busca el mismo directorio por identidad en los slots del título
actual bajo `/mnt/sandbox`. No fija un slot ni elige otro CA. Actualiza también
las rutas del diagnóstico y los marcos tardíos, conservando la interfaz.

Preflight CA POSIX, archivo regular de 1 byte a 2 MiB, errores por etapa/errno.
TLS, CA, identidad, rutas de usuario y núcleos se conservan. **45/45 CTest**,
44 casos HTTPS y resolver en dos backends con ASan/UBSan aprobados. Descarga
HTTPS real de Libretro en Linux con el CA extraído también aprobada. PKG
compilado/extraído/validado y ELF auditado; v0.4.3 intacto. La causa física
exacta y la corrección en consola siguen pendientes de contraste.
[Evidencia y prueba de descarga](PS4-CA-ASSETS-V044.md).

## v0.4.3 — compatibilidad de archivos PS4

PKG: `dist/R2RETRO-v0.4.3-ps4-filesystem-fix.pkg`, **63.963.136 bytes**,
SFO `00.43`. SHA-256:
`1c66f9739b638dc768d940a52093a8d926be2989999fb8bb2bf7ee0b95ea38f0`.

El usuario confirmó un `Invalid argument` al leer `/data/R2N64/roms/gb`
en PS4 física, con juegos en USB. El escáner ahora abre subcarpetas y ROMs
por rutas completas verificadas en PS4; desktop conserva `*at`. Ajustes y
caché también evitan `mkdirat/renameat/unlinkat`, que el ELF anterior enlazaba
como stubs ENOSYS. Los avisos conservan errno y añaden la operación; una
carpeta con error no detiene las otras raíces. El fallo exacto del usuario
todavía requiere contraste físico con el nuevo diagnóstico.

**43/43 CTest aprobados** y pruebas focalizadas ASan/UBSan. PKG compilado,
extraído y validado. La auditoría del nuevo ELF confirma ausencia de los
`*at` incompatibles y las importaciones absolutas correctas. El wrapper
`lstat` defectuoso de musl no estaba enlazado ni se usa en esta versión.
Title ID, rutas, partidas, formatos y núcleos conservados; v0.4.2 intacto.
Sin validación de este PKG en PS4 física.
[Incidente, cambios y prueba USB](PS4-FILESYSTEM-V043.md).

## v0.4.2 — R2RETRO, nuevo logo y marco SNES

PKG: `dist/R2RETRO-v0.4.2-snes-overlay.pkg`, **63.963.136 bytes**, SFO `00.42`.
SHA-256: `ae1e02977cb368cada13344f31def7d79f66dddd23d79c7907dda3f39feccd11`.
Nombre público R2RETRO, icono PS4 derivado del logo aportado. Title ID,
Content ID y rutas R2N64 conservados; sin migración de partidas.

SNES usa el JPEG original como marco, opción **L3+R3 → Marco → X** persistente,
4:3 por defecto. Activo en nuevas configuraciones; `overlay:false` previo
se respeta. Textura reutilizada, mate negro central sin modificar el original
y comprobación del sistema para no heredar el marco en NES/N64.

**38/38 CTest aprobados**, diez sesiones frontend NES/SNES con diagnósticos
originales (12.000 cuadros) y cuatro ejecuciones focalizadas de vídeo
software/GLES2 con arte sintético/original. XMB, icono, pausa y encuadre
inspeccionados. Compilado/extraído/validado el PKG; v0.4.1 intacto.
Sin validación física PS4 ni afirmación de mejora de rendimiento.
[Uso y evidencia](R2RETRO-V042.md).

## v0.4.1 — fichas y carátulas directas desde Libretro

PKG: `dist/R2N64-v0.4.1-libretro-library.pkg`, **63.766.528 bytes**, SFO `00.41`.
SHA-256: `3c4184b072d7ca979f3d1e4093dac3371d1ffc415174be511f4ad77b4cb581c8`.
Compilado, enlazado, extraído y validado con OpenOrbis/PacBrew. v0.4.0 intacto.

Biblioteca: **OPTIONS / M** descarga la ficha del juego seleccionado directamente
desde Libretro Database/Thumbnails, sin PC, cuenta ni API key; el mismo botón
cancela. Coincidencia de contenido por SHA-1 y CRC32 único como respaldo cuando
el registro no tiene SHA-1. Carátula, captura y título opcionales, ficha enriquecida
en XMB y caché para reapertura sin conexión. IDs de guardado y núcleos anteriores
sin cambios; no se empaquetan bases ni carátulas comerciales.

Un trabajador realiza HTTPS, hashes, caché y decodificación; texturas reutilizadas
en el renderer. Iniciar un juego cancela el trabajo y espera mediante el bucle de
eventos antes de entrar en emulación. TLS verificado con CA incluido y límites
de tamaño/tiempo. Caché atómica y validación de datos remotos/locales.

**38/38 grupos CTest aprobados**, incluidos los seis sistemas, software/GLES2,
transporte HTTPS local, caché y cancelación. Catálogo y HTTPS también pasan
ASan/UBSan. Seis RDB reales analizados; Pokémon Red y Mario coinciden por SHA-1.
Red descargó y mostró los tres medios en desktop, y volvió a abrir su ficha con
proxies bloqueados. No se ejecutaron ROMs comerciales en esas pruebas de catálogo.
No se ha comprobado conectividad ni ejecución de v0.4.1 en PS4 física.
[Uso, pruebas y límites](LIBRARY-V041.md).

## v0.4.0 — NES y SNES

PKG: `dist/R2N64-v0.4.0-nes-snes.pkg`, **61.931.520 bytes**, SFO `00.40`.
SHA-256: `5056973957762b10f09d85b0b88bf7cc548b204ead773bd7a7d617ddeda0ecd7`.
Compilado, enlazado, extraído y validado con OpenOrbis/PacBrew; prueba física pendiente.

NES usa FCEUmm y SNES usa bsnes-mercury Performance con símbolos separados.
Biblioteca, scanner, controles, guardados y preferencias incorporan ambos sistemas.
Acepta `.nes`, `.sfc` y `.smc`; presentación 4:3 por defecto, escala entera
opcional, cinco espacios de estado cuando el núcleo lo permite, capturas,
reinicio y avance rápido 2x/4x/8x. Las copias SNES con cabecera de 512 bytes
comparten identidad y guardados con el contenido equivalente sin cabecera.

Correcciones en copias aisladas de los núcleos: límites y lectura completa de
paletas NES, recuperación tras firmware SNES ausente, ciclo de vida ST0010 y
rechazo de estados incompletos DSP1–4 HLE. ST0011 conserva su ruta LLE; no se
ejecuta como ST0010. No se distribuyen ROMs comerciales ni archivos externos
de firmware. Las revisiones de N64, SameBoy y mGBA permanecen iguales.

**33 grupos CTest aprobados**, contando la ejecución completa y la repetición
del único fallo de lanzamiento: `Text file busy` al coincidir con el enlace del
ejecutable desktop. La repetición posterior al enlace pasó sin cambiar código.
Los diagnósticos originales NES/SNES cubren **4.844 cuadros** en los núcleos
y **9.600 cuadros** en ocho sesiones del frontend; también se verifican
paletas malformadas y el ciclo de vida de chips. No se han probado juegos
comerciales NES/SNES, sonido audible ni rendimiento físico PS4.
PKG v0.3.3 intacto. [Uso, evidencia y límites](NES-SNES-V040.md).

## v0.3.3 — diagnósticos de núcleos y capturas

PKG: `dist/R2N64-v0.3.3-core-app-upgrades.pkg`, **50.921.472 bytes**, SFO `00.33`.
SHA-256: `6d33f9e2ffa6e7bdeb9b947c0166dc3b41a954af5ee19078a1b48d41b973a7e8`.
Compilado, enlazado, extraído y validado con OpenOrbis/PacBrew; prueba física pendiente.

Agrupación exacta de INFO DMA rutinario de mGBA en cuatro contadores y un
resumen al cerrar. FireRed pasa de 3.134.515 a 2.199 bytes de log en el mismo
recorrido de 24.000 cuadros; se conservan los otros diagnósticos. No se han
medido ganancias de FPS en PS4. Revisiones de núcleos sin cambios.

Pausa incorpora Capturar imagen (PNG del juego y marco sin menú) y la opción
Estadísticas para mostrar/ocultar el HUD portátil. Capturas por sistema y
raíz de datos configurada, nombres exclusivos y limpieza ante fallo. Ajuste
showStats opcional y compatible con las preferencias v1 anteriores.

**28/28 CTest**, incluidos doce recorridos del frontend, escritura fallida de
captura y conservación de imágenes anteriores. Silver, Crystal, Gold y FireRed:
**96.000 cuadros**, ocho replays y hashes de video/PCM idénticos a v0.3.2.
SRAM idéntica y RTC persistido/restaurado; reloj prolongado no validado.
Se conserva intacto el PKG v0.3.2. [Evidencia y límites](CORE-APP-V033-TESTS.md).

## v0.3.2 — marcos GB/GBC/GBA

PKG: `dist/R2N64-v0.3.2-handheld-overlays.pkg`, **50.921.472 bytes**, SFO `00.32`.
SHA-256: `747d6401f1b8cf6a0f5012472fcdefbf9c1819494ad7f9490a363a76d28db208`.
Compilado, enlazado y validado con OpenOrbis/PacBrew; prueba física pendiente.

Los tres PNG aportados por el usuario se conservan sin modificación. Marco
activado por defecto y conmutación en pausa L3+R3, persistente por sistema.
Juego con proporción nativa y escala/filtro elegidos; texturas reutilizadas.
Fallo de carga del marco permite continuar sin él. N64 mantiene su vista 4:3.

El puente libretro evita copiar PCM descartado durante avance rápido portátil
y conserva los límites de callbacks. No se actualizan las revisiones de cores.
**28/28 pruebas aprobadas**, con marcos activados/desactivados en doce sesiones
del frontend. Comparación de avance rápido contra ejecución normal: imagen,
memoria y PCM al soltar idénticos en GB/GBC/GBA. Pokémon Red: dos sesiones de
6.000 cuadros conservan los hashes de video/PCM de v0.3.1; otras dos sesiones
SDL comprueban el marco y las preferencias. No son cifras de PS4.
[Resultados y límites](HANDHELD-V032-TESTS.md).

Validación posterior autorizada con Pokémon Silver, Crystal Rev 1, Gold y
FireRed Rev 1: cada juego completó dos sesiones de 12.000 cuadros, replay de
estado y prueba del frontend con marco activado/desactivado. Llegaron al
dormitorio del protagonista; hashes video/PCM repetidos y ROMs intactas.
FireRed generó muchos logs informativos DMA con escritura síncrona, corregidos
posteriormente en v0.3.3. Aquella prueba no cambió el PKG y no hubo prueba
física. [Resultados de los cuatro juegos](POKEMON-FOUR-ROM-TESTS.md).

## Investigación 3D — Pokémon Red

El usuario eligió investigar un modo 3D real empezando por Pokémon Red.
Se completó un laboratorio nativo aislado con Pueblo Paleta, ambas plantas
de la casa de Red y el laboratorio de Oak, atlas de texturas, mobiliario y
fachadas, sprites animados con paletas diferenciadas, Poké Balls volumétricas,
cámara de seguimiento, sombras sencillas y diálogos/menús sobre 3D. SameBoy
conserva toda la lógica y decide qué objetos permanecen visibles. F1 alterna
3D/2D sin reiniciar; la preferencia se conserva al salir. Paleta pasa de
81.392 a 3.910 triángulos. Siete grupos de pruebas aprobados; recorrido local
final de 22.844 cuadros y seis comprobaciones de restauración de WRAM.
Tres alternancias de vista conservan idénticas la WRAM y la imagen original
del juego frente a una ejecución sin alternar. No son medidas de PS4.
Datos derivados y guardados aislados solo en `build/`.
No se incorporó DramaticShapeVoxelMod y el laboratorio sigue fuera de los PKG,
incluida v0.4.1. Su ejecución en PS4 está pendiente.
[Resultados y capturas](VOXEL-FEASIBILITY.md).

## v0.3.1 — controles, imagen y preferencias portátiles

PKG: `dist/R2N64-v0.3.1-handheld-upgrades.pkg`, **49.217.536 bytes**, SFO `00.31`.
SHA-256: `df669d30c24b100362cf8e3364f002dd1cdd81d2fa1d837279050451148bfb52`.
Compilación OpenOrbis/PacBrew y validación del paquete completadas. El usuario
indicó que todavía no ha probado v0.3.0; v0.3.1 tampoco tiene prueba física.

- R2 mantenido permite avance rápido con objetivo 2x/4x/8x, condicionado al
  rendimiento disponible. Audio silenciado durante el avance y restaurado al
  soltar; lotes de trabajo limitados para atender el mando con frecuencia.
- Cinco espacios de estado por juego portátil, manteniendo compatible el
  archivo del primer espacio. N64 conserva su menú y controles anteriores.
- Cuatro paletas GB, escalado entero o ajuste proporcional y filtro nearest
  o bilineal. Preferencias persistentes por sistema en `configs/systems/`.
- **28/28 pruebas CTest aprobadas**: regresión N64, núcleos portátiles,
  persistencia, estados, imagen SDL software/GLES2, entrada, audio y paquete.
- Integración de frontend: ocho sesiones sintéticas y dos con Pokémon Red
  comprueban carga de preferencias, avance rápido y reanudación del audio.
- Pokémon Red: dos sesiones Release de 6.000 cuadros; estados y replay
  aprobados. Los hashes de imagen y PCM coinciden con v0.3.0 usando Gris.

Se conservan los hashes del PKG v0.3.0, fondo, logo e icono. Las pruebas Linux
no miden velocidad de PS4 ni verifican sonido audible en hardware. Detalle,
evidencia y controles: [HANDHELD-V031-TESTS.md](HANDHELD-V031-TESTS.md).

## v0.3.0 — GB/GBC/GBA y frontend multicore

PKG: `dist/R2N64-v0.3.0-multisystem.pkg`, **49.217.536 bytes**, SFO `00.30`.
SHA-256: `8ae4cd4ae68d09cc6bbb739d1f9bd374a3303652dfc38494e53650203836b3d0`.
Compilación OpenOrbis/PacBrew y validación de firmas, hashes, SFO, ejecutable,
imágenes y avisos de licencia extraídos aprobadas. Ejecución física pendiente.
El PKG v0.2.4, el fondo y el icono conservan sus hashes previos.

SameBoy para GB/GBC y mGBA para GBA comparten CoreManager y callbacks libretro
con Mupen64Plus-Next. Hay filtros de biblioteca, escalado entero, controles
portátiles, guardados/RTC por sistema, estados y reset desde pausa. Se preservan
los guardados N64 planos como respaldo. La serialización N64 con audio HLE queda
oculta por carecer del estado auxiliar HLE; con LLE se comprueba la ruta existente.

- **23/23 pruebas de frontend/core aprobadas**, más el nuevo caso CTest GP4
  con sus **2 pruebas Python aprobadas** (24 entradas CTest registradas en total).
- Pokémon Red: dos sesiones Release de 6.000 cuadros y dos restauraciones con
  replay idéntico de 60 cuadros; video/PCM coincidentes. SDL/GLES2 también pasa
  dos sesiones y reapertura de SRAM. Portada, menús y entrada de nombres observados;
  no equivale a validar la partida completa ni la velocidad en PS4.
- GB/GBC/GBA sintéticos: video, PSG, entrada, guardados, estados, rechazo de
  corrupción, cierre/reset inmediato y cambio de núcleo aprobados. Se corrigió
  el getter SRAM mGBA para consultar la memoria activa restaurada.
- Zelda: 6.000 VI con JIT/HLE reproducen los hashes de imagen/PCM y los 50 bloques
  del baseline previo. Biblioteca N64 intacta; comparación funcional de PC.
- El generador GP4 confundía carpetas homónimas bajo padres distintos. Se
  reconstruye el árbol por rutas completas, conservando todos los avisos.

Resultados, límites y prueba física: [MULTISYSTEM-V030.md](MULTISYSTEM-V030.md).
La ampliación sigue el MVP del Markdown aportado; rewind, fast forward, shaders,
remapeo configurable y configuración persistente quedan para siguientes etapas.

## Confirmación recibida de consola

El usuario confirmó el arranque de v0.1.2 y después que **v0.2.0 abre Zelda Ocarina of Time (U) V1.2 y Super Mario 64, pero ambos funcionan muy lentos**. Posteriormente aportó fotografías de la pantalla de pausa de v0.2.1 con estas mediciones:

| Juego / captura | Emulación | VI/s | Núcleo | Presentación |
|---|---:|---:|---:|---:|
| Zelda OoT (U) V1.2, `IMG_0242.jpeg` | 44 % | 26,2 | 37,0 ms/VI | 0,6 ms/VI |
| Super Mario 64, `IMG_0243.jpeg` | 32 % | 19,4 | 50,9 ms/VI | 0,1 ms/VI |

Las imágenes están en `C:/Users/R2A/Downloads/`. Confirman ejecución y mediciones de esas escenas, sin confirmar audio, guardados o compatibilidad completa. **No se informó si estaban seleccionados uno o cuatro trabajadores.** Posteriormente el usuario identificó la consola como PS4 original/Slim; firmware y GoldHEN siguen sin informar. El tiempo dominante se observa dentro del núcleo, no en la presentación SDL.

## v0.2.4 — logo de neón

Validación local posterior con Zelda OoT U V1.2: **16 sesiones completadas**,
108.000 VI acumulados y una sesión continua de 18.000 VI. Banco Release con
la misma revisión/parches/opciones del núcleo; consola de referencia
confirmada por el usuario: PS4 original/Slim. Sin cierres ni timeouts; CPU
efectiva, HLE y reapertura comprobados. Hay diferencias pequeñas de imagen
entre perfiles y diferencias de PCM HLE/LLE: no se declara equivalencia total.
No son mediciones de PS4. [Resultados](ZELDA-TESTS.md) y
[protocolo físico](PS4-ZELDA-CHECK.md). El paquete permanece intacto.

PKG: `dist/R2N64-v0.2.4-neon-logo.pkg`, **19.398.656 bytes**, SFO `00.24`.
SHA-256: `c22ee1b17946380f73886312f2c88fd824ed55e32ab80adb9bf9b146d19fd1ae`.

El icono del paquete utiliza el emblema aportado por el usuario. Se conserva
el original en `assets/logo.png`; el derivado `pkg/icon0.png` es RGB de
512 × 512, con proporciones intactas y márgenes laterales. El conversor
`scripts/make-icon.ps1` sustituye al generador del icono anterior.

Compilación OpenOrbis/PacBrew, firmas, hashes, SFO y extracción del PKG
aprobados; el icono extraído coincide exactamente con el nuevo PNG y su
aspecto se revisó visualmente. El fondo original y el PKG v0.2.3 conservan
sus hashes. No hay cambios en el núcleo, renderizado ni lógica del frontend;
solo el recurso del icono, su preparación, metadatos y número de versión.
No se repitieron pruebas de ROM por este cambio gráfico. Esta versión no se
ha probado en PS4 física ni corrige los fallos de shadPS4 documentados abajo.

## v0.2.3 — perfil del núcleo y diagnóstico GPU

PKG: `dist/R2N64-v0.2.3-gpu-diagnostics.pkg`, **19.398.656 bytes**, SFO `00.23`.
SHA-256: `6e000249599ed2df027ea0b41f63f0071b6605e124322753fb6c2a854620f87a`.
Compilación ELF/SELF con OpenOrbis/PacBrew y validación de firmas, hashes, SFO y
contenido extraído aprobadas. **19/19 pruebas desktop aprobadas** con el parche
final, más **2/2 pruebas del laboratorio** y el profiler aislado con ASan/UBSan.
Esta versión aún no está probada en una PS4 física.

- **Ajustes → Medir rendimiento**: desactivado por defecto; X abre el detalle y
  otro X cambia el estado. Se aplica a la próxima ROM. OPTIONS y el log muestran
  medias de RSP, RDP, salida de vídeo, audio HLE y CPU/resto desde el inicio de
  la sesión. El audio LLE queda dentro de RSP. Los tiempos son exclusivos por
  componente e incluyen esperas del hilo principal, sin sumar CPU de trabajadores.
- **Acerca de → Prueba GPU**: contexto GLES2 aislado, compilador GLSL, shaders
  originales, enlace, framebuffer 8 × 8 y verificación de sus 64 píxeles.
  Restablece el contexto SDL, atributos y VSync. Nunca se ejecuta al arrancar.
  Informe en `/data/R2N64/logs/gpu-probe.txt`; cada etapa queda en `startup.log`.
- **Correcciones de medición**: la rama PS4 usa `sceKernelGetProcessTime()` de
  OpenOrbis, evitando el identificador POSIX incompatible con libkernel. Una
  lectura fallida al comenzar una ventana no puede contabilizar la pausa previa.
  Las pruebas exactas con reloj simulado pasan ASan/UBSan. El objeto PS4 importa
  la función prevista y no `clock_gettime` para el nuevo profiler.
- **Estabilidad y UI**: prueba RDP original con sesiones off/on/off, mismos
  píxeles y contadores válidos; Mesa GLES 2.0 comprueba shaders y dos ciclos de
  restauración con texturas SDL existentes. Capturas de la pantalla GPU y pausa
  revisadas. Se corrigieron tamaños de fuente no cargados. El preparador del
  núcleo renueva su copia temporal al cambiar parches, incluidos archivos nuevos.

Se conservan CPU automática con respaldo a intérprete, audio HLE reconocido,
CXD4 SSE2, cuatro trabajadores Angrylion y los ajustes de v0.2.2. **No se activa
GLideN64 ni se atribuye mejora de velocidad a esta versión.** Un fallo al crear
el contexto de prueba no demuestra falta de capacidad de la GPU; un éxito en
esta prueba mínima tampoco demuestra compatibilidad de los shaders de GLideN64.
El PKG v0.2.2 conserva su hash original y el submódulo permanece intacto.

Mario completó otras dos sesiones Linux de 6000 VI, con perfil desactivado y
activado: imagen y PCM idénticos entre sí y al recorrido equivalente de v0.2.2.
El reparto medido en este PC fue aproximadamente 57 % RDP y 31 % RSP, con 5961
tareas HLE y sin fallos de medición. No se extrapola a PS4. Datos y límites en
[LOCAL-ROM-TESTS.md](LOCAL-ROM-TESTS.md).

Próxima comprobación física: resultado de Prueba GPU y pausa de Mario/Zelda tras
al menos treinta segundos en la misma escena, con Medir rendimiento activado.
Registrar CPU efectiva, trabajadores, tareas HLE y las dos líneas de perfil.
Comparar velocidad con la medición desactivada para excluir su coste.

### Prueba en shadPS4 0.7.0 SDL — 5 de octubre de 2026

Se probó la instalación indicada por el usuario:
`C:/Users/R2A/Downloads/shadps4-win64-sdl-0.7.0/shadPS4.exe`, revisión
`3b2c01272383e1fcd0b82c7873e1ebf1a641aada`, SHA-256
`0a2297de3fa64d9aac9c2509cad7a85d29e447d63cfe228f697dc1d377815890`.
Se extrajo el PKG v0.2.3 con PkgTool existente, incluidos SFO e icono, y se
comprobó que su SELF coincide con el ejecutable validado. La edición SDL se
inició con `-f false -g <carpeta extraída>/eboot.bin` y datos portables en una
carpeta `user` separada por experimento.

| Prueba | Resultado observado |
|---|---|
| Contenido exacto del PKG extraído | Reconoce RNTD00064, versión 00.23 y la GPU Vulkan del PC. Falla al iniciar `libc.prx`: instrucción inválida en `0x90fffc000`, salida `0xC000001D`. El log también informa errores al leer las tablas de esos módulos. |
| Copia diagnóstica sin `sce_module`, mismo eboot | Avanza sin el primer fallo, pero memoria y pthreads necesarios retornan stubs: `sceKernelMapNamedSystemFlexibleMemory`, `sceLibcMspaceCreate/Malloc/Memalign/Calloc`, `pthread_getspecific`, etc. Termina con `libc++abi: terminating` y salida `0xC0000096`. |

Ambas ejecuciones terminaron antes del XMB; no alcanzaron el diagnóstico GPU,
el núcleo N64 ni ninguna ROM. Piglet/GLES2 también aparece como importaciones
sin implementación, pero no se atribuye a eso el cierre temprano observado.
La segunda prueba es una variante de diagnóstico, no una validación del PKG
original. No se modificaron el paquete, su código ni la instalación shadPS4.
El `config.toml` predeterminado creado por `--help` se retiró tras comprobar
que seguía idéntico al generado por la prueba; la configuración previa no se
cambió. No se descargaron módulos de sistema ni ROMs.

Conclusión: **esta instalación no permite validar el arranque ni el rendimiento
de R2N64 v0.2.3**. El fallo del emulador de PS4 no confirma un fallo equivalente
en la consola. Se mantienen separadas las pruebas Linux del núcleo y la prueba
física pendiente.

Evidencia local:

- `build/shadps4-v023-070/result.json`
- `build/shadps4-v023-070/user/log/shad_log.txt`
- `build/shadps4-v023-070-no-loader/user/log/shad_log.txt`

### Prueba en shadPS4 0.19.0 y reparación de la instalación

Tras el aviso del usuario de que aparecía el logo y se cerraba, el registro de
Big Picture mostró que no existía el `eboot.bin` seleccionado. En
`C:/Users/R2A/Desktop/PKG DBUG/RNTD00064` solo estaban presentes el SFO y el
icono. Se restauraron los **35 archivos ausentes** desde la extracción validada
del PKG v0.2.3, sin sobrescribir los existentes, y se verificaron los hashes de
todo el contenido contra esa extracción. No se ha determinado qué dejó
incompleta esa carpeta.

Se repitió el arranque con la versión instalada `0.19.0`, revisión
`c7e065d1b415be16c23e260a21f1dd8bbfc4cb57`, mediante
`-f false --game <eboot.bin>` y configuración portable aislada en
`build/shadps4-v023-0190/user`. El error de archivo ausente desapareció, pero
persisten barreras de ejecución:

| Prueba | Resultado observado |
|---|---|
| Instalación completa, contenido exacto del PKG | Inicia `libSceFios2.prx` en `0x80000000` y termina allí con instrucción no decodificable y salida `0xC000001D`. |
| Copia diagnóstica sin `sce_module`, mismo eboot | Las llamadas `sceLibcMspaceCreate/Malloc/Memalign/Calloc` resuelven a stubs que devuelven cero. Termina con `std::bad_alloc` y salida `0xC0000096`. |

Ninguna prueba alcanzó el XMB, el diagnóstico GPU ni la emulación N64. Retirar
los módulos no resuelve el arranque y solo se ensayó en una copia de
diagnóstico; la instalación restaurada conserva los módulos originales.
Estos resultados corresponden a esta combinación de payload y shadPS4, sin
demostrar un fallo equivalente en PS4 física. El PKG conserva su SHA-256 y la
configuración global del emulador no se modificó.

Evidencia local en `build/shadps4-v023-0190/`: `result.json`,
`user-missing-executable.log`, `restored-files.json`, `exact-pkg-shad_log.txt`
y `no-loader-shad_log.txt`.

## v0.2.2 — recompilador y audio acelerado

Validación local posterior con la ROM de Mario aportada por el usuario: **17 sesiones automatizadas sin cierres**, incluyendo cinco perfiles de 6000 VI y controles de reapertura. Firmas de imagen iguales y PCM igual dentro del mismo modo de audio. La configuración predeterminada redujo el coste medio del núcleo de 3,951 a 2,703 ms/VI en este PC; no es una medida de PS4. No se confirmó un error del emulador que requiera otro PKG. Se añadieron el banco optativo y su control CTest, con **15/15 pruebas desktop aprobadas**. Evidencia y alcance: [LOCAL-ROM-TESTS.md](LOCAL-ROM-TESTS.md).

- CPU automática con `NEW_DYNAREC` x64. El frontend comprueba el permiso de ejecución del caché real y la inicialización del núcleo lo comprueba de nuevo; si se rechaza, utiliza el intérprete con caché. El caché PS4 usa alineación de 16 KiB y se conserva el mapa de memoria compacto.
- Audio HLE activado por defecto solo para tareas reconocidas. Los gráficos, audio desconocido y los handlers upstream incompletos MATS/EFZ continúan por CXD4 SSE2. El estado de síntesis y el contador se reinician por ROM; el contador refleja tareas realmente procesadas mediante HLE.
- Nuevas opciones **Ajustes → CPU de Nintendo 64** (Automática / Intérprete) y **Ajustes → Procesamiento de audio** (Acelerado / Original). X abre el detalle y otro X alterna. Se aplican al iniciar la siguiente ROM y no son persistentes.
- OPTIONS muestra la CPU efectivamente activa, el número de trabajadores RDP y las tareas de audio aceleradas, junto a las medidas existentes. Seleccionar Automática no constituye evidencia de que JIT esté funcionando.
- Se mantienen cuatro trabajadores Angrylion por defecto, opción de uno, sincronización Low, salida Unfiltered, VSync desactivado durante la partida y el arranque SDL/GLES2 anterior a GoldHEN.
- El build x64 requiere NASM en WSL, como dependencia adicional del ensamblado; se reutiliza OpenOrbis/PacBrew y no se sustituye la toolchain. Los scripts no instalan dependencias automáticamente.

| Comprobación de v0.2.2 | Estado |
|---|---|
| Fuentes, parches 0004/0005 y opciones del frontend | Implementados |
| Núcleo Linux y compilación/enlace estático OpenOrbis | Aprobados con los parches finales |
| Pruebas integradas del frontend y laboratorio | 14/14 desktop y 2/2 del laboratorio aprobadas |
| Puente de audio HLE aislado | Aprobado con ASan/UBSan, incluido retorno de MATS/EFZ a CXD4 |
| Aplicación PS4 final | ELF y SELF compilados con OpenOrbis/PacBrew |
| Generación, extracción y validación del PKG | Aprobada: `dist/R2N64-v0.2.2-jit-performance.pkg`, 19.333.120 bytes, SFO `00.22` |
| Arranque, disponibilidad efectiva de JIT y rendimiento en PS4 | Pendientes de prueba física |

No se atribuye todavía una mejora medida en consola a v0.2.2. Debe compararse con las escenas fotografiadas, registrando el backend activo y los ajustes usados; los resultados históricos siguientes no son resultados de esta actualización.

SHA-256 del PKG v0.2.2: `615778fac277d7cfdba22a1d52f981bdc1ff8e5770f3e82a2e89762b95e1106b`. Se comprobaron firmas/hashes del paquete, SFO y contenido extraído del ejecutable, diagnóstico, fondo, fuente e icono. Title ID `RNTD00064` y datos propios sin cambios. Los paquetes v0.2.0 y v0.2.1 conservan sus hashes originales. Las capturas de Ajustes CPU/Audio fueron inspeccionadas y están en `dist/R2N64-v0.2.2-cpu-preview.png` y `dist/R2N64-v0.2.2-audio-preview.png`.

La carga original de CPU en este PC tardó 65,259 ms con intérprete y 11,598 ms con recompilador (5,627×); las cuatro muestras completaron 149 lotes cada una y coincidieron con el cálculo entero de referencia independiente. La carga RDP original tardó 1017,910 ms con un trabajador y 310,483 ms con cuatro (3,278×), con salida equivalente. **Son mediciones desktop de cargas separadas: no se suman ni predicen la velocidad de Zelda/Mario en PS4.** Los archivos de evidencia y reproducción figuran en [CORE-LAB.md](CORE-LAB.md).

## v0.2.1 — rendimiento experimental

- Angrylion pasa de uno a cuatro trabajadores por defecto, con opción de volver a uno en Ajustes → Rendimiento. Se conserva sincronización Low, salida Unfiltered y CXD4; no se fuerza HLE incompatible con el renderer software.
- SSE2 habilitado explícitamente para CXD4 en Linux y PS4. Upstream solo lo activaba dentro de la rama de compilación dynarec, que este proyecto desactiva. Se conserva el intérprete y la memoria compacta.
- VSync se desactiva solo durante la sesión emulada, que mantiene su límite temporal según la región. El XMB lo recupera al volver. Puede haber tearing; los tiempos reales de presentación en consola quedan registrados.
- Indicador de velocidad y VI/s actualizado cada segundo, detalle en pausa y registros periódicos. Excluye pausas; 100 % significa velocidad normal, sin confundir los intervalos emulados con FPS del juego.
- Parche 0003 restablece el estado de Angrylion al cerrar para cambiar entre uno/cuatro trabajadores sin reinicializar con datos de una sesión anterior.
- Pruebas: diez del frontend y dos del laboratorio aprobadas; dos adicionales verifican una carga RDP original y píxeles idénticos con uno/cuatro trabajadores. Comparación CXD4 scalar/SSE2: 163.840 operaciones sobre 40 instrucciones, acumulador y flags idénticos.
- Mediciones en este PC: carga RDP de 40 intervalos, 1082,927 ms con un trabajador frente a 364,027 ms con cuatro (2,975×). Carga vectorial RSP, mediana de cinco muestras de 3.200.000 operaciones: 24,032 ms scalar y 7,255 ms SSE2 (3,313×). Son cargas sintéticas separadas; **no se suman ni predicen los FPS de Zelda/Mario en PS4**.
- Compilación PS4 ELF/SELF y PKG aprobada: `dist/R2N64-v0.2.1-performance.pkg`, 18.874.368 bytes, SFO `00.21`, mismo Title ID. SHA-256: `3c6e6a81854e6431085d7c24d60194869fea221e8da276bfb127346017d1ea7f`. Firmas/hashes y contenido extraído validados. El PKG v0.2.0 conserva su hash. La prueba física posterior produjo las mediciones de Zelda 44 % y Mario 32 % indicadas arriba; no se confirmó el número de trabajadores.

Para medir: ejecutar la misma escena con cuatro hilos, esperar al menos diez segundos y anotar Emulación %. Repetir con uno desde Ajustes, reiniciando la ROM. Conservar el log normal; OPTIONS muestra coste de núcleo/presentación. Las pruebas de desarrollo no utilizan ROMs comerciales.

## v0.2.0 — emulation alpha

La aplicación integra Mupen64Plus-Next con CPU en intérprete con caché, Angrylion de un hilo y CXD4; no utiliza JIT. El XMB, su fondo original y el orden de inicio SDL/GLES2/GoldHEN se conservan.

- Biblioteca: X muestra los datos de la ROM y otro X inicia la sesión.
- Acerca de → Prueba Nintendo 64: ejecuta el diagnóstico propio incluido, sin descargar contenido.
- Video software del núcleo presentado mediante SDL/GLES2, con audio estéreo convertido a 48 kHz.
- Mando N64 con Cruz=A, Cuadrado=B, L2=Z, L1/R1=L/R, cruceta, stick izquierdo, stick derecho para C y panel táctil para Start.
- OPTIONS abre la pausa con Continuar y Volver a la biblioteca. Esc y Enter cumplen las funciones de pausa y Start en desktop.
- Datos `RETRO_MEMORY_SAVE_RAM` restaurados al cargar y guardados en `/data/R2N64/saves/*.srm` al cerrar una sesión. Los nombres combinan CRC1/CRC2 y una huella FNV64 del contenido normalizado, compartida entre órdenes de bytes del mismo cartucho. No hay savestates en la interfaz.
- Parches locales del núcleo para cerrar y volver a abrir sesiones software y usar la reserva de memoria compacta del intérprete. Se aplican a copias en `build/`; el submódulo conserva el commit original.

Paquete generado y validado: `dist/R2N64-v0.2.0-emulation-alpha.pkg`, 18.808.832 bytes, SFO `00.20`, Title ID `RNTD00064`, Content ID `IV0001-RNTD00064_00-R2N64APP00000001`. SHA-256: `54542e1f01c84969cdec12d5afa6df8391592c1ddcb66059970cf1d786b1f04a`. Se comprobaron firmas y hashes del PKG, y se extrajeron y compararon SFO, ejecutable, diagnóstico, fondo, fuente e icono. El PKG v0.1.2 permanece intacto.

| Comprobación de v0.2.0 | Estado |
|---|---|
| Núcleo conectado al frontend | Implementado |
| Nueve pruebas de frontend, entrada, audio y sesiones | Aprobadas |
| Laboratorio aislado de CPU/video/audio/SI/PIF/estado | Dos pruebas aprobadas |
| Compilación y enlace del núcleo con OpenOrbis | Aprobados |
| Compilación final PS4 de la aplicación | Aprobada: ELF y SELF generados |
| Generación, extracción y validación del nuevo PKG | Aprobada |
| Arranque de v0.2.0 en consola | Confirmado por el usuario |
| Emulación N64 en PS4 física | Zelda OoT (U) V1.2 y Mario 64 abren; sonido/mando/guardados no detallados |
| Rendimiento de juegos en v0.2.0 | Muy lento según el usuario, sin medición numérica |

Las nueve pruebas aprobadas son `rom_and_storage`, `xmb_navigation_render`, `xmb_gles2_render`, `desktop_smoke`, `startup_failure_diagnostic`, `audio_output`, `n64_input_mapping`, `core_frontend_sessions` y `emulation_app_gles2`. El laboratorio añade `core_diagnostic_fixture` y `core_software_execution`. Se comprobó el cierre y la reapertura de sesiones, audio, entrada, persistencia y preservación de guardados incompatibles. El test de la app realiza dos sesiones completas del diagnóstico y regresa al frontend.

La captura real `dist/R2N64-v0.2.0-emulation-preview.png` se inspeccionó: bandas RGB, marcador de entrada e indicación de pausa/Start correctos. Estas pruebas utilizan Linux/Mesa y audio dummy; no demuestran ejecución de Piglet ni sonido físico en consola. Evidencia detallada: [CORE-LAB.md](CORE-LAB.md).

## Primera prueba física del nuevo paquete

1. Instalar el PKG validado de v0.2.2 y abrir el XMB.
2. Ejecutar **Acerca de → Prueba Nintendo 64**. Deben verse bandas RGB y oírse un tono continuo de aproximadamente 500 Hz.
3. Pulsar Cruz, Cuadrado, gatillos o cruceta, y mover los sticks. El marcador superior izquierdo debe pasar de negro a blanco y volver a negro al liberar.
4. Pulsar OPTIONS, continuar y comprobar la reanudación de sonido y entrada.
5. Volver a la biblioteca e iniciar nuevamente la prueba. Después comprobar una ROM propia y su guardado al salir de forma normal.
6. En la misma escena de Zelda/Mario de las fotografías, esperar al menos diez segundos activos y abrir OPTIONS. Anotar CPU activa, porcentaje, tiempos y contador HLE, junto con el número de hilos elegido. Comparar por separado CPU Intérprete y Audio Original, reiniciando la ROM entre cambios.

El diagnóstico ejecuta código MIPS original: escribe framebuffer, genera PCM mediante AI y consulta mando mediante SI/PIF. No contiene bootcode de Nintendo ni ROMs comerciales. No prueba listas RDP de juegos ni permite concluir compatibilidad general.

| Dato / comprobación en PS4 | Evidencia actual |
|---|---|
| Modelo | PS4 original/Slim, confirmado por el usuario |
| Firmware y GoldHEN | No informados |
| Arranque de v0.1.2 | Confirmado por el usuario |
| Arranque de v0.2.0 | Confirmado; Zelda y Mario abren muy lentos |
| Rendimiento de v0.2.1 | Zelda 44 % y Mario 32 % en fotografías; trabajadores sin confirmar |
| Arranque y rendimiento de v0.2.2 | Pendientes de prueba física |
| Navegación, contraste y transiciones XMB | Sin comprobaciones específicas reportadas |
| Crear datos, guardar logs y leer USB | Pendiente |
| Diagnóstico N64: imagen, tono y respuesta al mando | Pendiente |
| Pausa, retorno al XMB y segunda sesión | Pendiente |
| ROM propia: velocidad, audio y guardado | Pendiente |

Ante un cierre, conservar el código y momento exactos, `/data/R2N64/startup.log` o `/download0/R2N64-startup.log`, y `/data/R2N64/logs/r2n64.log` si existe. No atribuir a un juego concreto un fallo que también ocurra en el diagnóstico incluido.

## Historial de arranque

**v0.1.2 — corrección confirmada en consola.** Se cambió el orden de inicialización para preparar SDL, ventana, renderer GLES2/Piglet y primer cuadro antes de GoldHEN. SDL administra `USER_SERVICE`, sin inicialización manual duplicada; `scePadInit` queda después de SDL. Se añadió log temprano y salida controlada mediante `sceSystemServiceLoadExec` tras liberar recursos. Se aprobaron cinco pruebas desktop y se validó el PKG de 12.124.160 bytes, SFO `00.12`. SHA-256: `7aa80996fec14ad5ca99f349e4cc7a9e111a188dc2c85ced5965ce6fd0494515`. El usuario confirmó posteriormente que abre. No hay volcado que identifique la instrucción exacta del fallo anterior.

**v0.1.1 — XMB.** Se incorporaron cuatro categorías, navegación horizontal/vertical, transiciones, detalles de ROM y el fondo del usuario íntegro. Tres pruebas desktop y PKG aprobados. SHA-256: `b911f8f24e341eaa1ea404e5e38bf7714e976e380b9020348f86512bc7a1187d`. La prueba física produjo CE-34878-0 antes de mostrar el fondo o menú. Ese paquete no contenía el núcleo.

**v0.1.0 — foundation.** Se prepararon scanner, almacenamiento propio, plataforma SDL2, ELF/SELF/PKG y toolchain OpenOrbis/PacBrew reutilizada de R2FPKGI.

[PlayStation describe CE-34878-0 como un error de la aplicación](https://www.playstation.com/es-es/support/error-codes/ps4/ce-34878-0/); el código por sí solo no identifica la etapa que falló.

## Siguiente hito

Probar v0.2.2 en PS4 y medirla en las mismas escenas de Zelda y Mario. Registrar CPU realmente activa, tareas de audio HLE, número de trabajadores, velocidad y tiempo del núcleo/presentación; comprobar además sonido, mando, pausa y carga repetida. Si la consola rechaza memoria ejecutable, el modo automático debe mostrar Intérprete y continuar. Savestates desde la interfaz, carátulas, favoritos, ajustes persistentes y más mandos quedan pendientes.
