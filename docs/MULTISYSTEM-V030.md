# R2N64 v0.3.0 — GB, GBC y GBA

Implementación del MVP de `GB-GBC-GBA-BRIEF.md`. Mantiene el nombre, Title ID,
logo y fondo de R2N64. Referencia física del usuario: PS4 original/Slim;
esta actualización todavía no se ha ejecutado en esa consola.

PKG compilado y validado: `dist/R2N64-v0.3.0-multisystem.pkg`,
49.217.536 bytes, SFO `00.30`, SHA-256
`8ae4cd4ae68d09cc6bbb739d1f9bd374a3303652dfc38494e53650203836b3d0`.
Se verificaron firmas, hashes, metadatos, ejecutable, imágenes y todos los avisos
de licencia después de extraer el paquete. El cartucho comercial de prueba no
forma parte del payload; solo se conserva el diagnóstico N64 original anterior.

## Comportamiento

- CoreManager detecta extensión y valida cabecera. SameBoy ejecuta GB/GBC,
  mGBA ejecuta GBA y Mupen64Plus-Next conserva N64 con sus parches previos.
- Una sola capa libretro recibe video, audio y controles. Solo una sesión de
  núcleo permanece activa; se puede volver a la biblioteca y cambiar de sistema.
- L1/R1 filtran la biblioteca. Se exploran la raíz ROM y sus subcarpetas
  inmediatas `gb`, `gbc`, `gba` y `n64`.
- Portátiles usan escalado entero y nearest: GB/GBC 160×144, GBA 240×160.
  En la pantalla de 1920×1080 corresponden a 7× y 6× respectivamente.
- Portátiles: Cruz=A, Cuadrado=B, OPTIONS=Start, táctil=Select, L1/R1=L/R GBA,
  L3+R3=pausa. Desktop: Z/X, Enter/Tab, Q/W, Esc. N64 conserva sus controles.
- Guardados nativos y RTC separados por sistema. Los estados tienen un espacio
  por juego y comprueban versión, identidad e integridad. Se rechazan estados
  incompatibles sin reemplazar la sesión cuando es posible revertirla.
- N64 con audio HLE oculta los estados: el estado auxiliar HLE aún no se
  serializa. N64 con LLE conserva la ruta de estados comprobada.

## Validación local

Suite completa: **23/23 pruebas aprobadas**, registro
`build/v030-final-tests.log`. Incluye arranque y fallo controlado, scanner,
XMB software/GLES2, entrada, audio, contratos del puente, sesiones N64/JIT,
medición RDP y los tres cartuchos portátiles originales. Detalle de fixtures
en [HANDHELD-TESTS.md](HANDHELD-TESTS.md).

El empaquetador añadió después otra entrada CTest: `package_directory_tree`,
que ejecuta dos pruebas Python de carpetas homónimas y rutas inválidas; ambas
pasaron. El registro actual contiene 24 entradas. La aplicación GBA completa
también pasó dos sesiones de 1.200 cuadros con audio y reapertura del guardado:
captura `build/v030-gba-preview.png`, escalado entero 6× y RGB correctos.

Regresión adicional de Zelda OoT U V1.2: 6.000 VI neutros con JIT y audio HLE,
sin cambios en la biblioteca N64. La secuencia de video
`a71ae48fd6b4d44b` y PCM `778926d8f2ef541a` coincide con la referencia anterior;
también coinciden los 50 bloques de 120 VI, sus muestras y tareas HLE.
Evidencia: `build/v030-n64-regression/20261005T151145Z-2452/baseline-comparison.json`.
Es una regresión funcional en PC, no una medición de PS4.

Las pruebas detectaron un defecto en el getter SRAM de mGBA después de cargar
un estado: devolvía el respaldo anterior aunque la emulación usaba la memoria
restaurada. La adaptación expone la memoria activa y el reset conserva esos
datos en memoria. Se verificaron cargar estado → cerrar → abrir y cargar
estado → reiniciar, sin ejecutar cuadros intermedios que oculten el problema.

También se retiró del bucle de cuadros la consulta de capacidad de estados:
mGBA genera internamente un snapshot al consultar su tamaño. Ahora se consulta
al abrir la pausa. La captura del frontend detectó y permitió corregir un tamaño
de fuente no cargado que ocultaba las opciones de ese menú.

## Pokémon Red proporcionado por el usuario

Archivo leído directamente en Downloads, **1.048.576 bytes**, SHA-256
`5ca7ba01642a3b27b0cc0b5349b52792795b62d3ed977e98a09390659af96b7b`.
No se copió al proyecto ni al paquete; todos los datos de prueba están aislados.

Banco Release final: **dos sesiones de 6.000 cuadros**, sin cierres ni timeout,
SameBoy en modo DMG, video 160×144, PCM estéreo a 48 kHz. Se observó la portada,
menús y entrada de nombres; esto no valida una partida completa.

| Comprobación por sesión | Resultado |
|---|---|
| Cuadros ejecutados | 6.000 |
| Frecuencia nominal del núcleo | 59,727501 Hz |
| Muestras PCM no nulas | 9.380.392 |
| Muestras saturadas | 0 |
| Hash secuencia RGB | `c917c6a42cf8cb94` |
| Hash secuencia PCM | `eac83c2346310960` |
| Guardar/cargar estado y repetir 60 cuadros | Mismo video, `4a784a06fc019f6c` |

Los hashes coinciden entre ambas sesiones con datos nuevos. La aplicación SDL
completa también pasó dos sesiones de 1.200 cuadros con renderer GLES2,
audio SDL dummy, cierre, restauración de SRAM y retorno al frontend. Captura
revisada: `build/v030-pokemon-preview.png`. Sus cifras sin limitador son del PC,
no medidas de PS4 ni una promesa de velocidad física.

Evidencia: `build/v030-pokemon-states/20261005T150930Z-280/matrix.json` y
`1-handheld/run-370-yQjju1/report.json`; incluye PNG y los últimos 10 segundos
de PCM. El banco inicial está separado en `build/v030-pokemon-tests/`.

## Probar en PS4

Instalar el PKG v0.3.0 y poner las ROMs propias en, por ejemplo,
`/data/R2N64/roms/gb/`, `roms/gbc/` o `roms/gba/`, o esas mismas carpetas
bajo `R2N64/` en un USB. No se requiere BIOS propietaria para la configuración
integrada. Triángulo vuelve a escanear la biblioteca.

Comprobar imagen, sonido, cruceta, A/B, Start y Select; luego abrir L3+R3,
guardar estado, avanzar, restaurarlo y volver a la biblioteca. Reabrir el
juego y después un título N64. Anotar Emulación %, FPS/cuadros y tiempos del
núcleo, además de cualquier fallo. El firmware y GoldHEN exactos siguen sin
registrarse. Esta prueba física es la que falta para confirmar el nuevo PKG.

## Fuera de este MVP

Rewind, fast forward, remapeo configurable, shaders/paletas seleccionables,
selector manual de núcleos alternativos, favoritos/recientes, carátulas,
configuración por juego y menú de capturas quedan para siguientes etapas.
La actualización no activa RDP por GPU ni demuestra mejoras de rendimiento N64.
No incluye ROMs comerciales, BIOS propietarias ni claves. Las fuentes,
adaptaciones y avisos están en [HANDHELD-CORES.md](HANDHELD-CORES.md) y
`THIRD_PARTY_LICENSES.md`.
