# Mejoras posteriores a v0.5.5 — 7 de octubre de 2026

> Actualización v0.5.6: cambios compilados y pruebas completadas con autorización.
> [Resultados, paquete y límites](releases/v0.5.6.md). Las menciones a fuentes
> sin compilar que siguen registran el estado anterior a esta autorización.

Estado: cambios fuente, sin autorización nueva para compilar. No se generó un
PKG, no se ejecutaron binarios de pruebas ni se publicó otra versión. Los 52
grupos aprobados para v0.5.5 no validan estas modificaciones.

Comprobaciones realizadas sin compilar: `git diff --check` sin errores, sintaxis
Bash de los dos scripts modificados, análisis sintáctico de sus bloques Python
y aplicación en memoria de los dos reemplazos literales mGBA (una coincidencia
por bloque). No se ejecutó el script de construcción ni se modificó su copia
de núcleo preparada. Esto no valida compilación, enlace o comportamiento C++.

## Instalación local

El usuario informa que la descarga termina en PS4, pero al elegir Instalar y
cerrar aparece `Instalador PS4: Ruta local (0xFFFFFFFF). PKG conservado; no se
desinstaló R2RETRO.` Firmware declarado 13.52 y GoldHEN posiblemente 2.4b18.6;
versión exacta de la aplicación instalada durante el intento no confirmada.

Ese mensaje procede del preflight propio, antes de invocar AppInstUtil/BGFT.
El código anterior agrupaba ruta inválida, errores lstat, archivo no regular,
tamaño y discrepancia de dispositivo/inode bajo el mismo -1. Exigía además que
`/data/...` y `/user/data/...` expusieran idénticos dispositivo e inode. Sin log
físico no es posible señalar cuál de esas condiciones falló.

El adaptador ahora abre la ruta original sin seguir el enlace final, comprueba
archivo regular, tamaño, SHA-256 completo, identidad y SFO. Si está disponible
el alias `/user/data/...`, lo verifica también íntegramente y lo entrega a las
APIs del sistema; no exige igualdad de inode entre vistas de montaje. Un alias
alterado, de tamaño incorrecto, enlace o error de I/O aborta la operación. Si
el alias no existe, su padre no es directorio o el proceso no puede acceder a
él, se entrega la ruta original verificada. La aceptación de esa ruta por
BGFT requiere prueba física; no se presupone que tenga la misma visibilidad.

Los errores de archivos indican operación, ruta y errno cuando existe; los de
las APIs conservan su código hexadecimal y etapa. Los descriptores permanecen
abiertos durante el envío, pero BGFT recibe una ruta: esto no elimina la carrera
si otro proceso reemplaza el archivo entre verificación y lectura del sistema.
La verificación adicional se ejecuta en el trabajador, no en el hilo de UI.
Esta comprobación interna del adaptador todavía no tiene cancelación cooperativa;
la verificación previa del servicio sí la admite. Los archivos están limitados
a 512 MiB por el validador compartido.

No se llama a desinstalar ni se elimina el PKG. Se mantiene FORCE_UPDATE y el
slot existente. Registrar/iniciar la tarea correctamente sólo confirma el envío,
no que el reemplazo haya finalizado. Los guardados y la configuración mantienen
sus rutas e identidad. El uso público de `/user/data` y BGFT por almacenamiento
está descrito por [flatz](https://flatz.github.io/); no demuestra funcionamiento
del reemplazo de una aplicación activa en este firmware.

Pruebas fuente preparadas en `tests/update_installer_tests.cpp`: alias ausente,
sin acceso, diferente inode y contenido admitido, contenido alterado, tamaño
distinto, enlace, error de I/O, ruta relativa/NUL, directorio, original alterado
y archivo ausente. Conservan escenarios de identidad, slot, usuario, conflicto,
registro, inicio y limpieza de tarea. La verificación de contenido de este
adaptador es un doble explícito: SHA/SFO reales pertenecen a `update_tests.cpp`.
No existe stub de desinstalación para que una dependencia accidental falle al
enlazar. Todo ello está pendiente de ejecutar con autorización.

## Presentación y núcleos

Cada entrega válida de vídeo no NULL recibe un serial de sesión; NULL conserva
la imagen anterior. El serial continúa al reiniciar o cargar estado, mientras
que el cuadro se invalida. La textura del juego se inicializa al abrir una sesión
y sólo vuelve a transferirse cuando cambia la entrega, resolución o textura.
Un fallo de subida no marca los píxeles como válidos. La composición, marcos,
filtros y menús siguen dibujándose sobre los píxeles reutilizados.

mGBA 0.10.5 omite dibujar con su contador nativo de frameskip, pero su adaptador
libretro reenvía el buffer igualmente. El parche de la copia aislada emite NULL
cuando ese contador indica omisión. No salta llamadas de CPU/APU ni modifica
memoria del juego. Las revisiones upstream y submódulos se conservan.

La pausa portátil/consolas clásicas distingue ritmo emulado de entregas de
imagen por segundo. Se cuentan pasos que entregan imagen, no diferencias de
píxeles ni imágenes finalmente presentadas durante avance rápido. Las métricas
N64 de VI/s y porcentaje conservan su significado. No se cambia su backend GPU
ni se promete resolver la lentitud reportada.

Regresiones fuente preparadas: subida inicial y reutilización, filtro sin nueva
subida, fallo y reintento, resolución/sesión nueva, seriales tras reinicio, ritmo
de entregas GBA con saltos 0/1/2 y comparación de RAM/PCM. Pendientes ejecución
desktop y prueba física de marcos, shaders, guardados y estabilidad.

## Próxima validación, sólo con autorización de compilación

1. Reconstruir copias aisladas de núcleos y frontend; ejecutar pruebas unitarias,
   vídeo software/GLES2 y `scripts/check-update-installer.sh` con sanitizadores.
2. Contrastar GBA con/sin salto y cambios en vivo: vídeo, PCM, RAM, reinicio,
   carga de estado y primera imagen tras cada transición.
3. Compilar PS4, validar imports/PKG y mantener intactos los artefactos v0.5.5.
4. Instalar manualmente la primera versión corregida: el actualizador instalado
   no recibe cambios fuente hasta sustituir su PKG. Después probar reemplazo
   de una versión por otra en consola, conservando guardados y configuración.
