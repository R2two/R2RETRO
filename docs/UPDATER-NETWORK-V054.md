# Actualizador: conexión y cancelación — v0.5.4

## Cambio posterior sin compilar: Cloudflare fijo

El usuario confirma que Cloudflare funciona en su PS4 y solicita dejarlo
predeterminado y sin posibilidad de elegir otro proveedor en R2RETRO.
El código actual fija DoH Cloudflare en el transporte HTTP compartido: afecta
actualizador y descargas Libretro (bases, fichas y carátulas). Se elimina la
opción editable del menú; el nombre aparece solo como información de conexión.
No cambia el DNS global de PS4 ni se permite fallback al resolver del sistema.
Se conservan CA, verificación peer/host y bootstrap previamente implementados.

Las preferencias antiguas conservan canal/búsqueda automática. El antiguo byte
DNS se valida al leer por compatibilidad, pero se ignora y se guarda como1.
Overrides de endpoint/resolución solo existen al compilar el target aislado
`http_tests` con `R2N64_HTTP_TESTING=1`, nunca en el ejecutable de la aplicación.
Para futuras pruebas manuales de ese target con sanitizadores hace falta esa
misma definición. Se actualizaron las pruebas fuente, **sin ejecutarlas ni
compilar**. No hay nuevo PKG ni versión publicada. La validación que sigue
corresponde al paquete v0.5.4 anterior, que aún tenía selector.

## Problema observado y límites

El usuario instaló v0.5.3 por USB en PS4 física (13.52; GoldHEN cree
2.4b18.6) e informó que permanece en «Buscando versión publicada…».
Usa DNS 62.210.38.117. Desde el PC ese DNS resolvió los tres hosts GitHub
consultados; esto no prueba el comportamiento de la red de la PS4.
No se identificó todavía la causa exacta del bloqueo físico.

El canal `main/updates/experimental.txt` devuelve HTTP 404 al realizar este
trabajo: no se ha publicado el manifiesto. Un 404 debe terminar la búsqueda
con «Este canal todavía no tiene un manifiesto publicado», sin ofrecer instalar.

## Cambios

- Ajustes → Actualizaciones → **DNS: Sistema / HTTPS (Cloudflare)**.
  Optativo, predeterminado Sistema; solo se aplica a consultas y descargas del
  actualizador. No altera DNS global, bloqueo de actualizaciones PS4, ni catálogo
  Libretro. No hay fallback oculto ni direcciones GitHub fijadas.
- DoH usa `https://cloudflare-dns.com/dns-query`. `CURLOPT_RESOLVE` proporciona
  exclusivamente su bootstrap `1.1.1.1,1.0.0.1`, con caché privada de la petición.
  Se conserva nombre/SNI y comprobación de certificado. PacBrew tiene mbedTLS
  2.16.6: se evita depender de certificados con IP literal.
- TLS peer/host y CAINFO explícitos también para los handles internos DoH.
  Curl 7.80 hereda CAINFO en `dohprobe`; no se desactiva validación ni se usa
  CAINFO_BLOB. Endpoint/bootstrap son constantes del cliente, no datos del canal.
- Bucle `curl_multi_perform/poll` con espera máxima de 100 ms entre comprobaciones
  de cancelación. Se reprodujo localmente que `easy_perform` demoraba la
  cancelación del padre durante una petición DoH interna estancada. Con multi
  pasa el mismo caso; no se abandona ni se separa el trabajador.
- UI muestra etapa aproximada y segundos: CA, inicialización, DNS/conexión,
  TLS, respuesta, recepción, cierre, verificación e instalación. Las etapas se
  registran sin cabeceras, cuerpos ni información privada remota. Se conservan
  límites conexión 12 s, manifiesto 40 s, descarga 30 min, baja velocidad 10 s.
- A los 45 s una consulta aún pendiente recibe cancelación desde UI y muestra
  aviso. **Es un watchdog cooperativo, no una garantía de terminar una syscall
  bloqueada de PS4.** Círculo solicita cancelación y vuelve al menú; jugar y
  nuevas transferencias esperan que el trabajador finalice. El cierre normal
  sigue uniéndolo; no se descarga curl con trabajadores activos.
- Durante consulta/descarga se puede guardar modo DNS y búsqueda al iniciar.
  Cambiar DNS cancela la petición previa y se aplica al siguiente Buscar ahora.
  Canal, descarga e instalación permanecen bloqueados mientras hay trabajo.
  La entrega a BGFT no se cancela desde la UI.
- Preferencias v2 de cinco bytes: `2`, automático, experimental, DoH, LF.
  Lectura compatible con v1 de cuatro bytes y DoH desactivado al migrar.
  No se cambian IDs, rutas, partidas ni opciones de emulación.

## Reproducción

```sh
cmake --build build/desktop -j4
ctest --test-dir build/desktop --output-on-failure
bash scripts/check-update-installer.sh
build/desktop/http_tests --doh-smoke \
  https://raw.githubusercontent.com/R2two/R2RETRO/main/updates/experimental.txt \
  assets/certs/cacert.pem https://cloudflare-dns.com/dns-query 404
```

`tests/http_doh_tests.py` usa dominios `.invalid`, un servidor DoH local con TLS,
DNS wire format real y un segundo servidor HTTPS. Prueba resolución bootstrap,
redirección a otro host, 404, CA incorrectas de ambos servidores, nombre TLS
incorrecto, DNS malformado, espera cancelada y rechazo de DoH HTTP. Ninguna
fixture contiene ROMs, credenciales reales ni claves de producción.

Resultados del 7 de octubre de 2026:

- 52/52 CTest aprobados (`build/updater-v054-full-tests.log`). Tras el ajuste
  final de navegación, se repitieron las tres pruebas XMB/software/GLES2 y
  diagnóstico de inicio, también aprobadas (`updater-v054-final-ui.log`).
- Los diez casos DoH pasan además con ASan/UBSan, 14 peticiones DNS reales
  locales (`build/updater-v054-doh-sanitized.log`). El caso de cancelación exige
  terminar antes de 5 s; el servidor simula una espera de 6 s.
- Adaptador BGFT simulado y verificador/worker/persistencia pasan ASan/UBSan en
  backend nativo y operaciones de archivo PS4 (`updater-v054-sanitizers.log`).
- Consulta real con DoH y CA de la aplicación termina en HTTP 404 esperado
  (`updater-v054-live-doh.log`). Es Linux/libcurl del host, no PacBrew ejecutado.
- Ejecutable PS4 recompilado, PKG extraído, assets y SFO verificados. El
  verificador de la aplicación acepta manifiesto y paquete final.
- `dist/R2RETRO-v0.5.4-updater.pkg`: **65.994.752 bytes**, SFO **00.54**,
  SHA-256 `0b0ea347ff591c5bfddaf78651f5f90b1087e3cafa8d2daebe39921a366f356b`.
- v0.5.3 conserva su SHA-256 anterior. Manifiesto v0.5.4 preparado en `dist/`,
  **sin publicación remota**, sin instalación física realizada por el agente.

Las pruebas desktop no certifican la implementación de red de PS4, la eficacia
del DNS de bloqueo ni la instalación sobre sí misma mediante BGFT.

## Prueba en consola

1. Instalar v0.5.4 por USB sobre v0.5.3, sin desinstalar la aplicación.
2. Conservar DNS 62.210.38.117 en la PS4.
3. Ajustes → Actualizaciones → DNS → HTTPS (Cloudflare), luego Buscar ahora.
4. Sin canal publicado, el resultado correcto es «Este canal todavía no tiene
   un manifiesto publicado». No hay una actualización remota instalable todavía.
5. Si falla, registrar mensaje completo, etapa y segundos; si tarda más de 45 s,
   comprobar Círculo y conservar `startup.log` bajo la raíz de datos configurada.

## Fuentes de implementación

- [Curl 7.80, doh.c: CAINFO heredado y verificación DoH](https://github.com/curl/curl/blob/curl-7_80_0/lib/doh.c).
- [CURLOPT_DOH_URL y bootstrap con CURLOPT_RESOLVE](https://curl.se/libcurl/c/CURLOPT_DOH_URL.html).
- [Cloudflare DoH: endpoint, formato y TLS](https://developers.cloudflare.com/1.1.1.1/encryption/dns-over-https/make-api-requests/).
- [Mbed TLS: limitaciones de IP/SNI](https://mbed-tls.readthedocs.io/en/latest/kb/how-to/use-sni/).
