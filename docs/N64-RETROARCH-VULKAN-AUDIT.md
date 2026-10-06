# N64: revisión de RetroArch, Vulkan y comparación de RSP

## Evidencia física nueva

El usuario confirmó que, con v0.5.0 en PS4 original/Slim, la pausa indica
**GLideN64 GPU (experimental)** y el rendimiento sigue parecido. Esto confirma
el backend indicado por la aplicación, no una medición GPU ni velocidad normal.
No se recibió un perfil por componente de esa ejecución. La hipótesis de que
simplemente no se había activado GLideN64 queda descartada para esa prueba.

## RetroArch y diferencia con nuestro perfil

El [Makefile.orbis de RetroArch](https://github.com/libretro/RetroArch/blob/4a56c2f38c47ab2b6a15ba3becc869464e45e8dc/Makefile.orbis)
activa OpenGL ES 2/EGL y enlaza Piglet; no proporciona en ese target una
integración Vulkan que podamos activar directamente. Su toolchain orbisdev
no es idéntica a nuestro PacBrew/OpenOrbis.

En nuestro núcleo fijado a `12edd2c74a517ff86dfa8cfc71ad75e4c10486d5`,
`libretro/libretro.c:948` permite RSP HLE con GLideN64, y lo selecciona si
no se proporciona otra opción. En cambio `core/libretro_core.cpp` fuerza
`rsp-plugin=cxd4` incluso con GLideN64. La GPU rasteriza, pero el RSP sigue
interpretando las tareas gráficas. Nuestro HLE actual sólo acelera audio
reconocido. Más RAM o más trabajadores Angrylion no eliminan ese trabajo;
los trabajadores Angrylion no son el renderizador activo en modo GLideN64.

Otra zona a medir es la frontera SDL/núcleo: `GpuSession::begin/end` conserva
el estado GLES2 mediante numerosas consultas/restauraciones en cada VI.
RetroArch tiene un driver de vídeo propio y gestión de contexto/estado distinta
([gl2.c](https://github.com/libretro/RetroArch/blob/4a56c2f38c47ab2b6a15ba3becc869464e45e8dc/gfx/drivers/gl2.c)).
Esto identifica trabajo a perfilar, **no prueba que esas consultas sean el
cuello de botella en PS4**. No se puede eliminar la protección de estado sin
probar texturas, buffers, atributos, pausa y retorno al frontend.

## Vulkan: bloqueo verificable en el código revisado

Existe [vulkan-ps4](https://github.com/PS4-OpenGNM/vulkan-ps4), sobre OpenGNM y
un compilador SPIR-V/GCN. Por tanto no sería correcto decir que Vulkan en
homebrew PS4 es imposible. Tampoco basta su README para validar un emulador.

Revisión examinada: `f4d940b723771b3fe637e8ac0704bb5d6ab19622`.

- [vk_ps4_vulkan11.c](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_vulkan11.c)
  devuelve `storageBuffer16BitAccess = VK_FALSE`, tanto en Vulkan11Features
  como en 16BitStorageFeatures. Esto describe ese driver, no una imposibilidad
  física del hardware ni una limitación irremediable de un compilador futuro.
- [vk_ps4_entry.c](https://github.com/PS4-OpenGNM/vulkan-ps4/blob/f4d940b723771b3fe637e8ac0704bb5d6ab19622/src/vk_ps4_entry.c)
  anuncia la extensión de 16 bits, pero eso no activa la característica
  anterior. No anuncia `VK_KHR_8bit_storage` en su lista de dispositivo.
- El [ParaLLEl-RDP incluido en nuestro núcleo](https://github.com/libretro/mupen64plus-libretro-nx/blob/12edd2c74a517ff86dfa8cfc71ad75e4c10486d5/mupen64plus-video-paraLLEl/parallel-rdp/parallel-rdp/rdp_renderer.cpp)
  rechaza el dispositivo sin `storageBuffer16BitAccess` o
  `storageBuffer8BitAccess` (`Renderer::init_caps`, líneas 166–176). Desactivar
  aritmética de tipos pequeños no suprime esos requisitos de almacenamiento.

Conclusión: **no hay una ruta directa funcional para ParaLLEl-RDP con esas
revisiones**. Habría que implementar y probar las capacidades ausentes, además
del host Vulkan/libretro, memoria, sincronización y presentación de R2RETRO.
No se compiló ni ejecutó ese driver en consola; no se ha probado su conformidad.
El SDK instalado tiene headers de SDL Vulkan, pero no se encontró un ICD
Vulkan preparado. No se instaló otra toolchain ni se sustituyó Piglet.

## Experimento local reproducible

Se añadieron `tools/n64_rsp_probe.cpp` y
`scripts/run-n64-rsp-comparison.py`. Son herramientas optativas de escritorio;
no modifican el núcleo ni la aplicación y no forman parte del PKG o CTest.
Usan los archivos autorizados de Mario USA y Zelda OoT U V1.2 en Downloads,
sin copiarlos. SHA-256 antes/después idénticos en las ocho ejecuciones finales.

Condiciones:

- Misma biblioteca GLES2 previamente compilada con los parches 0001–0007:
  SHA-256 `f95fdd3b57cbd0d3575f97e0b8980a0b1952d97259d7fff2c239890ba5514ecb`.
- WSL Ubuntu, Mesa 25.2.8, **llvmpipe software**, GLES 2.0; no es Piglet ni GPU
  PS4. Host compilado `-O2 -Wall -Wextra -Werror`.
- Recompilador x64 con permisos comprobados; GLideN64 320×240, sin texturas HD,
  sin caché de shaders de GLideN64 ni caché de shaders de Mesa en disco.
- Arranque nuevo por proceso, 300 VI de calentamiento, 1.800 VI medidos;
  dos repeticiones por ROM/modo, orden CXD4/HLE y HLE/CXD4. Sin input: escenas
  de título/introducción. 16.800 VI totales contando calentamiento.
- Captura fuera del intervalo medido; `glFinish` en sus extremos, sin
  readback por VI. No incluye composición SDL, XMB, presentación ni audio
  físico. Los temporizadores del núcleo están activos y añaden coste.
- CXD4 usa nuestro audio HLE reconocido; HLE usa la implementación completa
  upstream. **No es un A/B exclusivamente gráfico**. En modo HLE, el tiempo de
  audio permanece dentro de RSP y su contador específico marca cero.

Promedios del tiempo de pared por VI (menor es mejor):

| ROM | CXD4 + audio HLE | RSP HLE upstream | Reducción |
| --- | ---: | ---: | ---: |
| Super Mario 64 USA | 3,147 ms | 1,947 ms | 38,1 % |
| Zelda OoT U V1.2 | 4,528 ms | 3,871 ms | 14,5 % |

Rangos entre repeticiones: Mario CXD4 3,088–3,206 ms, HLE 1,937–1,957 ms;
Zelda CXD4 4,502–4,554 ms, HLE 3,865–3,876 ms. No convertir estas cifras
en porcentajes de velocidad esperados en PS4. Sólo dos repeticiones y una
ventana de escena por juego; no son un estudio estadístico ni gameplay completo.

Las ocho sesiones terminaron correctamente, produjeron 2.100 callbacks de
vídeo hardware y muestras PCM no nulas, sin errores GL detectados en los
límites comprobados. Los PNG finales se repiten exactamente dentro de cada
modo. Hay diferencias de imagen **entre** modos, visibles en Zelda; no se
ha certificado equivalencia gráfica ni PCM. Se inspeccionaron capturas de
los dos juegos. No se verificaron guardado/Continue, juego prolongado ni
sonido audible. La primera pasada `mario-zelda` queda como exploratoria:
usaba caché de Mesa y su captura intermedia no restauraba el FBO; no se usa
para las cifras finales. Esto se corrigió en el banco antes de repetir.

Resultados válidos: `build/rsp-comparison/final/results.json`, y `process.log`,
`warmup.png`, `final.png` en cada subcarpeta. Ejemplo desde WSL:

```sh
python3 scripts/run-n64-rsp-comparison.py --label nueva-comparacion \
  '/ruta/externa/Super Mario 64 (USA).z64' \
  '/ruta/externa/Zelda.z64'
```

El directorio de salida debe ser nuevo; las ejecuciones tienen timeout.
El script verifica que tampoco cambie la biblioteca durante la comparación.

## Siguiente integración y límites

Prioridad fundamentada: una opción GLideN64 + RSP HLE con CXD4 conservado
como alternativa, y medición separada del tiempo de intercambio de estado
SDL/GL. Antes del PKG hay que resolver una diferencia de seguridad funcional:
`mupen64plus-rsp-hle/src/plugin.c:HleForwardTask` devuelve `-1`; el fallback
LLE de tareas desconocidas **no está conectado** en esta revisión. Además
el interruptor/contador de audio de R2RETRO controla nuestro puente CXD4,
no el HLE completo. Cambiar sólo la cadena `cxd4` por `hle` dejaría esos
controles engañosos y perdería el respaldo existente para audio desconocido.

Esta investigación deja un candidato medido, no un arreglo publicado.
No se alteraron opciones de producción ni se generó un PKG nuevo.
v0.5.0 permanece intacta: SHA-256
`0ea552579f5acbdc2fd8636db4f33dc9aae1fb77228172cc7da441e74788cb15`.
