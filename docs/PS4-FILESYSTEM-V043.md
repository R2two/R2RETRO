# R2RETRO v0.4.3 — acceso a ROMs y archivos en PS4

## Incidente y alcance

El usuario comunicó `No se pudo leer /data/R2N64/roms/gb: Invalid argument`
en una PS4 física con las ROMs en USB. El escáner consulta tanto la carpeta
interna como `/mnt/usb0/R2N64/roms` y `/mnt/usb1/R2N64/roms`; la ruta interna
del aviso no significa que el usuario deba mover allí sus juegos.

La apertura relativa de las subcarpetas con `openat` es una hipótesis para ese
`EINVAL`, no una causa confirmada en hardware: el aviso anterior no distinguía
apertura, `fdopendir` ni `readdir`. También se encontró un defecto en el
diagnóstico: al fallar `fdopendir`, cerrar su descriptor podía sustituir el
`errno` original antes de construir el mensaje.

La auditoría de la toolchain instalada (`ps4-openorbis-musl 1.5-2`) confirma
que `mkdirat`, `renameat`, `unlinkat` y `fstatat` son stubs que devuelven
`ENOSYS`. El ELF anterior enlazaba los tres primeros para ajustes y caché.
`openat` tiene un wrapper que reenvía a `_openat`; no se ha demostrado su
comportamiento con rutas relativas en esta consola. Evidencia de símbolos y
desensamblado en `build/v043-libc-audit.txt`.

Hay un matiz importante: aunque musl contiene un wrapper `lstat → fstatat`,
el orden de enlace real (`-lkernel` antes de `-lc`) importa `lstat` de
`libkernel`. Ese wrapper defectuoso no formaba parte del ejecutable anterior.
No se sustituye `lstat` ni se relajan las comprobaciones de enlaces.

## Corrección

- Capa `fileops` compartida. Desktop conserva las operaciones nativas `*at`.
  PS4 usa `open`, `mkdir`, `unlink` y `rename` con rutas absolutas comprobadas,
  sin cambiar el directorio de trabajo global.
- Las aperturas verifican los componentes del padre, su identidad frente al
  descriptor, el tipo del hijo y su identidad antes y después de abrirlo.
  Conservan `O_NOFOLLOW`, `O_NONBLOCK`, `O_EXCL` y los demás flags del llamador.
  Se rechazan enlaces, componentes de escape y archivos especiales.
- Los ajustes por sistema y la caché mantienen archivos temporales exclusivos
  y una sola operación de renombrado atómico. No se añade un fallback de
  copiar/borrar ni se cambian sus formatos o ubicaciones.
- El escáner identifica la operación que falla, conserva el código original
  y continúa con las otras carpetas/raíces. Por ejemplo:
  `/mnt/usb0/R2N64/roms/gb [open sistema]: Invalid argument (errno 22)`.
  Los errores al comprobar un directorio con `fstat` ya no se omiten.

Las verificaciones por ruta detectan sustituciones, pero no ofrecen la misma
garantía atómica de los descriptores nativos frente a cambios concurrentes de
directorios. La limitación está documentada en `include/file_ops.h`; no se
deben renombrar o sustituir esas carpetas mientras la app las usa.

Se mantienen el nombre público R2RETRO, Title ID `RNTD00064`, Content ID
`IV0001-RNTD00064_00-R2N64APP00000001`, `/data/R2N64`, `R2N64_DATA`, las carpetas
USB `R2N64/roms`, los identificadores de juegos y las revisiones de núcleos.
No se incluyen ROMs comerciales ni se modifican los juegos del usuario.

## Pruebas reproducibles

```sh
cmake -S . -B build/desktop -DCMAKE_BUILD_TYPE=Debug
cmake --build build/desktop --parallel 8
ctest --test-dir build/desktop --output-on-failure
bash scripts/build.sh ps4
```

Cinco grupos nuevos amplían la suite:

| Prueba | Comprobación |
| --- | --- |
| `ps4_file_operations_contract` | Backend de rutas con los cinco `*at` interceptados; EINVAL/ENOSYS, espacios/UTF-8, lectura/escritura, mkdir, rename atómico, unlink, enlaces, FIFO, padres incorrectos y descriptores obsoletos. |
| `ps4_rom_scan_paths` | Scanner y almacenamiento con el backend de rutas y cabeceras sintéticas; los `*at` incompatibles no pueden ejecutarse correctamente. |
| `ps4_settings_paths` | Persistencia y validaciones de ajustes con ese backend. |
| `ps4_metadata_paths` | Caché de fichas/imágenes con ese backend y fallos de `*at` simulados. |
| `rom_scan_failure_recovery` | EINVAL al abrir GB, EMFILE en fdopendir, EIO en fstat/readdir, cierre que altera errno, continuidad y reescaneo. |

**43/43 grupos CTest aprobados**, en 157,94 segundos. Registro:
`build/v043-ctest.log`. La suite incluye regresión de los seis sistemas,
persistencia, red/caché, arranque y vídeo software/GLES2. Compilación desktop
en `build/v043-desktop-build.log`.

El contrato de archivos y las pruebas focalizadas del escáner también se
ejecutaron con AddressSanitizer/UndefinedBehaviorSanitizer. Estas pruebas
simulan limitaciones concretas en Linux; no emulan el filesystem de PS4 ni
prueban el montaje real del USB.

## Paquete y enlace PS4

**`dist/R2RETRO-v0.4.3-ps4-filesystem-fix.pkg`**, **63.963.136 bytes**,
SFO `00.43`. SHA-256:
`1c66f9739b638dc768d940a52093a8d926be2989999fb8bb2bf7ee0b95ea38f0`.

Compilado con OpenOrbis/PacBrew, validado por PkgTool y extraído para comparar
SFO, ejecutable, icono, fondo, marcos y licencias con sus fuentes. Registro:
`build/v043-package-build.log`; metadatos: `dist/build-info-v0.4.3.json`.

La tabla completa de símbolos del nuevo ELF ya no contiene `openat`,
`openat64`, `_openat`, `mkdirat`, `renameat`, `unlinkat`, `fstatat` ni
`fstatat64`. Las operaciones absolutas, incluidas `lstat` y `fstat`, quedan
importadas de la biblioteca del sistema. Strings de arranque confirman
R2RETRO 0.4.3. Evidencia: `build/v043-elf-audit.txt`; SHA-256 del ELF:
`f059a6d3e1a5cce3faa694415adbf1e6c39ba6fb0fd0df580181440b028d13a0`.

El PKG v0.4.2 conserva su SHA-256
`ae1e02977cb368cada13344f31def7d79f66dddd23d79c7907dda3f39feccd11`.
No se ejecutaron juegos comerciales durante esta actualización.

## Comprobación en consola

1. Instalar el PKG v0.4.3 y abrir R2RETRO con el USB conectado.
2. Mantener las ROMs directamente dentro de `R2N64/roms/gb`, `gbc`, `gba`,
   `n64`, `nes` o `snes`, en la raíz del USB. Por ejemplo, la carpeta que en
   Windows era `E:\R2N64\roms\gb` se consulta en PS4 como
   `/mnt/usb0/R2N64/roms/gb` o `/mnt/usb1/R2N64/roms/gb`.
3. En la biblioteca pulsar **Triángulo** para volver a buscar. Confirmar que
   aparece un juego GB y se puede abrir. No usar comillas ni acentos graves
   alrededor de `R2N64` en el nombre real de la carpeta.
4. Cambiar una preferencia por sistema y reabrir para comprobar persistencia;
   si se utiliza la biblioteca en red, comprobar también su caché.
5. Si sigue apareciendo un aviso, registrar el mensaje completo con la
   operación entre corchetes, el número `errno` y si la ruta es interna o USB.

La corrección y el PKG requieren todavía esta validación física. No se
atribuye una mejora de rendimiento de emulación a este cambio.
