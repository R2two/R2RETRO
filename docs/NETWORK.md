# Descargas HTTPS

Cambio posterior a v0.5.4, todavía sin compilar: Cloudflare DoH fijo para todo
`httpGet/httpDownload`, incluidos RDB y Boxart/Snap/Title de Libretro. Sin
selector ni fallback al DNS del sistema; bootstrap 1.1.1.1/1.0.0.1 únicamente
para cloudflare-dns.com, TLS/CA/hostname verificados. No modifica DNS PS4.
La configuración de red alternativa existe solo en el target de pruebas HTTP.

`include/http.h` y `src/http.cpp` proporcionan `r2n64::httpGet`. La operación es
bloqueante y pertenece al trabajador de descargas de la aplicación. El catálogo
decide qué recursos de Libretro solicitar y dónde guardar el resultado; la
capa de transporte no escribe archivos, no analiza imágenes ni registra datos.

Desde v0.4.4, la ruta CA se construye a partir del directorio de recursos
comprobado después de habilitar acceso a datos PS4. Se conserva un descriptor
antes del cambio de sandbox; sólo se acepta una ruta que conserve su identidad.
Los errores locales de CA distinguen ruta, open, fstat, tipo, archivo vacío,
tamaño y read, con errno y sin exponer la ruta en la interfaz. Preflight POSIX:
archivo regular, no enlazado, no vacío, hasta 2 MiB y primer byte legible. Curl
sigue validando el contenido PEM, la cadena TLS y el hostname. La lectura previa
no sustituye la validación TLS. [Incidente y pruebas](PS4-CA-ASSETS-V044.md).

## Contrato

```cpp
bool httpGet(const std::string& url, size_t maxBytes,
             const std::string& caFile, const std::atomic<bool>& cancel,
             std::vector<uint8_t>& body, std::string& error);
```

En éxito devuelve el cuerpo completo y limpia `error`. Ante fallo o cancelación,
`body` queda vacío: no se entrega un archivo parcial al siguiente componente.
El límite debe ser positivo y representable por `curl_off_t`. Se valida el
tamaño anunciado y cada bloque recibido, incluyendo respuestas chunked; una
excepción de asignación no cruza el callback C de libcurl.

Se admiten únicamente direcciones HTTPS sin credenciales incrustadas. El
cliente verifica la cadena TLS y el nombre del servidor, exige TLS 1.2 como
mínimo y utiliza el PEM indicado por `caFile`. Se permiten como máximo tres
redirecciones, también HTTPS. `.netrc` no aporta credenciales y no se activa un
almacén de cookies. Un HTTP 404, otros errores HTTP, problemas TLS, límites de
tamaño y transferencias incompletas producen mensajes diferenciados. Los
mensajes no incorporan cuerpos del servidor, URLs, credenciales o rutas locales.

La conexión dispone de 12 segundos y la transferencia de 40 segundos en total.
Una tasa inferior a 1024 bytes/segundo durante 10 segundos también interrumpe la
descarga. El callback de progreso y el receptor consultan la cancelación atómica.
La frecuencia del callback depende de libcurl y del estado de la conexión;
la cancelación no constituye una garantía de respuesta instantánea del sistema.

## Plataforma

Linux utiliza libcurl del entorno; la validación local se ejecutó con 8.5.0.
El OpenOrbis/PacBrew existente aporta curl 7.80.0, mbedTLS 2.16.6 y resolución
AsynchDNS. `CURLOPT_NOSIGNAL` evita que el trabajador manipule señales del proceso.
La compilación usa las opciones de protocolo disponibles en cada versión,
comprobando siempre el resultado de cada `curl_easy_setopt`.

PS4 inicializa la red al solicitar la primera descarga, después de comprobar
que SDL video está listo. Carga exclusivamente el módulo NET, ejecuta
`sceNetInit` y comprueba si la red ya estaba inicializada cuando la llamada
retorna error. No modifica USER_SERVICE, GoldHEN, input, audio ni la secuencia de
arranque existente. La comprobación de red ya inicializada no registra ni
transmite la dirección de hardware obtenida por la API.

La inicialización global de curl está serializada y cada descarga posee su
propio handle. `httpShutdown()` se llama después de cancelar y unir los
trabajadores. No hace cleanup mientras haya peticiones activas y no termina
el módulo NET compartido. El transporte debe ser el único propietario de la
inicialización/cleanup de libcurl dentro de R2N64.

Enlace PS4: `curl mbedtls mbedx509 mbedcrypto z SceNet`, junto con SDL2 y
SceSysmodule ya usados por la aplicación. Linux: `libcurl` y soporte de hilos.
El módulo no requiere NetCtl, pool HTTP de Sony, SDK oficial ni una toolchain
adicional.

El conjunto de confianza se distribuye en `assets/certs/cacert.pem`. Fuente
fijada, checksum, licencia MPL-2.0 y actualización están en
[assets/certs/README.md](../assets/certs/README.md). No se debilita TLS para
compensar errores de fecha o certificados en una consola.

## Reproducción de las pruebas

Las pruebas offline crean una autoridad/certificado de localhost temporal,
exclusivamente dentro del directorio de pruebas. Su clave se elimina al terminar
y no se incluye en assets ni en el PKG.

```sh
mkdir -p build/http-v041
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude \
    src/http.cpp tests/http_tests.cpp -o build/http-v041/http_tests -lcurl -pthread
python3 tests/http_tests.py build/http-v041/http_tests assets/certs/cacert.pem \
    --work-dir build/http-v041
```

Los 44 casos actuales cubren URLs prohibidas, CA ausente/no confiable, nombre de servidor
incorrecto, cuerpo binario y vacío, redirección HTTPS, downgrade HTTP, bucles,
HTTP 404/403/500, tamaño declarado/chunked, archivo truncado, cancelación antes
y durante la descarga, timeout por conexión estancada y recuperación posterior.
Se añadieron en v0.4.4 rutas vacías/con NUL, CA vacío, directorio, symlink, FIFO,
PEM malformado y límite de 2 MiB, con recuperación usando una CA válida tras
cada rechazo local. El timeout del harness es 35 segundos. La prueba normal no
necesita Internet. Los 44 casos pasan con AddressSanitizer y
UndefinedBehaviorSanitizer (`build/http-v044/test-asan.log`); la evidencia
histórica de 26 casos se conserva en `build/http-v041/tests-sanitized.txt`.

La comprobación optativa de Internet usa el mismo código y el CA distribuido:

```sh
build/http-v041/http_tests --smoke https://thumbnails.libretro.com/ assets/certs/cacert.pem
```

El 5 de octubre de 2026 pasaron el índice de Libretro (27685 bytes) y una imagen
PNG pública (166824 bytes), sin guardar su contenido en el repositorio. Evidencia
de la segunda descarga: `build/http-v041/libretro-smoke.txt`. El archivo CA
coincide con el SHA-256 publicado por curl. `src/http.cpp` también se compiló con
los headers y target reales de OpenOrbis.

Las pruebas de transporte Linux y la compilación cruzada no verifican la
conectividad, DNS, reloj ni negociación TLS del firmware de una PS4 física.
