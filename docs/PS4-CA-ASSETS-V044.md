# R2RETRO v0.4.4 — certificados CA y recursos después de GoldHEN

## Qué se encontró

El usuario informó `No se encuentra un archivo de certificados CA valido`
al usar la app en el contexto de sus pruebas en PS4 física. Ese mensaje de
v0.4.3 se genera en `httpGet` al comprobar `std::ifstream(...).peek()`; aparece
antes de inicializar la red, curl o una conexión TLS. No permite distinguir
archivo ausente, apertura fallida, lectura fallida ni archivo vacío.

Se extrajo el PKG v0.4.3 y se comprobó que
`uroot/assets/certs/cacert.pem` existe y coincide con el recurso fuente:
188.900 bytes, SHA-256
`a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`.
No se encontró una dependencia `*at` defectuosa en la cadena efectiva
`ifstream → fopen/fread` del ejecutable anterior.

La ruta anterior era siempre `/app0/assets/certs/cacert.pem`. Fuentes y fondo
se cargan antes de `sys_sdk_jailbreak`, pero el CA, los marcos y la ROM original
de diagnóstico se abren después. El cambio de raíz del proceso es una causa
plausible de que la primera ruta deje de servir; todavía no hay un registro
de esta transición tomado en la consola del usuario. La corrección elimina
esa suposición y añade diagnóstico para contrastarla.

El montaje absoluto bajo `/mnt/sandbox/<título>_<slot>/app0` tiene precedente
en el [código de Apollo PS4](https://github.com/bucanero/apollo-ps4/blob/main/include/saves.h).
R2RETRO obtiene el título por la API de la aplicación y enumera los slots;
no fija el Title ID en el resolver ni supone el sufijo `_000`.
`sceKernelGetFsSandboxRandomWord` se documenta para rutas de módulos del
sistema en [OpenOrbis](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/include/orbis/libkernel.h);
no se trata ese valor como identidad del montaje de recursos.

Auditoría del transporte y la toolchain: `build/v044-ca-audit.txt`.
La extracción anterior está aislada en `build/v044-audit/pkg-v043`.

## Cambios

1. Después de iniciar SDL y presentar el primer cuadro, la plataforma abre el
   directorio de recursos y conserva su descriptor antes de GoldHEN.
2. Después del cambio de acceso se comprueba si la ruta original sigue
   apuntando al mismo directorio. Si no, se buscan únicamente los slots del
   título actual bajo `/mnt/sandbox`, con un máximo de 1.024 entradas.
3. Sólo se acepta un directorio con la misma identidad `st_dev/st_ino` que el
   descriptor conservado. Se comprueban todos los componentes de las rutas,
   rechazando enlaces, traversal, NUL, tipos incorrectos e identidad ausente.
   El descriptor se cierra al terminar y nunca se cambia el cwd del proceso.
4. La ruta comprobada alimenta el CA, el diagnóstico incluido y los marcos
   cargados al abrir un juego. `Video::setAssetPath` conserva la interfaz ya
   cargada e invalida sólo la caché de marcos si cambia la ubicación.
5. Si no se logra comprobar la ubicación, permanecen disponibles la interfaz
   y las ROMs del usuario; las funciones que necesitan esos recursos dan un
   error. No se elige un CA externo ni otra carpeta sin comprobar identidad.
   `startup.log` registra `Asset path ready` o `Asset path unavailable`.
6. La comprobación previa de CA usa `open/fstat/read` POSIX, sin bloquear ante
   FIFO y sin seguir el enlace final. Requiere un archivo regular no vacío de
   hasta 2 MiB y un primer byte legible. El error identifica etapa y errno,
   sin mostrar la ruta local. Curl vuelve a abrir el archivo indicado y
   valida el PEM y la conexión.

Se conservan `CURLOPT_SSL_VERIFYPEER=1`, `CURLOPT_SSL_VERIFYHOST=2`, TLS 1.2
como mínimo, CAINFO explícito y redirecciones sólo HTTPS. No se añade una
opción de omitir TLS, descarga automática de CA ni certificados en el USB.
El bundle distribuido permanece intacto.

La revisión de curl 7.80/mbedTLS del toolchain confirmó que `CAINFO_BLOB` no
está soportado por ese backend, aunque el enum exista en los headers. No se
usa esa opción ni un callback específico de otra biblioteca TLS.

Se conservan el arranque SDL → primer cuadro → GoldHEN → mando, la identidad
de instalación, `/data/R2N64`, las carpetas USB, partidas, formatos y núcleos.
Las operaciones por ruta conservan la limitación frente a sustituciones
concurrentes documentada en `include/file_ops.h`.

## Validación

**45/45 grupos CTest aprobados**, en 157,06 segundos. Incluyen regresión de
los seis sistemas, almacenamiento, biblioteca, arranque y render software/GLES2.
Registro: `build/v044-ctest.log`; compilación desktop:
`build/v044-desktop-build.log`.

Pruebas del resolver: ruta original aún válida, recurso trasladado al slot
del título, otros títulos/slots, identidad distinta, descriptor prestado,
rutas inseguras, symlinks, búsqueda acotada y errores de acceso. Se ejecutan
con las operaciones nativas y con el backend PS4 de rutas absolutas mientras
las funciones `*at` incompatibles están interceptadas.
Ambos modos también pasaron AddressSanitizer/UndefinedBehaviorSanitizer en
binarios aislados bajo `build/v044-audit/`.

El renderer comprueba que los marcos tardíos usan la nueva ruta, que el
cambio invalida su caché, que la ruta sin cambios conserva la textura y que
la ausencia de recursos no intenta cargar desde una ruta inventada ni
descarta el fondo ya cargado. Se cubren software y GLES2.

**44 casos HTTPS aprobados**, también con AddressSanitizer y
UndefinedBehaviorSanitizer. Cubren CA ausente, ruta vacía/NUL, archivo vacío,
directorio, symlink, FIFO sin escritor, tamaños límite, PEM malformado,
recuperación, certificado no confiable, hostname erróneo, descarga, límites,
redirecciones, cancelación y timeout. Evidencia: `build/http-v044/test-asan.log`.
Los certificados privados del servidor de pruebas se crean en temporales y
no se incorporan al paquete.

También pasó una descarga HTTPS real del índice de Libretro Thumbnails
(27.685 bytes) con el código actualizado y el CA extraído del PKG v0.4.3,
idéntico al distribuido en v0.4.4. Registro: `build/v044-https-smoke.log`.
Esta comprobación se ejecutó en Linux y no verifica la conectividad de PS4.

Estas pruebas Linux y la compilación cruzada no sustituyen la comprobación
en PS4 física. No se atribuye una ganancia de rendimiento a este cambio.

## Paquete

**`dist/R2RETRO-v0.4.4-ps4-ca-assets-fix.pkg`**, **63.963.136 bytes**,
SFO `00.44`. SHA-256:
`724a8544117e788b3542a47a05bc967919579d739ca644119fe7e52f624d4140`.

Compilado con OpenOrbis/PacBrew y validado por PkgTool. La extracción verifica
SFO, ejecutable, CA, fondo, icono, cuatro marcos y licencias contra los recursos
fuente. Metadatos: `dist/build-info-v0.4.4.json`; registro de compilación y
paquete: `build/v044-package-build.log`.

La auditoría del ELF confirma los imports `lstat/open/fstat/read` y
`sceKernelGetAppInfo`, presentes en la biblioteca del sistema de la toolchain,
sin reintroducir `openat/_openat/mkdirat/renameat/unlinkat/fstatat`. Strings de
versión y resolución de recursos presentes. Evidencia:
`build/v044-elf-audit.txt`; SHA-256 del ELF:
`99f1eafac38ac74461b17c8552dddac154979ad50f49dfa69e2928e10d2ccd47`.

Se conserva el PKG v0.4.3 con SHA-256
`1c66f9739b638dc768d940a52093a8d926be2989999fb8bb2bf7ee0b95ea38f0`.
No se ejecutaron ni empaquetaron ROMs comerciales en esta actualización.

## Prueba en PS4

Instalar v0.4.4, abrir R2RETRO, seleccionar una ROM y pulsar **OPTIONS** en
la biblioteca para descargar la ficha. Comprobar que la carátula aparece y
que vuelve a mostrarse al reabrir sin conexión. También abrir un juego con
marco y la prueba N64 incluida para comprobar los otros recursos tardíos.

Si vuelve a fallar, conservar el mensaje completo y
`/data/R2N64/startup.log` junto con `/data/R2N64/logs/r2n64.log`.
Un fallo local CA ahora se distingue de un rechazo TLS, DNS o HTTP.
