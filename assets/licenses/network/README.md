# Procedencia de las bibliotecas HTTPS

R2N64 reutiliza las bibliotecas ya instaladas en OpenOrbis/PacBrew bajo
`/opt/pacbrew/ps4/openorbis/usr/`. Las versiones se contrastaron con
`include/curl/curlver.h` y `include/mbedtls/version.h` el 5 de octubre de 2026.
No se recompilaron ni modificaron estas bibliotecas para incorporar HTTPS.

## curl 7.80.0

`curl.txt` conserva sin cambios el archivo `COPYING` del tag oficial
[`curl-7_80_0`](https://github.com/curl/curl/blob/curl-7_80_0/COPYING).
Incluye copyright 1996–2021 de Daniel Stenberg y colaboradores y la licencia curl.

SHA-256 de `curl.txt`:
`6fd1a1c008b5ef4c4741dd188c3f8af6944c14c25afa881eb064f98fb98358e7`.

## mbedTLS 2.16.6

`mbedtls.txt` concatena íntegramente `LICENSE` y `apache-2.0.txt` del tag oficial
[`mbedtls-2.16.6`](https://github.com/Mbed-TLS/mbedtls/tree/mbedtls-2.16.6):

- [Declaración de licencia](https://github.com/Mbed-TLS/mbedtls/blob/mbedtls-2.16.6/LICENSE).
- [Texto completo Apache-2.0](https://github.com/Mbed-TLS/mbedtls/blob/mbedtls-2.16.6/apache-2.0.txt).

El header de versión instalado conserva el aviso:
Copyright (C) 2006-2015, ARM Limited, All Rights Reserved.
Las fuentes mantienen los demás avisos y excepciones indicados en cada archivo.
La licencia abarca las bibliotecas `mbedtls`, `mbedx509` y `mbedcrypto` utilizadas.

SHA-256 del archivo combinado `mbedtls.txt`:
`c48e8469d46a1226c7039f2f98dc4f746d4300126804e9b82ff94a89a752ba80`.

## Alcance de la procedencia

Estos textos corresponden a las versiones declaradas por la instalación.
No identifican por sí solos los parches ni un commit del port PacBrew; no se
atribuye a los binarios un commit de construcción desconocido. Una entrega de
fuentes debe conservar también la procedencia y los cambios de ese port.

Identidad SHA-256 de los archivos estáticos instalados consultados:

| Archivo | SHA-256 |
|---|---|
| `libcurl.a` | `1edc0332b980112430a0ddd656a5c262a9a3b70c93b8d0300d9eb356edd95637` |
| `libmbedtls.a` | `ebab762669690c35d4d6e95d785326eb58b966ac8e0b66d4885d7a9088a25718` |
| `libmbedx509.a` | `06d0c740979116cc05d9857da3711914819ecf7f14174682a38c2bb1cca924a2` |
| `libmbedcrypto.a` | `3a65c9d950f211047c451147d56ad509e4c6d61dff2b1cdda3f72e98cc50dec5` |

Los certificados públicos del cliente constituyen un recurso separado,
licenciado bajo MPL-2.0. Sus fuentes y checksum están en `assets/certs/README.md`.
