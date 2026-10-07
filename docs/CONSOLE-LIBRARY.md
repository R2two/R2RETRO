# Biblioteca por consola — cambios sin empaquetar

La raíz Biblioteca contiene seis carpetas: Nintendo 64, NES, Super Nintendo,
Game Boy, Game Boy Color y Game Boy Advance. Entrar con X muestra solamente
juegos identificados como ese sistema, aunque procedan de raíces USB/internas
diferentes o tengan el mismo título. Ya no existe una vista inicial «Todos».

- X en una carpeta entra; en un juego abre sus detalles; otro X inicia el juego.
- Círculo cierra detalles, vuelve a la consola y luego permite salir con la
  confirmación anterior. Se recuerda el cursor de juegos de cada consola.
- L1/R1 selecciona carpetas en la raíz y cambia de consola dentro de ellas.
- Las carpetas vacías ofrecen buscar juegos o consultar las ubicaciones.
- Cada carpeta muestra su número de juegos. Al reescanear se recalculan los
  contadores y se acota la selección si desaparecieron archivos.
- Fichas/carátulas se solicitan únicamente sobre juegos; el menú de carpetas
  no inicia hashes ni descargas de fichas. La vista de almacenamiento cuenta
  todos los juegos de su raíz, independientemente de la consola abierta.

`core/system_catalog.h` define el orden de navegación separado de los IDs
persistidos. `ConsoleLibrary` reúne filtrado y contadores, y el test XMB exige
que todos los sistemas del registro de cores tengan carpeta. Añadir un sistema
al catálogo hace aparecer su carpeta sin mantener otra lista en UI o App.
No cambia rutas, archivos, IDs, guardados, core seleccionado ni preferencias.

Pruebas: biblioteca mezclada, títulos iguales, fuentes distintas, todos los
sistemas, contenido desconocido, carpetas vacías, regreso, persistencia del cursor,
desaparición de juegos y metadatos en raíz. Se generan capturas de raíz y GBA
vacía con renderer SDL software y GLES2 en `build/desktop/previews/`.

La validación desktop no confirma la presentación física en PS4. No se genera
un nuevo PKG para estos cambios y se conserva el número de versión de trabajo.
Dolphin permanece pendiente; véase `DOLPHIN-PS4-INTEGRATION.md`.

Resultado: 49/49 CTest aprobados (124,83 s) en
`build/console-folders-ctest.log`; ejecutable PS4 compilado mediante el target
`r2n64`, sin invocar `r2n64_self` ni empaquetador. Log de enlace:
`build/console-folders-ps4-build.log`. Las pruebas XMB se repiten tras el ajuste
final de etiquetas singular/plural en `build/console-folders-final-xmb.log`.
