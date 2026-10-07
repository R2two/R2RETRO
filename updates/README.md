# Canales de actualización

R2RETRO desde v0.5.3 consulta `updates/experimental.txt` o `updates/stable.txt`
en la rama `main` de `R2two/R2RETRO` mediante HTTPS. No interpreta commits,
el README ni una etiqueta como una actualización instalable.

Los punteros de canal se publican **después** del PKG y de verificar su descarga.
El canal experimental anuncia **v0.5.6**: PKG público descargado y verificado
por tamaño, SHA-256, identidad y SFO con el validador del cliente. El canal
estable sigue sin publicar. Un HTTP 404 se presenta como un canal que aún
no tiene manifiesto; no bloquea los juegos.
No añadir archivos de ejemplo llamados `stable.txt` o `experimental.txt`.

`scripts/package.py` genera, tras validar y extraer el paquete:

- `dist/R2RETRO-v0.5.6-color-stability.pkg`
- Su archivo `.sha256` y `build-info-v0.5.6.json`.
- `dist/update-v0.5.6-experimental.txt`, copiado al canal tras validar la descarga.

[Release v0.5.6](https://github.com/R2two/R2RETRO/releases/tag/v0.5.6).
La publicación y validación del paquete no confirman la instalación sobre
sí misma en una PS4 física; esa prueba continúa pendiente.
Instalar v0.5.6 manualmente sin desinstalar para incorporar la corrección de
rutas del instalador. Una versión anterior con ese fallo no se corrige sólo
descargando el nuevo PKG.

En futuras solicitudes de «empaquetar y publicar»:

1. Incrementar versión CMake, versión del empaquetador y APP_VER/SFO. Las dos
   versiones deben ser posteriores a la instalada. Mantener los IDs actuales.
2. Ejecutar las pruebas y compilar/validar el PKG con la toolchain existente.
3. Revisar las notas, el commit de código correspondiente y las licencias.
   Publicar el código fuente correspondiente al binario y sus dependencias/parches.
4. Crear la Release `v<version>` y subir el PKG, SHA-256 y build-info. Usar
   pre-release mientras no haya validación suficiente para el canal estable.
5. Verificar por HTTPS el PKG publicado: tamaño, SHA-256 y SFO.
6. Copiar el manifiesto generado a `updates/experimental.txt` (o `stable.txt`
   si se generó para ese canal), revisar el diff y publicar ese archivo en `main`
   **al final**. El puntero no debe anunciar un asset inexistente o un borrador.

No reemplazar un PKG ya publicado conservando su versión. El cliente conserva
paquetes verificados por SHA-256 y rechaza versiones anteriores o iguales.
No colocar tokens, claves, ROMs o guardados dentro del repositorio o del PKG.
El compilador/usuario que publica necesita acceso a GitHub; la consola no.

La publicación remota no forma parte automática de `build.sh`/`package.py`:
construir o probar localmente no cambia lo que reciben las consolas.

El formato `R2RETRO-UPDATE-1` es texto UTF-8 con finales LF, nueve campos exactos
sin duplicados, máximo 4096 bytes. Incluye versión semántica, SFO `NN.NN`, canal,
URL del asset de la misma etiqueta/repo, SHA-256, tamaño, notas de hasta 240 bytes,
Title ID y Content ID. El transporte verifica TLS; el SHA-256 detecta corrupción,
pero no sustituye una firma independiente del propietario del repositorio.

Detalle de implementación y prueba física: [UPDATER-V053.md](../docs/UPDATER-V053.md).
