# Perfiles N64 y reducción de trabajo del frontend

6 de octubre de 2026. Cambios posteriores a v0.5.2, todavía sin nuevo PKG.

## Selección por contenido

El menú inicia con **Ajustes → Perfil por juego N64 → Automático**. El núcleo
consulta una tabla local después de leer y validar la ROM. Reutiliza la identidad
de guardado: CRC de cabecera + FNV64 del contenido completo normalizado por
endianness, además del tamaño exacto. No usa el nombre del archivo ni descarga
reglas. No cambia los identificadores ni lee otra vez la ROM para identificarla.
Esta huella es identificación local, no autenticación criptográfica.

| Revisión reconocida | Identidad | Tamaño |
| --- | --- | --- |
| Super Mario 64 USA | `635A2BFF-8B022326-623B80DDBD00E7B7` | 8 MiB |
| Zelda Ocarina of Time USA 1.2 | `693BA2AE-B7F14E9F-6E8332B2F362A221` | 32 MiB |

Ambos seleccionan GLideN64 GLES2, HLE gráfico conservador, audio HLE y CPU
automática (recompilador si la comprobación real de permisos lo permite).
Son **perfiles experimentales** basados en nuestras pruebas locales previas,
no configuraciones óptimas certificadas en PS4. La pausa y el log muestran cuál
se aplicó. No alteran relojes emulados, precisión, framebuffer ni frameskip.

Otros contenidos/revisiones conservan los ajustes base. GB/GBC/GBA/NES/SNES no
reciben estos perfiles. Cambiar explícitamente gráficos, CPU, audio o trabajadores
N64 activa modo manual; activar de nuevo Automático vuelve a permitir la tabla.
La medición de componentes es independiente. Los ajustes duran la sesión, como
los ajustes N64 anteriores; al reiniciar la app vuelve el modo automático.

El frontend aporta el contexto GPU antes de cargar, pero sólo el perfil reconocido
o la selección manual GPU lo inicializan. Sin host disponible se conservan los
ajustes base. Si falla la carga GPU, el frontend reintenta Angrylion con automático
desactivado para impedir un ciclo. Un error durante la ejecución no cambia de
backend a mitad de juego. Los estados rápidos GPU siguen deshabilitados; SRAM
conserva su identidad y funcionamiento anteriores.

Para pruebas optativas desktop: `--n64-auto --rom-smoke <ruta>` con `R2N64_DATA`
aislado. Los llamadores directos del core y las pruebas anteriores siguen en modo
manual salvo que soliciten automático explícitamente.

## Trabajo aplicado

- Buffer PCM: reserva los 176.400 bytes del límite ya existente al cargar, evitando
  crecimiento del vector durante callbacks. No cambia muestras ni límites.
- Intercambio SDL/núcleo: compara estados capturados y omite escrituras GL iguales
  de framebuffer, renderbuffer, programa, texturas, viewport, scissor y capacidades.
  Atributos, VAO, buffers y demás estado conservan restauración completa. Rutas de
  error sin estado actual válido restauran todo. No se añaden readbacks ni fences.
- Diagnóstico: registra versión/renderer y soporte anunciado de binarios de
  programas una vez por carga GPU. No compila un shader adicional por cuadro.
- Medición optativa: picos de tiempo host de núcleo/presentación e intervalos que
  superan el presupuesto nominal. Excluye lotes de avance rápido; las esperas de
  pacing no cuentan como trabajo. No son tiempos exclusivos de ejecución GPU.

La prueba de intercambio sin cambios reduce las escrituras enable/disable de
18 a 0 por pareja begin/end. Es una reducción de llamadas, **no una medida de FPS
ni una ganancia certificada en consola**. Las consultas de estado se conservan.

## Validación y evidencia

- 50/50 CTest antes de la optimización diferencial; 11/11 regresiones focalizadas
  después, incluyendo composición GPU, conservación de estado, ciclo libretro,
  perfiles, tiempos y navegación/render del XMB.
- Mario y Zelda: automático y manual GPU+HLE, dos sesiones de 1.200 VI por modo y
  juego, **9.600 VI** en total. Las cuatro capturas finales del juego tienen SHA256
  idéntico a sus referencias v0.5.2. Carga, cierre y segunda carga completados.
  No se certifica aquí igualdad PCM, partida completa ni rendimiento físico.
- Linux/Mesa llvmpipe, SDL offscreen/opengles2, `MESA_GLES_VERSION_OVERRIDE=2.0`,
  audio dummy. ROMs leídas en Downloads, hashes originales intactos, datos aislados.
- La ejecución inicial sin limitar Mesa a GLES2 obtuvo GLES3.2 y falló en Mario
  con `GL 0x0502` durante la primera llamada del núcleo. Se reprodujo desactivando
  las restauraciones diferenciales; no lo introduce esa optimización. El fallo
  GLES3.2 queda pendiente; no se presenta el cambio de entorno como arreglo.
- Ejecutable PS4 compilado y enlazado con OpenOrbis/PacBrew existente. No ejecutado
  en PS4. El PKG v0.5.2 permanece intacto, SHA256
  `7cbd17dbfc17081df8077fc6a84cc3e7a09c7de95bf39ebddea4b3f216344691`.

Evidencia local: `build/n64-auto-ctest.log`, `build/n64-auto-final-tests.log`,
`build/n64-auto-rom-smoke-gles2/results.json`, capturas/logs bajo ese directorio,
`build/n64-auto-es3-fullrestore.log`, `build/n64-auto-ps4-build.log`.

## Pendiente de la referencia DolphinPS4

No se integran Dolphin/GameCube/Wii. Su JIT PowerPC no sustituye el recompilador
MIPS de N64. Un backend RADV/GNM requiere portar el entorno gráfico y comprobar
sus capacidades, ABI y compatibilidad; no es un selector listo para activar en
nuestro build GLES2. Ver [auditoría](DOLPHIN-PS4-INTEGRATION.md).

La caché de shaders en disco continúa desactivada. El lector de GLideN64
`GLSL/glsl_ShaderStorage.cpp` construye un vector con `binaryLength` del archivo
sin una validación previa suficiente y también redimensiona cadenas con longitudes
leídas. Antes de habilitarlo hacen falta límites, lecturas completas, invalidación
por driver/revisión y pruebas con cachés truncadas/corruptas. Anunciar soporte de
program binary no demuestra que guardar/restaurar shaders funcione en Piglet.

## GoldHEN y frecuencias

En las [funciones oficiales de GoldHEN](https://github.com/GoldHEN/GoldHEN) y sus
[releases](https://github.com/GoldHEN/GoldHEN/releases) consultadas no se encontró
una función/API documentada de overclock CPU/GPU para PS4 original/Slim.
El plugin [FliprateRemover](https://github.com/GoldHEN/GoldHEN_Plugins_Repository)
elimina el límite de presentación mediante `sceVideoOutSetFlipRate`; no aumenta
la frecuencia del hardware. El [Modo optimizado de Sony](https://www.playstation.com/en-us/support/hardware/ps4-pro-boost-mode/)
corresponde a PS4 Pro. No se añade un interruptor de overclock sin implementación
verificada. La prioridad sigue siendo medir el coste RSP/CPU/GL en la PS4 real.
