# Actualizador experimental — v0.5.3

Implementado el 7 de octubre de 2026. El usuario informa PS4 original/Slim,
firmware **13.52** y cree usar **GoldHEN v2.4b18.6** (versión pendiente de confirmar).
Eligió instalar la primera versión **por USB**. No hay acceso remoto a su PS4
ni resultado de instalación física durante esta implementación.

## Interfaz y estados

- Ajustes → Actualizaciones: Buscar ahora, canal Experimental/Estable,
  búsqueda al iniciar, Descargar, Instalar y cerrar, Volver.
- Canal experimental y búsqueda al inicio predeterminados en esta preview.
  Preferencias de cuatro bytes versionadas en `<data>/configs/updates.conf`,
  con escritura temporal y rename. No cambia los ajustes de sistemas/juegos.
- Una consulta al iniciar el menú, desactivada en pruebas smoke. Sin sondeo
  durante juegos, sin descargas/instalación automáticas.
- Consulta, descarga, SHA y solicitud de instalación en trabajador dedicado;
  el renderer SDL permanece en el hilo principal. Al iniciar un juego se cancela
  la consulta y se espera su finalización desde el bucle de eventos.
- Círculo cancela consulta/descarga. La solicitud de instalación no se cancela
  desde la UI una vez iniciada. Instalar requiere un segundo X de confirmación.
- Un fallo deja la app abierta, muestra etapa/código y conserva el PKG verificado.
  Aceptación BGFT se registra como **solicitud**, nunca como instalación completada.
- Tras aceptación el frontend termina normalmente. El servicio de sistema queda
  a cargo del paquete. No se promete reinicio automático ni reemplazo en caliente.

## Transporte y validación

Fuente de canales: `https://raw.githubusercontent.com/R2two/R2RETRO/main/updates/`.
Formato y procedimiento de publicación en [updates/README.md](../updates/README.md).
No se publicó remotamente el PKG ni el manifiesto durante este cambio.

Se reutiliza curl/mbedTLS y el CA existente con verificación peer/host, TLS1.2
mínimo y hasta tres redirecciones HTTPS. Manifiesto de 4 KiB con campos exactos,
IDs fijos y URL de un asset `.pkg` del repositorio/etiqueta anunciados.
La descarga tiene límite de 512 MiB, conexión de 12 s, total de 30 min y límite
de lentitud de 10 s. Se escribe en disco desde el callback, sin guardar el PKG
completo en RAM. En errores se elimina únicamente el temporal propio.

El verificador usa bloques de 64 KiB y comprueba:

1. Archivo regular, un enlace, tamaño exacto y SHA-256 completo.
2. Magic PKG, Content ID, tipo de aplicación completa (no parche/DLC) y tamaño
   de cabecera. Tabla limitada a 4096 entradas de 32 bytes.
3. Un único `PARAM_SFO` de hasta 64 KiB, con offsets/capacidades/cadenas acotadas,
   máximo 128 campos y claves sin duplicados.
4. TITLE_ID, CONTENT_ID, APP_VER y categoría `gd` o `gde`. Los PKG actuales
   de OpenOrbis utilizan `gde`, contrastado con v0.5.2 real.
5. Versión semántica **y** APP_VER superiores a las del ejecutable instalado.

Destino: `<data>/updates/R2RETRO-<sha256>.pkg`; temporal `.part`. Un paquete ya
descargado se vuelve a verificar al seleccionar Descargar y puede reutilizarse.
Se verifica otra vez antes de entregarlo al instalador. No se borran paquetes
anteriores automáticamente ni se conserva una descarga parcial para reanudarla.
El trabajador usa `fileops` para las operaciones de almacenamiento compatibles
con PS4. Se mantiene la limitación de carreras por rutas del backend PS4;
no modificar concurrentemente los directorios privados durante una actualización.

El canal HTTPS es la autoridad de publicación. El hash permite detectar errores
de transferencia/almacenamiento; no hay firma de manifiesto separada de GitHub.

## Instalador PS4

`src/update_installer.cpp` carga AppInstUtil/BGFT cuando el usuario confirma.
No cambia el arranque SDL/Piglet → GoldHEN. Comprueba identidad de archivo entre
la ruta local y `/user/data` cuando corresponde, Title ID mediante AppInstUtil,
actualización previa activa, slot instalado y usuario activo.

Registra `sceBgftServiceIntDownloadRegisterTaskByStorageEx` con FORCE_UPDATE y
arranca **solo esa tarea**. Conserva el ABI de tamaño de 64 bits con static_assert
sobre tamaño/offsets de los headers OpenOrbis existentes. Si el inicio falla,
intenta retirar solo la tarea recién registrada e informa si necesita revisar
Descargas. No llama a ninguna función de desinstalación ni modifica `/app0`.

La consulta de identidad/slot, el registro con la aplicación todavía abierta y
la persistencia de la instalación después de salir necesitan validación física.
Si firmware/GoldHEN los rechazan, no se fuerza el reemplazo: se conserva el
archivo y se registra la etapa y el código hexadecimal. No se implementó un
helper residente ni un servidor HTTP temporal.

Referencias leídas como documentación de APIs (no se copió la ruta de
desinstalar/reintentar de otros clientes):

- Cliente local R2FPKGI: `client/source/installer.cpp` (descarga BGFT desde URL).
- [PS4 ezRemote Client, instalador local](https://github.com/cy33hc/ps4-ezremote-client/blob/master/source/installer.cpp).
- Headers de la toolchain instalada `orbis/AppInstUtil.h`, `orbis/Bgft.h` y
  `orbis/_types/bgft.h`.

## Validación local

- **51/51 CTest**, `build/updater-full-tests.log` (63,99 s).
- HTTPS local con CA temporal: descarga a archivo, redirección, longitud mayor/
  menor, transferencia incompleta/chunked, cancelación, CA no confiable, rechazo
  de HTTP y destino no escribible; regresiones previas del catálogo conservadas.
- ASan/UBSan de manifiestos, SHA (vacío, abc y millón de a), PKG/SFO corruptos,
  downgrade, preferencias, trabajador y limpieza de descarga fallida, tanto con
  fileops nativo como con backend de rutas PS4 y stubs *at.
- Adaptador compilado contra headers Orbis y servicios simulados: identidad,
  usuario/slot, conflictos, error de inicio/limpieza y aceptación. No se enlaza
  un stub de desinstalación. No son llamadas a una PS4 real.
- `bash scripts/check-update-installer.sh`; resultados `build/updater-sanitizers.log`.
- Verificador nuevo acepta el PKG v0.5.2 real y el v0.5.3 generado, con sus hashes
  y APP_VER correspondientes. Manifiesto generado aceptado por el parser C++.
- HTTPS real con el CA del paquete: README de raw.githubusercontent.com leído
  (14.824 bytes) y PKG v0.5.1 descargado por streaming desde GitHub Releases
  (64.159.744 bytes, con redirección). SHA-256 coincide con la versión publicada
  `4889669f6e556fc8b229111be2f5a1c4f650ecbbdb66025bdfacf990f8bb4483` y pasa
  el verificador nuevo de identidad/SFO00.51. Descarga de referencia en
  `build/update-tests/github-v051.pkg`, no instalada. `build/updater-github-stream.log`.
- UI software/GLES2 renderizada e inspeccionada. Captura de confirmación usa
  **v0.5.4 sintética como ejemplo**, no es una versión publicada ni un PKG real.
- Ejecutable y SELF PS4 compilados. PKG v0.5.3 extraído y validado por PkgTool,
  con recursos comparados. Registros `build/updater-ps4-build.log` y
  `build/updater-package.log`. v0.5.2 intacta.

Paquete: `dist/R2RETRO-v0.5.3-updater.pkg`, **65.994.752 bytes**, APP_VER **00.53**.
SHA-256: `a896d970d3226a2b39f680b8bbe725b9bb6f601b4de2f7e64a94426665e51be6`.

También incluye los cambios locales acumulados: biblioteca por consolas,
perfiles N64 automáticos experimentales, nuevo fondo y logos de consola.
No hay nueva medición de rendimiento físico N64 ni cambios de revisión de cores.

## Prueba física pendiente

1. Instalar v0.5.3 por USB sobre R2RETRO, sin desinstalar la anterior. Abrirla
   y verificar biblioteca, acceso a una partida existente y menú Actualizaciones.
2. Confirmar GoldHEN exacto. Si no hay canal publicado aún, el mensaje
   «Este canal todavía no tiene un manifiesto publicado» es el resultado esperado.
3. Cuando se publique una **versión posterior real** junto a su manifiesto,
   abrir Buscar ahora. Debe anunciar versión/tamaño correctos.
4. Descargar, cancelar/reintentar si se desea, y comprobar que llega a «PKG
   verificado». Confirmar Instalar y cerrar.
5. Revisar Notificaciones/Descargas: aceptación de BGFT no basta. Esperar la
   instalación completa y reabrir. Comparar la versión mostrada y verificar las
   mismas ROMs USB, preferencias y partida guardada.
6. Ante fallo, conservar texto/código y `<data>/logs`/`startup.log`. No desinstalar
   para evitar el conflicto. El PKG verificado permanece en `<data>/updates`.

No se puede probar este recorrido con una versión igual/inferior: el bloqueo es
intencional. La primera instalación por USB valida el arranque del actualizador,
no su capacidad de actualizarse. No hay confirmación física todavía.
