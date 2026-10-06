# Catálogo Libretro — v0.4.1

La descarga se solicita para el juego seleccionado. La identificación, red,
validación de caché y preparación de imágenes se ejecutan en el worker del
catálogo. No se transmiten ROMs ni partidas al servidor. `Game.id`, SRAM y
estados conservan sus nombres y formatos anteriores.

## Fuentes

- Bases oficiales: [libretro/libretro-database](https://github.com/libretro/libretro-database),
  `master/rdb/Nintendo - {Game Boy, Game Boy Color, Game Boy Advance,
  Nintendo 64, Nintendo Entertainment System, Super Nintendo Entertainment System}.rdb`.
- Formato: [libretro-db de RetroArch](https://github.com/libretro/RetroArch/tree/master/libretro-db).
  El lector propio recorre MessagePack, comprueba la cabecera `RARCHDB`, el
  centinela y el recuento de registros. No sigue los índices del archivo.
  Los RDB oficiales pueden repetir `serial` en texto y binario; ese campo se ignora.
- Imágenes: [libretro-thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails),
  repositorios por sistema, ramas `master`, carpetas `Named_Boxarts`,
  `Named_Snaps` y `Named_Titles`. Las sustituciones de caracteres siguen sus
  [reglas de nombres](https://github.com/libretro-thumbnails/libretro-thumbnails#file--naming-guidelines).
  Los bytes restantes se codifican como un segmento URL, nunca como ruta local.

Las ramas remotas son actualizables, no una versión fijada dentro del PKG.
Los datos agregan aportes de Libretro y sus fuentes; las imágenes conservan los
derechos de sus respectivos autores, desarrolladores y editores. Ningún RDB ni
imagen comercial de este catálogo se incorpora al paquete de distribución.

## Identificación y caché

Se prioriza SHA-1 del contenido. CRC32 se admite únicamente para registros sin
SHA-1 y sin coincidencias contradictorias. No se buscan títulos aproximados.
Los tamaños declarados, cuando existen, también deben coincidir. Dos fichas
distintas con la misma fuerza de coincidencia producen un error de ambigüedad.

N64 se convierte lógicamente a big-endian mientras se calcula el hash. SNES
omite el copier opcional de 512 bytes. NES prueba el archivo iNES completo y
la vista sin cabecera ni trainer; los RDB oficiales incluyen formatos con y sin
cabecera. GB/GBC/GBA usan todos los bytes. Las fichas locales incluyen el hash
completo normalizado, que se recalcula incluso al cargarlas sin conexión.

- RDB compartido: `cache/libretro/<sistema>.rdb`.
- Ficha: `cache/library/<sistema>/<Game.id>.meta`, formato binario versionado.
- Imágenes: `covers/<sistema>/<Game.id>/{boxart,snap,title}-<sha1PNG>.png`.

Una base local válida se reutiliza, incluso si no encuentra el juego; una base
ausente o corrupta se intenta descargar. Para actualizar voluntariamente una
base válida se puede retirar sólo su `.rdb` de caché y solicitar de nuevo la
descarga. Una ficha conocida sigue disponible sin conexión.

Las imágenes son opcionales: una ficha identificada puede guardarse aunque
falte alguna; el mensaje conserva HTTP/TLS/PNG para diagnóstico. Los PNG tienen
nombres inmutables y la ficha se reemplaza al final mediante rename atómico.
Un error no trunca la ficha anterior; una interrupción puede dejar un PNG nuevo
sin referencia, pero no cambia sus imágenes activas. Se rechazan enlaces y
archivos especiales en las rutas propias de caché.

## Límites y pruebas

RDB: 32 MiB, 250 000 registros, 256 campos/mapa, 1 024 elementos/array y ocho
niveles. PNG: 4 MiB, dimensiones de 8 a 2 048 píxeles por lado, hasta cuatro
millones de píxeles; firma, chunks, CRC, IHDR, IDAT e IEND comprobados antes de
publicar. SDL_image realiza la decodificación final fuera del hilo de UI.
Ficha: 16 KiB. Todos los límites también se aplican a archivos locales.

`library_metadata_tests` usa HTTP simulado, ROMs originales y PNG sintéticos.
La comprobación independiente con seis RDB reales leyó GB 4 399, GBC 2 984,
GBA 4 169, N64 2 472, NES 30 298 y SNES 7 762 registros. Las copias locales
autorizadas de Pokémon Red y Super Mario 64 coincidieron por SHA-1; esa prueba
sólo leyó y calculó hashes, sin ejecutar ni copiar los juegos.
La red y el catálogo todavía requieren comprobación en PS4 física.
