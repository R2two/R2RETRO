# v0.4.1 — Biblioteca en línea y sin conexión

R2N64 consulta [Libretro Database](https://github.com/libretro/libretro-database)
y [Libretro Thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails)
directamente desde la aplicación. No necesita una utilidad de Windows, cuenta
de usuario ni clave API. La implementación usa el mismo frontend C++/SDL2 y
se compila para OpenOrbis/PacBrew; las pruebas físicas de esta versión siguen
pendientes. Se conservan los seis sistemas y las revisiones de sus núcleos.

## Uso en PS4

1. Conectar la consola a Internet y abrir la Biblioteca de R2N64.
2. Seleccionar un juego y pulsar **OPTIONS**. La descarga se solicita por juego;
   no hay descargas automáticas al arrancar ni durante la emulación.
3. **OPTIONS** vuelve a cancelar. La biblioteca permanece navegable mientras
   se trabaja. **X** abre la ficha y otro **X** inicia el juego.
4. Si se inicia un juego mientras se descarga, la app cancela el trabajo y
   atiende los controles hasta que termina. Navegar cancela ese inicio diferido.
5. Una ficha guardada vuelve a cargarse sin Internet al seleccionar el juego,
   también después de cerrar y abrir R2N64.

En desktop, **M** sustituye a OPTIONS para esta acción. Los controles dentro
de los juegos no cambian: OPTIONS sigue siendo pausa N64 y Start en los demás.

El XMB muestra título, región, año, género, desarrollador, editor y jugadores
cuando Libretro dispone de esos campos. Jugadores describe el juego original;
R2N64 continúa admitiendo un solo mando. Los datos no se traducen ni completan
por inferencia: un campo ausente aparece como «Sin datos». No se añaden sinopsis.

Se intentan descargar carátula, captura y pantalla de título. La carátula tiene
prioridad visual; si falta se usa la captura o el título. No se añade un selector
de las tres imágenes en este hito. Una ROM no reconocida conserva su ficha
básica e icono. No se asignan portadas mediante búsquedas aproximadas por nombre.

## Datos y trabajo en segundo plano

La raíz configurada se respeta; en PS4 es `/data/R2N64`:

- `cache/libretro/<sistema>.rdb`: base compartida por sistema.
- `cache/library/<sistema>/<id>.meta`: ficha local versionada, con hash de ROM.
- `covers/<sistema>/<id>/`: PNG verificados y nombrados por su contenido.

Las ROMs se leen para identificarlas y nunca se suben al proveedor. La lectura,
el cálculo de hashes, HTTPS, la escritura de caché y la decodificación PNG
ocurren en un trabajador. SDL crea la textura en el hilo de render y la reutiliza;
dibujar la biblioteca no decodifica ni descarga imágenes por cuadro. Al jugar
no queda activo el trabajador de biblioteca.

Las fichas previas permanecen si hay un fallo o cancelación. Las imágenes nuevas
se escriben con nombres inmutables antes de reemplazar la ficha. Una interrupción
puede dejar una imagen sin referencia, sin invalidar la ficha anterior. Guardados,
estados, archivos ROM e identificadores de juego no se modifican.

Se verifica HTTPS con certificados de confianza incluidos. Errores de red,
certificado, archivo ausente y formato inválido se presentan sin impedir jugar.
Si falla TLS, comprobar conexión y fecha de la consola; la verificación nunca
se desactiva. Límites, procedencia y pruebas del transporte en [NETWORK.md](NETWORK.md).
Identificación, normalización y política de caché en [LIBRETRO-CATALOG.md](LIBRETRO-CATALOG.md).

## Validación

**38/38 grupos CTest aprobados**, con regresión de N64, GB/GBC/GBA y NES/SNES.
Compilación desktop y OpenOrbis completadas; PKG extraído y comparado con staging.
Archivo: `dist/R2N64-v0.4.1-libretro-library.pkg`, **63.766.528 bytes**, SFO `00.41`,
Title ID `RNTD00064`. SHA-256:
`3c4184b072d7ca979f3d1e4093dac3371d1ffc415174be511f4ad77b4cb581c8`.
Se conserva intacto v0.4.0. Registros: `build/v041-ctest.log`,
`build/v041-final-desktop-build.log`, `build/v041-package-build.log` y
`dist/build-info-v0.4.1.json`.

Los tests nuevos usan cartuchos y PNG sintéticos propios, transporte simulado
y un servidor HTTPS local. Comprueban hashes/normalización/ambigüedad, RDB
truncados y duplicados reales de `serial`, rutas y enlaces, cancelación,
conservación de caché, límites de imágenes, certificados y redirecciones,
trabajo fuera del hilo principal y texturas sin nuevas lecturas o subidas por
cuadro. Renderizado probado en software y GLES2/Mesa. HTTPS y catálogo pasan
también AddressSanitizer y UndefinedBehaviorSanitizer.

La comprobación de formatos leyó seis RDB oficiales y reconoció las copias
locales autorizadas de Pokémon Red y Super Mario 64 por SHA-1. En el frontend
GLES2, Pokémon Red descargó los tres PNG y mostró la ficha completa. Una segunda
ejecución cargó la ficha e imagen con proxies HTTPS/HTTP dirigidos a un puerto
local cerrado, sin depender de la conexión. Las ROMs se leen desde Downloads;
no se copian ni se ejecutan en estas comprobaciones del catálogo.

Evidencia local bajo `build/`: `catalog-tests/reference-results.txt`,
`catalog-tests/library_metadata-tests-asan.log`, `http-v041/tests-sanitized.txt`,
`v041-library-download-2.log`, `v041-library-offline.log` y las capturas
`v041-library-red/download.png` / `offline.png`. El primer intento real detectó
claves `serial` duplicadas válidas en RDB, corregidas y añadidas a regresión;
se conserva ese diagnóstico en `v041-library-download.log`.

Una compilación OpenOrbis y un PKG validado no demuestran conectividad, sonido,
compatibilidad de juegos ni rendimiento en PS4 física. La prueba física debe
confirmar descarga, cancelación, reapertura sin conexión y retorno de una partida.
