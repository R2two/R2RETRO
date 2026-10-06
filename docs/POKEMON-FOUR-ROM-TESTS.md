# Pruebas locales: Silver, Crystal, Gold y FireRed

5 de octubre de 2026, integración v0.3.2. ROMs autorizadas por el usuario,
leídas en sus rutas originales de Downloads sin copiarlas al repositorio ni
al PKG. Núcleos y frontend existentes, sin modificaciones para estas pruebas.
Datos aislados en `build/four-pokemon-tests/`.

## Resultado

| Juego | Núcleo | Sesiones del núcleo | Restaurar estado y repetir 60 cuadros | Marcos activado/desactivado | Escena final inspeccionada |
|---|---|---|---|---|---|
| Pokémon Silver | SameBoy | 2 × 12.000 cuadros | 2/2 aprobadas | Aprobados | Dormitorio del protagonista |
| Pokémon Crystal Rev 1 | SameBoy | 2 × 12.000 cuadros | 2/2 aprobadas | Aprobados | Dormitorio del protagonista |
| Pokémon Gold | SameBoy | 2 × 12.000 cuadros | 2/2 aprobadas | Aprobados | Dormitorio del protagonista |
| Pokémon FireRed Rev 1 | mGBA | 2 × 12.000 cuadros | 2/2 aprobadas | Aprobados | Dormitorio, interacción con NES |

En total: **96.000 cuadros de núcleo**, más las repeticiones para comprobar
estados. Los cuatro juegos produjeron imagen y PCM no nulo, sin muestras
saturadas, cierres ni fallos de las comprobaciones. Las dos sesiones de cada
cartucho conservaron hashes de secuencia de imagen y PCM idénticos.

Para cada juego se ejecutó además el frontend SDL/GLES2 con marco activado y
desactivado; cada modalidad completa dos sesiones de 1.200 cuadros. Son
**16 sesiones de frontend, 19.200 cuadros adicionales**, con preferencias
persistentes, avance rápido 4x, cola de audio limpia durante el avance y audio
reanudado al soltar. Las capturas muestran el juego completo sin estirarlo.
Los tres GBC se ejecutaron como hardware CGB, incluidos Gold/Silver compatibles
con GB. SameBoy produjo 160 × 144; mGBA, 240 × 160.

## Guardados y audio

Se guardó un estado, se ejecutaron 60 cuadros, se restauró y se repitieron
los mismos pasos: la imagen resultante coincide en las ocho comprobaciones.
La reapertura del núcleo escribió y recuperó los buffers nativos: GBC usa
SRAM de 32.768 bytes y RTC de 32 bytes; FireRed usa guardado de 131.072 bytes.
Esto comprueba el puente de persistencia, no la opción SAVE/Continue del
menú de cada juego ni el avance del reloj durante días u horas.

| Juego | Hash FNV64 de la secuencia de video | Hash FNV64 PCM |
|---|---|---|
| Silver | `0264378832b9155e` | `54ec5a1fe55d1149` |
| Crystal | `e92186eda9620a49` | `0dd70cc2312f27e7` |
| Gold | `bd8ccb1df21d8cd4` | `dbb50029b8a4707f` |
| FireRed | `3b7e1fde8a4d763e` | `73d068bb84d620c5` |

El audio del frontend utiliza SDL dummy: se validan PCM y colas, no sonido
audible. La comprobación de estado compara video; no afirma que los estados
incluyan todo el historial de los filtros de audio del anfitrión.

## Hallazgo de rendimiento pendiente

FireRed generó 34.823 líneas / 3.134.515 bytes de registro durante las dos
sesiones largas, mayormente mensajes informativos de DMA. En la revisión
fijada, `src/gba/dma.c` de mGBA emite `Starting DMA` a nivel INFO; el puente
descarta DEBUG pero conserva INFO, y `src/log.cpp` hace `fflush` por mensaje.
No se encontraron advertencias ni errores de emulación en este registro.

Esto identifica trabajo de entrada/salida potencialmente evitable. No se ha
medido cuánto cuesta en PS4 ni se modificó el filtro de logs en esta tarea.
Una mejora posterior debe reducir los mensajes rutinarios conservando los
diagnósticos útiles, advertencias y errores, con una comparación equivalente
de imagen, audio y estados.

## Evidencia y reproducción

Resumen consolidado: `build/four-pokemon-tests/core-summary.json`.

Matrices producidas por `scripts/run-rom-matrix.py`:

- Silver: `build/four-pokemon-tests/silver/20261005T180209Z-412/matrix.json`.
- Crystal: `build/four-pokemon-tests/crystal/20261005T180232Z-584/matrix.json`.
- Gold: `build/four-pokemon-tests/gold/20261005T180300Z-864/matrix.json`.
- FireRed: `build/four-pokemon-tests/firered/20261005T180201Z-315/matrix.json`.

Cada matriz enlaza su informe con contadores, capturas periódicas, audio WAV
y estados aislados. Escenario `scripted`, 12.000 cuadros, dos sesiones con
datos compartidos, perfil `handheld` y `--require-av`. El ejecutable es
`build/ps4-parity-linux/rom_probe`, compilado Release. Ese nombre identifica
la configuración local; se ejecutó en Linux y no en PS4.

```sh
python3 scripts/run-rom-matrix.py --rom "$ROM" \
  --probe build/ps4-parity-linux/rom_probe \
  --output build/four-pokemon-tests/recheck --frames 12000 \
  --scenario scripted --profiles handheld --sessions 2 \
  --session-data shared --require-av
```

Frontend GBC: `build/four-pokemon-tests/gbc-frontend-b4atliac/summary.json`.
Frontend FireRed: `build/four-pokemon-tests/firered-summary.json` y
`build/four-pokemon-tests/firered-ui/{on,off}/`.
Las capturas `pause.png.game.png` muestran el juego antes del menú de pausa;
`pause.png` conserva las opciones del frontend.

## Integridad y límites

SHA-256 de las cuatro ROMs, idéntico antes y después:

| Juego | SHA-256 |
|---|---|
| Silver | `72b190859a59623cbef6c49d601f8de52c1d2331b4f08a8d2acc17274fc19a8c` |
| Crystal Rev 1 | `fdcc3c8c43813cf8731fc037d2a6d191bac75439c34b24ba1c27526e6acdc8a2` |
| Gold | `fb0016d27b1e5374e1ec9fcad60e6628d8646103b5313ca683417f52b97e7e4e` |
| FireRed Rev 1 | `729041b940afe031302d630fdbe57c0c145f3f7b6d9b8eca5e98678d0ca4d059` |

No se han validado batallas, conexión entre juegos, RTC/día-noche, partidas
largas ni todos los eventos. No son medidas de velocidad, sonido o arranque
en PS4 física. Algunas pruebas se ejecutaron en paralelo y sus tiempos de
host no se usan como comparación de rendimiento.

No se recompiló ni sustituyó el PKG v0.3.2: conserva SHA-256
`747d6401f1b8cf6a0f5012472fcdefbf9c1819494ad7f9490a363a76d28db208`.
