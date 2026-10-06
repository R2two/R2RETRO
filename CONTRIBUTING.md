# Colaborar con R2RETRO

Gracias por ayudar a probar y mejorar el proyecto. Prioridades en [ROADMAP](docs/ROADMAP.md).

## Reportar un error

Indica versión del PKG, modelo PS4, firmware, GoldHEN, sistema/juego y región, pasos, resultado esperado y observado. En N64 incluye CPU/RDP efectivos, HLE/LLE, VI/s y tiempos de pausa. Adjunta solo capturas y fragmentos relevantes de logs, revisados para eliminar datos privados. No compartas ROMs ni BIOS.

## Proponer cambios

1. Clona con submódulos: `git clone --recurse-submodules https://github.com/R2two/R2RETRO.git`.
2. Crea una rama para un cambio concreto y conserva las rutas/identificadores compatibles.
3. No modifiques directamente los submódulos upstream; usa el mecanismo de parches/copias aisladas.
4. Ejecuta las pruebas pertinentes y documenta entorno, comandos, resultados y límites. La suite completa se ejecuta con `bash scripts/build.sh test`.
5. Abre un pull request con el problema, solución y evidencia. Distingue compilación, pruebas desktop y pruebas físicas.

No añadir ROMs comerciales, claves, guardados personales, cachés, toolchains o paquetes al historial. Los PKG se publican como archivos de Releases. Conserva licencias y créditos de terceros. El laboratorio Pokémon 3D sigue separado del PKG PS4.
