# Diagnósticos originales de GB, GBC y GBA

Este banco comprueba los núcleos portátiles a través del mismo `Emulator` que usa
la aplicación. Los programas se generan desde código original; no contienen ROMs
comerciales, logo Nintendo ni BIOS propietarias. Las pruebas de escritorio no
confirman arranque, controles, audio ni velocidad en la PS4 física.

## Archivos y generación

`scripts/make_handheld_diagnostics.py` genera estos archivos, por defecto en
`build/handheld-fixtures`:

| Archivo | Programa | Video | Memoria de guardado |
|---|---|---|---|
| `diagnostic.gb` | SM83, MBC1 con batería | 160 × 144, cuatro tonos y desplazamiento horizontal | 8 KiB SRAM |
| `diagnostic.gbc` | SM83, modo CGB, MBC1 con batería | 160 × 144, blanco/rojo/verde/azul y desplazamiento | 8 KiB SRAM |
| `diagnostic.gba` | ARM, modo 3 | 240 × 160, bandas RGB y un píxel animado | 32 KiB SRAM |

Cada archivo ocupa 32 KiB. GB/GBC generan un pulso PSG continuo; GBA utiliza el
canal PSG 1. Los programas consultan los registros de entrada de la máquina
emulada y reflejan su estado en SRAM. La zona de logo de cada cabecera contiene
exclusivamente ceros. Sus checksums de cabecera son válidos.

SameBoy acepta estas cabeceras y arranca con su bootstrap libre incorporado si no
hay una imagen externa. Las fuentes fijadas `libretro/libretro.c` y
`BootROMs/dmg_boot.asm`/`cgb_boot.asm` muestran que esa ruta no exige comparar el
logo del cartucho. mGBA identifica el salto ARM de entrada y el byte fijo `0x96`;
la configuración del frontend desactiva la carga de BIOS externa.

Estos diagnósticos son material de pruebas bajo `build/`. El generador no los
copia a `assets`, al payload PS4 ni al PKG.

```sh
python3 scripts/make_handheld_diagnostics.py build/handheld-fixtures
```

La generación imprime longitud y SHA-256 de cada archivo. No requiere
ensambladores externos: el script codifica únicamente las instrucciones
utilizadas por estos programas.

## Prueba integrada

`tests/handheld_tests.cpp` recibe cinco argumentos:

```sh
build/desktop/handheld_tests \
  build/handheld-fixtures/diagnostic.gb \
  build/handheld-fixtures/diagnostic.gbc \
  build/handheld-fixtures/diagnostic.gba \
  assets/diagnostic.z64 \
  build/handheld-tests
```

Cada ejecución crea un directorio `data-XXXXXX` independiente. Conserva allí el
log, los guardados, los estados y capturas PPM/WAV para inspección. No elimina ni
utiliza guardados personales.

Las comprobaciones cubren:

- Selección de sistema y núcleo; dimensiones y patrón de píxeles esperado.
- Ejecución real del programa, mediante firmas y contadores escritos en SRAM.
- Audio estéreo no silencioso durante los últimos 60 de 360 cuadros de arranque,
  para que el sonido del bootstrap no pueda aprobar por sí solo la prueba. Ambos
  canales deben oscilar alrededor de cero a la frecuencia esperada del PSG:
  aproximadamente 512 Hz en GB/GBC y 128 Hz en GBA, con un margen del 10 %.
- Llegada de A/B, Start, Select, arriba y L/R —estos dos últimos en GBA— a los
  registros emulados; entrada neutral cuando el mando está desconectado.
- Guardado nativo y persistencia de un marcador que el programa no modifica.
- Creación de estado, reproducción de la misma secuencia de 17 cuadros con
  idénticos píxeles y contadores de programa/entrada, y continuidad del audio.
- Rechazo de estados corruptos, de otro sistema o de otra ROM sin modificar la
  sesión activa.
- Carga de estado seguida de cierre inmediato y nueva carga, con un marcador
  distinto en el respaldo, para detectar guardados obtenidos de memoria obsoleta.
- Reinicio, nueva ejecución de la inicialización y conservación de SRAM, incluso
  reiniciando inmediatamente después de cargar un estado con datos diferentes.
- Cambios GB → GBC → GBA → N64 → GB/GBC/GBA sin reiniciar el frontend, incluido
  un estado N64 con RSP LLE y la recuperación posterior de los datos portátiles.

Al cargar un estado, mGBA restaura SRAM en una máscara interna de memoria. El
getter libretro upstream continúa apuntando al respaldo anterior hasta que la
máscara se sincroniza. El banco detectó esta diferencia: el primer cuadro GBA
coincidía, pero el contador leído mediante el getter seguía adelantado. La
comprobación exige exponer la SRAM activa y que cerrar inmediatamente después de
cargar estado persista esa memoria, sin depender de ejecutar más cuadros ni de
un periodo sin escrituras del juego. El getter local corregido expone la memoria
activa después de la inicialización diferida, manteniendo el respaldo original
durante la carga inicial. El frontend también conserva SRAM/RTC alrededor del
reinicio, para que mGBA no descarte la máscara restaurada al reiniciar. Ambos
caminos pasan las comprobaciones integradas de cierre y reinicio inmediatos.

La equivalencia de estados exige igualdad de video y progreso/entrada del
programa. El banco comprueba que sigue saliendo audio, pero no exige PCM idéntico
después de restaurar: los búferes de salida y filtros de cada núcleo pueden tener
semánticas propias. No se deduce ausencia de diferencias audibles de esa prueba.

## Resultados de escritorio y límites

La comprobación directa preliminar de 360 cuadros con las bibliotecas portátiles
compiladas produjo las tres firmas esperadas, cuatro colores por imagen, las
dimensiones y tamaños de SRAM de la tabla y muestras PSG no silenciosas. Esa
comprobación directa precede al test del frontend y no sustituye sus resultados.

El 5 de octubre de 2026, después de corregir el getter y la conservación de SRAM
al reiniciar, `handheld_fixtures` y `handheld_core_sessions` aprobaron **2/2**.
La sesión completa terminó en 3,32 segundos de tiempo de prueba en Linux; este
valor no es un benchmark de juegos ni una medida de PS4.

| Sistema | PCM observado en los últimos 60 cuadros | Frecuencia de salida |
|---|---:|---:|
| GB | 48.219 cuadros estéreo; 96.438 muestras no silenciosas | 48.000 Hz |
| GBC | 48.219 cuadros estéreo; 96.438 muestras no silenciosas | 48.000 Hz |
| GBA | 32.918 cuadros estéreo; 65.836 muestras no silenciosas | 32.768 Hz |

Las comprobaciones de frecuencia PSG, replay de 17 cuadros, entrada, persistencia,
estados inválidos, reinicio y cambios de núcleo descritas arriba aprobaron en esa
misma ejecución. Las revisiones de origen son SameBoy
`8230189896a8bb6598574d302ba0ad3658f98ab4` y mGBA
`26b7884bc25a5933960f3cdcd98bac1ae14d42e2`, con los ajustes locales registrados en
el proceso de compilación. La biblioteca mGBA probada tenía SHA-256
`33c15976258c009cff91321a9ec1ba9a63235bdc6120350a73a61ea8bb3873db`.

Evidencia: `build/handheld-final-check.log`; archivos de esa ejecución en
`build/desktop/handheld-tests/data-zLmlQl`. La suite general, las pruebas con juegos
autorizados por el usuario y la compilación final del PKG se registran por
separado. Ningún éxito de este banco demuestra compatibilidad completa con el
catálogo, rendimiento real de PS4, audio físico correcto ni ausencia de
regresiones en juegos N64 comerciales.
