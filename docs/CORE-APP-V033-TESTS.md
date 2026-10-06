# v0.3.3 — diagnósticos de núcleos y capturas

Validación local del 5 de octubre de 2026. Se conservan las revisiones de
Mupen64Plus-Next, SameBoy y mGBA y sus opciones de emulación. Los cambios
afectan al puente de diagnósticos y a la aplicación. Sin prueba física PS4.

## Cambios disponibles

- **Capturar imagen** desde pausa: PNG del juego con escala, filtro y marco
  activos, sin menú ni indicadores. No ejecuta cuadros adicionales del núcleo.
  Disponible en N64 y portátiles. PS4 guarda en
  `/data/R2N64/screenshots/{n64,gb,gbc,gba}/`; desktop usa su raíz configurada.
  Nombre fechado y creación exclusiva: nunca sobrescribe capturas anteriores.
  Un fallo de codificación/escritura elimina el archivo incompleto y mantiene
  la pausa con un aviso. La lectura GPU y escritura ocurren solo al solicitar
  la captura, nunca por cuadro durante una partida normal.
- **Estadísticas: activadas/desactivadas** en pausa portátil: permite ocultar
  el HUD de rendimiento durante el juego. Las métricas de pausa y el aviso de
  avance rápido siguen visibles. `showStats` es opcional en preferencias v1,
  verdadero por defecto, persistente por sistema y sin reescritura al jugar.
- **mGBA:** únicamente los INFO que coinciden con el formato completo y conocido
  `GBA DMA: Starting DMA ...` se cuentan por canal. Un resumen al cerrar incluye
  los mensajes de reset/unload/deinit. Se conservan WARN, ERROR, mensajes INFO
  distintos o con formatos desconocidos. SameBoy y N64 mantienen sus registros.
  Los cuatro contadores no escriben al disco por mensaje; no se cambia DMA ni
  su temporización emulada. Un cierre abrupto puede perder el resumen pendiente.

## Pruebas automáticas

Compilación Linux Debug y **28/28 CTest** aprobados. Evidencia en
`build/v033-ctest.log` y `build/desktop/Testing/Temporary/LastTest.log`.

- Contrato libretro: 10.000 mensajes DMA no alteran el archivo hasta cerrar;
  contadores, apertura/cierre repetidos, reset, deinit y carga rechazada;
  conservación de avisos, errores, formatos desconocidos y otros núcleos.
- Renderer software y GLES2: PNG válido, nombres únicos, conservación de la
  primera imagen, captura del cuadro actual, fallo de escritura inyectado sin
  archivo incompleto y rechazo de destinos inválidos o enlazados.
- Preferencias antiguas y nuevas, tipos JSON inválidos y persistencia del HUD.
- Seis configuraciones GB/GBC/GBA, dos sesiones de 1.200 cuadros por caso:
  marco activado/desactivado, HUD legado/activado/desactivado, avance rápido y
  vuelta de audio. Dos capturas únicas por caso, última imagen idéntica byte
  por byte a la composición limpia sin pausa. Los archivos de preferencias
  quedan intactos al jugar. También aprobado en compilación Release.
- Regresión N64 de CPU, RDP, entrada, puente, audio, XMB y almacenamiento.

La captura real de Piglet, el sonido audible y la interacción DS4 de esta
versión deben verificarse en consola. Las pruebas SDL usan audio dummy.

## Comparación con cuatro ROMs autorizadas

ROMs leídas directamente en Downloads, sin copiarlas ni incluirlas en el PKG.
Datos de prueba aislados en `build/v033-pokemon-tests/`. Por juego: dos sesiones
Release de 12.000 cuadros con entradas sintéticas y dos replays de 60 cuadros.
Total: **96.000 cuadros**, ocho replays, 336 archivos PNG/WAV idénticos a v0.3.2.
No se usa el tiempo de estas ejecuciones concurrentes como benchmark.

| Juego | Hash de secuencia de video FNV64 | Hash PCM FNV64 | Resultado |
|---|---|---|---|
| Silver | `0264378832b9155e` | `54ec5a1fe55d1149` | Idénticos a v0.3.2 |
| Crystal Rev 1 | `e92186eda9620a49` | `0dd70cc2312f27e7` | Idénticos a v0.3.2 |
| Gold | `bd8ccb1df21d8cd4` | `dbb50029b8a4707f` | Idénticos a v0.3.2 |
| FireRed Rev 1 | `3b7e1fde8a4d763e` | `73d068bb84d620c5` | Idénticos a v0.3.2 |

ROMs intactas, cero WARN/ERROR y SRAM idéntica. RTC se escribe/restaura y
conserva su tamaño; sus bytes varían con el reloj del anfitrión. Estas pruebas
no validan SAVE/Continue desde el menú del juego ni el RTC a largo plazo.
Resumen y rutas exactas: `build/v033-pokemon-tests/regression-summary.json`.

FireRed: **3.134.515 → 2.199 bytes** de log y **34.823 → 19 líneas** (99,93 %
menos bytes). Los **34.806** registros rutinarios se agrupan exactamente:
por sesión, canales 0/1/2/3 = 34/1.734/1.734/13.901. El resto del registro
coincide tras normalizar fecha y rutas. Esto demuestra menor escritura de
diagnósticos; no cuantifica una ganancia de velocidad en PS4.

Reproducción por ROM:

```bash
python3 scripts/run-rom-matrix.py --rom /ruta/local/al/juego \
  --probe build/ps4-parity-linux/rom_probe --output build/v033-pokemon-tests \
  --frames 12000 --scenario scripted --profiles handheld \
  --sessions 2 --session-data shared --require-av
```

## Paquete PS4

Compilación y enlace OpenOrbis/PacBrew aprobados con `bash scripts/build.sh ps4`.
PKG validado y extraído; verificación byte por byte de ejecutable, marcos,
fondo, icono, fuentes, diagnóstico original y licencias. No contiene ROMs
comerciales ni el laboratorio 3D. v0.3.2 permanece intacta.

- Archivo: `dist/R2N64-v0.3.3-core-app-upgrades.pkg`.
- Tamaño: **50.921.472 bytes**. SFO **00.33**. Title ID **RNTD00064**.
- SHA-256: `6d33f9e2ffa6e7bdeb9b947c0166dc3b41a954af5ee19078a1b48d41b973a7e8`.
- Registro: `build/v033-ps4-build.log`; metadatos: `dist/build-info-v0.3.3.json`.
- **Arranque y rendimiento de v0.3.3 en PS4 física pendientes.**
