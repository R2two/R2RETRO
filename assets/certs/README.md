# Certificados de confianza para HTTPS

`cacert.pem` es una copia sin modificaciones del conjunto público de raíces
Mozilla convertido a PEM por curl. No contiene claves privadas.

- Fuente fijada: https://curl.se/ca/cacert-2026-09-25.pem
- SHA-256 publicado: https://curl.se/ca/cacert-2026-09-25.pem.sha256
- SHA-256 del archivo incorporado: `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`
- Tamaño: 188900 bytes; 121 certificados.
- Fecha del conjunto: 25 de septiembre de 2026; incorporado el 5 de octubre de 2026.
- Licencia: Mozilla Public License 2.0, texto completo en `MPL-2.0.txt`.
- Procedencia y licencia del convertido: https://curl.se/docs/caextract.html
- Fuente Mozilla: https://github.com/mozilla-firefox/firefox/blob/release/security/nss/lib/ckfw/builtins/certdata.txt

El PKG debe conservar el PEM, este aviso y el texto de licencia. R2N64 pasa su
ruta explícita a libcurl y exige la comprobación del certificado y del nombre
del servidor. No existe una opción para omitir la verificación TLS.

Las actualizaciones del conjunto forman parte de una revisión del proyecto:
descargar desde la fuente HTTPS oficial, contrastar el SHA-256 publicado,
actualizar la fecha/hash de este aviso y repetir las pruebas HTTPS. La aplicación
no reemplaza el archivo de confianza con datos descargados automáticamente.

El PEM conserva certificados; no traslada todas las restricciones adicionales
de dominio que Firefox puede aplicar a determinadas autoridades. Esta
limitación del formato está documentada por curl en la página de procedencia.
