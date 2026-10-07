# R2RETRO v0.5.2 — compatibilidad de la ruta N64 GPU

## Alcance

Se continúa con el homebrew nativo PS4; no se integra Linux. El usuario indica
que v0.5.1 mantiene la lentitud en su consola. No se ha recibido un nuevo perfil
físico y estos cambios no se presentan como una solución de rendimiento.

El parche 0009 corrige dos defectos reproducibles en las copias aisladas del
núcleo, sin modificar el submódulo upstream ni ampliar la lista HLE admitida:

- **Tareas gráficas suspendidas:** v0.5.1 rechazaba PC reanudado, pero no la
  bandera `OS_TASK_YIELDED`. Una tarea puede reiniciar por el código de arranque
  con esa bandera y necesitar restaurar su contexto. Ahora se envía a CXD4 real
  antes de modificar el estado SP o llamar a GLideN64. La bandera `DP_WAIT`
  por sí sola no desactiva HLE. Esto no identifica la causa de la lentitud de
  Mario/Zelda ni confirma que una escena comercial concreta presentara el fallo.
- **Nombre interno vacío:** el recorte de espacios en `RSP_Init` consultaba
  `romname[strlen(romname)-1]` sin comprobar longitud. Una cabecera con nombre
  vacío o solo espacios podía leer antes del array. Ahora se comprueba longitud
  y se conserva el nombre vacío; no se inventa un título ni un perfil de juego.

El manejo de tareas/suspensión está descrito en la documentación de
[OSTask](https://ultra64.ca/files/documentation/online-manuals/functions_reference_manual_2.0i/os/OSTask.html)
y en el código HLE existente (`jpeg.c`, `memory.h`). No se copió código ni
microcódigo comercial para las pruebas.

## Reproducción de los defectos

- `tests/graphics_hle_tests.c`: añade tarea gráfica con PC de arranque y bandera
  de suspensión. Antes del parche falla la aserción de respaldo LLE
  (`build/v052-yield-before.log`); después ejecuta el microprograma escalar
  original que escribe un marcador en DMEM. También comprueba DP_WAIT, audio,
  interrupciones, límites y reinicialización con ASan/UBSan.
- `scripts/check-gpu-rom-name.py`: extrae el bloque real de decodificación del
  source preparado, lo compila con ASan/UBSan y prueba cinco cabeceras originales:
  vacía, espacios, título con padding, longitud máxima y NUL intermedio. Antes
  informa índice 18446744073709551615 fuera de `char[21]`
  (`build/v052-name-before.log`); después pasa los cinco casos.

Comandos desde WSL:

```sh
bash scripts/build-core.sh build-only
bash scripts/check-graphics-hle.sh
python3 scripts/check-gpu-rom-name.py
```

La admisión GLideN64 se simula en el test del despachador; la ejecución CXD4
es real. La prueba del nombre ejecuta el bloque de producción aislado, no todo
el arranque GPU. Las pruebas de juegos/frontend complementan esos límites.

## Regresión final

- 49/49 CTest aprobados, 131,47 s (`build/v052-ctest.log`). Incluyen lifecycle
  GPU, cambio hardware/software, núcleo y frontend de los otros sistemas.
- Ambos tests ASan/UBSan pasan después del parche:
  `build/v052-yield-after.log`, `build/v052-name-after.log`.
- Mario USA y Zelda OoT U V1.2: 2 modos × 2 sesiones × 1.200 VI por juego,
  9.600 VI. Salida GPU, muestras de audio, captura y regreso a biblioteca
  verificados, sin fallback al renderer CPU ni errores de proceso. Contadores
  HLE positivos en su modo; cero en GPU/LLE. Los cuatro PNG finales del juego
  coinciden byte a byte con sus respectivas capturas v0.5.1. Esto no equivale
  a comparar todos los cuadros o PCM ni a probar partidas completas.
- Hashes de ROM antes/después idénticos; datos aislados en
  `build/v052-rom-smoke/`, resultados en `results.json`, comparaciones de
  capturas en `image-comparison.json`.

Estas sesiones usan Linux/Mesa llvmpipe. No se publican sus tiempos como una
ganancia de velocidad; no miden GPU, audio físico ni estabilidad de la PS4.

## Compatibilidad conservada

Mismos modos Angrylion/GPU+HLE/GPU+LLE, misma selección inicial y controles,
audio HLE opcional, copias de framebuffer y protección del estado GL. No se
habilitan estados rápidos GPU, no se cambian las rutas/identidad/guardados,
ni los núcleos GB/GBC/GBA/NES/SNES. Los originales de arte permanecen intactos.
Los tests leen las ROMs autorizadas desde Downloads y usan datos aislados;
ninguna ROM comercial se incorpora al PKG.

## Paquete local

`dist/R2RETRO-v0.5.2-compatibility.pkg`: 64.225.280 bytes, SFO `00.52`.
SHA-256: `7cbd17dbfc17081df8077fc6a84cc3e7a09c7de95bf39ebddea4b3f216344691`.
Compilación OpenOrbis, extracción y comparación de payloads aprobadas.
Metadatos: `dist/build-info-v0.5.2.json`.

El empaquetador vuelve a preparar los avisos y parches desde las fuentes
actuales, incluso en builds incrementales que no ejecutan `build.sh` completo.
Se verificó que el parche 0009 queda incluido. No se ha publicado una release
remota: el enlace público de README continúa apuntando a v0.5.1.

v0.5.1 se conserva con SHA-256
`4889669f6e556fc8b229111be2f5a1c4f650ecbbdb66025bdfacf990f8bb4483`.
No hay prueba de v0.5.2 en PS4 física ni ganancia de velocidad certificada.
