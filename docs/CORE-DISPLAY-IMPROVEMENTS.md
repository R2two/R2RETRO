# Reproducción, shaders y catálogo — v0.5.5

Actualización: el usuario autorizó empaquetar y publicar v0.5.5. Las compilaciones desktop/PS4 y las pruebas aquí descritas ya pasaron (52 grupos CTest entre ejecución completa y repetición focalizada). PKG y manifiesto validados. Sigue pendiente PS4 física; las notas siguientes conservan el estado previo a esa autorización.

7 de octubre de 2026. Cambios posteriores al PKG v0.5.4. El usuario prohíbe
compilar sin autorización: esta entrega modifica fuentes y pruebas, pero no
ejecuta CTest, compiladores C++/GLSL, aplicaciones ni empaquetadores. No hay
medidas nuevas de rendimiento ni validación en PS4.

## Controles de pausa

GB, GBC, GBA, NES y SNES incorporan **Shader** y **Escribir partida SRAM/RTC**
en L3+R3 (Esc en escritorio). El menú conserva desplazamiento vertical.

- Shader: desactivado, LCD suave, CRT suave. Se guarda por sistema, con valor
  inicial desactivado, también al leer preferencias antiguas.
- SRAM/RTC: escribe la memoria del cartucho que el núcleo expone, sin ejecutar
  cuadros ni reiniciar. No equivale a seleccionar Guardar dentro del juego.
  Conserva el escritor atómico y la protección de archivos incompatibles. Los
  errores se muestran en pausa; un cartucho sin memoria persistente no anuncia
  un guardado exitoso. SRAM y RTC son archivos independientes, no una transacción
  conjunta: si uno falla, se informa el fallo aunque el otro haya sido escrito.
- GBA incorpora **Salto GBA**: desactivado, dibujar uno de cada dos cuadros,
  dibujar uno de cada tres. La preferencia se aplica al cargar y al continuar
  después de cambiarla. No afecta a otros núcleos.

`configs/systems/{gb,gbc,gba,nes,snes}.json` mantiene versión 1 y sus campos
obligatorios. Los nuevos enteros opcionales `shader` y `gbaFrameskip` aceptan
0..2; duplicados, tipos y rangos incorrectos se rechazan. Las preferencias
anteriores no se reescriben al leerlas. Se conservan ROMs, partidas e identidad.

## Shaders GLES2

`DisplayShader` en `src/gpu_session.cpp` comparte el contexto del renderer SDL.
Compila y enlaza un programa la primera vez que se selecciona un efecto; lo
reutiliza al alternar y al cambiar de juego. Desactivado no llama a GL por cuadro.
La selección persistida puede prepararlo durante la carga del juego, nunca
durante el arranque del XMB. Sus recursos se liberan antes del renderer.

Es una máscara procedural: LCD dibuja una rejilla tenue y CRT modula líneas
horizontales. Un triángulo oscurece solamente el rectángulo del juego después
de su copia SDL y antes de la UI. No convierte la textura privada de SDL, no
lee píxeles, no crea un framebuffer adicional, no altera el core y no colorea
los overlays. Conserva estado GLES mediante la infraestructura existente.
La captura de pantalla incluye el efecto porque lee la composición final.

Se omite la máscara cuando la salida no alcanza 2 píxeles por píxel original
en ambos ejes para evitar aliasing. En software o ante rechazo GLSL se presenta
la imagen normal y se informa «no disponible». Un error de dibujo desactiva
el efecto de la sesión y deja diagnóstico para la pausa. N64 está excluido.

No es un cargador de presets RetroArch, ni incluye CRT complejo, curvatura,
LCD temporal o interpolación de movimiento. El coste de conservación de estado
GL y el aspecto final deben medirse en Piglet/PS4; no se afirma que sea gratis.

## Frameskip: GBA frente a N64

El mGBA integrado ya consume `mgba_frameskip` tanto al cargar como mediante
`GET_VARIABLE_UPDATE`. En `external/mgba/src/gba/video.c`, el contador omite
`drawScanline`/`finishFrame` mientras mantiene DMA, IRQ, CPU y avance de cuadro.
Se expone ese mecanismo, sin modificar upstream. Los límites 0..2 son
conservadores frente al rango mayor que upstream permite. Reduce fluidez y
solo puede ahorrar la porción dedicada al dibujo; no promete velocidad plena.
La pausa aclara que los FPS emulados pueden incluir imágenes repetidas.

En el Mupen64Plus-Next integrado, **Frame Duplication** se describe expresamente
como distinto de frameskip (`libretro/libretro_core_options.h`). No se encontró
un control equivalente listo para usar en la ruta GLideN64 que ejecutamos.
Saltar únicamente SDL/presentación no evita el trabajo R4300/RSP/RDP previo.
Las capturas físicas anteriores situaban el coste mayor dentro del núcleo;
no son un perfil nuevo ni separan todo el tiempo GPU.

No se activa un falso frameskip N64 ni se omiten tareas RDP de forma indiscriminada.
Una implementación futura necesita elegir operaciones gráficas descartables,
preservar accesos del juego a framebuffer/RDRAM, sincronización y audio, y
contrastar imagen/compatibilidad con perfil físico comparable. El JIT y HLE
actuales permanecen intactos.

## Cloudflare y Libretro

Comprobado por lectura del código: `src/library_metadata.cpp` usa `httpGet`
para RDB y para las tres imágenes Boxart/Snap/Title. Todas llegan al mismo
transporte de `src/http.cpp`, ya fijado a Cloudflare DoH en el cambio anterior:
`https://cloudflare-dns.com/dns-query` con bootstrap `1.1.1.1,1.0.0.1`.
No hay selector de proveedor ni caída al DNS del sistema en producción. TLS,
certificado de host, CA, límites, cancelación y caché offline siguen activos.
La excepción configurable está aislada en `R2N64_HTTP_TESTING` para fixtures.
No se cambia el DNS global de PS4. La confirmación física del usuario sobre
Cloudflare no sustituye una prueba nueva de carátulas con esta versión.

## Validación disponible y pendiente

Realizado: revisión de fuentes y `git diff --check`, sin errores de espacios.
El PKG v0.5.4 conserva 65.994.752 bytes; no se ha generado una nueva versión.

Preparadas, **sin ejecutar**, ampliaciones de:

- `handheld_settings_tests`: lectura antigua, campos nuevos, rangos/tipos,
  persistencia separada y valores iniciales desactivados.
- `handheld_tests`: escritura SRAM antes del unload, rechazo fuera de sesión,
  frameskip GBA e invariancia CPU/input. La comparación PCM parte de sesiones
  nuevas (un estado no garantiza restaurar el historial del resampler anfitrión).
- `console_tests`: guardado manual NES/NROM/MMC3/SNES y rechazo de frameskip GBA.
- `gpu_session_tests`: fallback software, on/off, efecto confinado al juego,
  conservación de estado SDL, ausencia de readback y UI después del efecto.

Tras autorización: compilar, ejecutar estas pruebas y regresión general,
contrastar shaders/marcos/pausa/capturas en GLES2, probar GBA con frameskip
0/1/2 en la misma escena y finalmente validar en PS4 física. No distribuir
un PKG de estos cambios antes de esa validación de compilación y escritorio.
