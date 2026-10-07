# R2RETRO

**Versión actual: v0.5.5 experimental.** En pausa GB/GBC/GBA/NES/SNES se puede elegir Shader (LCD/CRT/apagado) y escribir SRAM/RTC. GBA añade salto de cuadros 0/1/2. Todo desactivado inicialmente salvo las preferencias anteriores. La SRAM no sustituye Guardar dentro del juego. Ajustes → Actualizaciones consulta el canal experimental; información/carátulas y actualizaciones usan Cloudflare DoH fijo, sin cambiar el DNS PS4. N64 inicia con perfil automático por identidad exacta; el resto conserva la base. [Notas y límites](releases/v0.5.5.md).

Las secciones por versión siguientes describen su comportamiento histórico.

Emulador homebrew de Nintendo 64, Game Boy, Game Boy Color, Game Boy Advance, NES y SNES para PlayStation 4, en C++17 y SDL2, con interfaz XMB.

**v0.5.1 — GPU + HLE gráfico:** Ajustes → Gráficos de Nintendo 64 permite
acelerar tareas gráficas reconocidas con GLideN64, conservando CXD4 para
las demás y la opción de audio HLE existente. El modo GPU + RSP LLE permite
comparar. La pausa muestra contadores de tareas y, con el perfil activado,
el coste de intercambio SDL/núcleo. Se aplica al abrir la próxima ROM;
Angrylion sigue siendo el modo inicial. Pruebas locales de Mario y Zelda
completadas; rendimiento de esta versión en PS4 pendiente.
[Integración, evidencia y prueba física](GPU-HLE-V051.md).

**v0.5.0 — GPU experimental y marcos:** Ajustes → Gráficos de Nintendo 64
permite elegir GLideN64 GLES2 a 320×240, con textura GPU compartida con SDL y
respaldo Angrylion ante fallo de inicialización. Angrylion sigue siendo el
modo inicial. Se libera la copia de ROM N64 del frontend y se comprueba la
subida y composición real de los marcos. Pendientes compatibilidad y medición
en PS4 física. [Cambios y validación](GPU-OVERLAY-V050.md).

**v0.4.5 — vídeo y núcleos:** los marcos GB/GBC/GBA/SNES conservan sus imágenes
comprimidas antes de GoldHEN y se decodifican desde memoria al primer uso.
La textura del juego declara RGB opaco; la pausa muestra el estado real del
marco y permite distinguir una imagen negra del núcleo de un fallo de
presentación. Añade regresión MMC3 para NES y pruebas locales de Super Mario
Bros. 3. [Cambios, pruebas y límites](VIDEO-CORE-V045.md).

**v0.4.4 — certificados y recursos en PS4:** conserva la identidad del directorio
de recursos al habilitar acceso a datos y localiza su ruta efectiva para las
descargas HTTPS, marcos y diagnóstico incluido. Añade errores precisos de
lectura del CA; mantiene la comprobación de certificados y servidor.
[Corrección y validación](PS4-CA-ASSETS-V044.md).

**v0.4.3 — acceso a archivos en PS4:** el escáner abre las subcarpetas con
rutas completas verificadas y los ajustes/caché utilizan operaciones compatibles
con la libc de OpenOrbis instalada. Los errores de lectura identifican la
operación y su código. [Corrección y validación](PS4-FILESYSTEM-V043.md).

**v0.4.2 — R2RETRO y marco SNES:** nuevo nombre e icono de PS4 con el logo
aportado por el usuario. SNES incorpora su marco, con **L3+R3 → Marco → X**
para activarlo o desactivarlo. Conserva 4:3 por defecto y recuerda la preferencia.
Las configuraciones nuevas lo activan; si ya guardaste `overlay:false`, se respeta.
R2RETRO conserva el Title ID y las carpetas `R2N64` para continuar usando las
ROMs, partidas y configuraciones existentes sin moverlas.
[Cambios y validación](R2RETRO-V042.md).

**v0.4.1 — biblioteca con Libretro:** selecciona un juego y pulsa **OPTIONS**
(**M** en desktop) para descargar su ficha y carátula directamente por Internet.
OPTIONS cancela una descarga activa; X abre los detalles. Se identifica el
contenido de la ROM y se guardan los datos e imágenes para navegar después
sin conexión. No requiere Windows, cuenta ni clave API. Incluye captura y
pantalla de título cuando existen en Libretro; el XMB muestra la carátula y
recurre a esas imágenes si falta. Las revisiones de núcleos y los identificadores
de guardado se conservan. [Uso y pruebas](LIBRARY-V041.md).

**v0.4.0 — NES y SNES:** integra FCEUmm y bsnes-mercury Performance, filtros de biblioteca, controles y datos separados por sistema. NES acepta `.nes`; SNES acepta `.sfc` y `.smc`. Ambos ofrecen cinco espacios de estado, reinicio, capturas, indicador opcional y avance rápido 2x/4x/8x. Imagen 4:3 por defecto, con escala entera opcional. [Guía y validación](NES-SNES-V040.md). La ejecución en PS4 física sigue pendiente; las pruebas Linux no miden rendimiento en consola.

Las capturas se guardan en `/data/R2N64/screenshots/{gb,gbc,gba,n64,nes,snes}/` con nombres únicos; en desktop usan la raíz de datos configurada. Se toman mientras el juego está pausado. **L3+R3 → Estadísticas → X** guarda la preferencia por sistema, excepto N64; las métricas completas siguen disponibles en pausa. **Marco** permite activar o desactivar el overlay de GB/GBC/GBA/SNES. NES y N64 mantienen su presentación sin marco. Se conserva la agrupación de diagnósticos rutinarios mGBA. [Cambios y pruebas v0.3.3](CORE-APP-V033-TESTS.md).

Abre **L3+R3**, usa arriba/abajo para recorrer todas las opciones y X para cambiar la seleccionada. R2 acelera mientras se mantiene pulsado; Espacio hace lo mismo en desktop. El audio se silencia durante el avance y vuelve al soltar. El multiplicador es un objetivo, condicionado por el rendimiento disponible. Paletas GB: Gris (predeterminada), Verde clásico, Oliva y Turquesa; se aplican al continuar. GBC/GBA conservan sus colores propios.

En la biblioteca, **L1/R1** alternan Todos, Game Boy, Game Boy Color, Game Boy Advance, Nintendo 64, NES y SNES. La selección del núcleo es automática. En GB/GBC/GBA/NES/SNES, **OPTIONS es Start**, el panel táctil es Select y **L3+R3 abre la pausa**. Consulta [núcleos NES/SNES](CONSOLE-CORES.md), [marcos y pruebas v0.3.2](HANDHELD-V032-TESTS.md), [guía multisistema](MULTISYSTEM-V030.md) y [núcleos portátiles](HANDHELD-CORES.md).

Desde v0.2.3 hay mediciones opcionales por componente y una prueba manual de compilación GLSL, enlace y dibujo en framebuffer. Se conservan el recompilador x64 comprobado, audio HLE reconocido y Angrylion/CXD4. Desde v0.5.0 se puede seleccionar GLideN64 GPU para juegos; v0.5.1 añade HLE gráfico. La prueba GPU manual sigue siendo un diagnóstico independiente y no demuestra una mejora de velocidad.

Las capturas del usuario de v0.2.1 muestran Zelda Ocarina of Time (U) V1.2 al **44 %** (26,2 VI/s; núcleo 37,0 ms/VI; presentación 0,6 ms/VI) y Super Mario 64 al **32 %** (19,4 VI/s; núcleo 50,9 ms/VI; presentación 0,1 ms/VI). No se confirmó cuántos trabajadores estaban seleccionados. El coste observado se concentra dentro del núcleo; esta versión lo desglosa por componentes para elegir la siguiente optimización.

Se conserva el fondo original aportado por el usuario y el arranque GLES2/Piglet de v0.1.2. Se reutilizan OpenOrbis/PacBrew y las herramientas PKG utilizadas por R2FPKGI, sin modificar ese proyecto.

## Primera prueba de emulación

1. Instalar `dist/R2RETRO-v0.5.5-display-network.pkg` y abrir **R2RETRO**.
2. Ir a **Acerca de → Prueba Nintendo 64** y pulsar **X**. La prueba viene incluida; no necesita descargar ROMs.
3. Comprobar las bandas roja, verde y azul, y un tono continuo de aproximadamente **500 Hz**. El cuadrado de la esquina superior izquierda pasa de negro a blanco al pulsar un botón N64 o mover un stick, y vuelve a negro al soltar.
4. Pulsar **OPTIONS**, elegir **Continuar** y comprobar que imagen, entrada y sonido se reanudan.
5. Abrir OPTIONS nuevamente y elegir **Volver a la biblioteca**. Repetir la prueba incluida para comprobar una segunda sesión.

La prueba incluida es un programa MIPS original de 4 KiB, sin contenido de juegos ni bootcode de Nintendo. Comprueba CPU, framebuffer, audio y mando; no demuestra compatibilidad con juegos, rendimiento general ni ejecución de listas RDP de juegos.

Las pruebas desktop de esta actualización comprueban sesiones con intérprete/recompilador, retorno al intérprete cuando se rechaza memoria ejecutable y cargas originales de CPU/RDP. Los resultados de cada versión se registran en [docs/STATUS.md](STATUS.md). Esas comprobaciones no sustituyen la validación del PKG ni su medición física.

## Comprobar rendimiento

El indicador durante la partida muestra **Emulación %**: 100 % significa velocidad normal para la región del cartucho. **VI/s no son los FPS internos del juego**. La pausa (OPTIONS en N64; L3+R3 en los demás sistemas) muestra también tiempo del núcleo y presentación; las medidas se guardan cada diez segundos activos en `/data/R2N64/logs/r2n64.log`.

En Ajustes, X abre los detalles y otro X cambia la opción. Los ajustes se aplican a la próxima ROM y duran esta sesión:

| Opción | Predeterminado | Comparación disponible |
|---|---|---|
| Rendimiento | Cuatro hilos Angrylion | Un hilo |
| CPU de Nintendo 64 | Automática: recompilador si la comprobación permite ejecutarlo | Intérprete con caché |
| Procesamiento de audio | Acelerado (HLE) para tareas reconocidas | Original (LLE) |
| Medir rendimiento | Desactivado | Tiempos separados del núcleo en pausa y log |

OPTIONS muestra la **CPU realmente activa**, el número de trabajadores RDP y el contador de tareas de audio HLE procesadas. Seleccionar Automática no garantiza que se use el recompilador: si se rechaza el permiso de ejecución de su caché, se conserva el intérprete. El contador HLE indica trabajo efectivamente realizado, no el porcentaje de aceleración. El diagnóstico incluido genera audio directamente por AI y no necesita tareas RSP de audio, por lo que puede mantener ese contador en cero.

Comparar la misma escena durante al menos diez segundos, anotando CPU activa, Emulación %, tiempos y tareas HLE. Cambiar una opción por vez y reiniciar la ROM; si cambia el sonido, comparar con Procesamiento de audio → Original. El XMB recupera VSync al volver; durante la partida puede aparecer tearing al evitar esa espera. Se conservan los guardados y los PKG anteriores.

Para localizar el cuello de botella, activar **Ajustes → Medir rendimiento** con dos pulsaciones de X, iniciar el juego y abrir OPTIONS después de al menos treinta segundos en la escena elegida. El perfil muestra promedios desde el inicio de la sesión, en ms por VI: RSP, RDP, salida de vídeo, audio HLE y CPU/resto. Los componentes no se cuentan dos veces cuando una tarea invoca otra. El tiempo RDP incluye las esperas de sus trabajadores; no es la suma de uso de CPU de todos los hilos. «CPU y resto» incluye R4300, servicios, callbacks e instrumentación; no mide solamente el R4300. El audio original LLE se contabiliza dentro del RSP. Desactivar la medición y reiniciar la ROM para comparar velocidad sin su coste.

## Comprobar la ruta GPU

Abrir **Acerca de → Prueba GPU** y pulsar X. La prueba se ejecuta únicamente a petición, sin una ROM activa. Comprueba un contexto GLES2 aislado, el compilador GLSL, dos shaders originales, el enlace, un framebuffer de 8 × 8 y sus 64 píxeles; después restaura el contexto SDL. X o Círculo vuelve al XMB. No descarga assets ni añade shaders a la sesión normal.

El resultado se guarda en `/data/R2N64/logs/gpu-probe.txt` y en el log normal. `startup.log` registra cada etapa antes de las llamadas al driver. Si el puerto no permite un segundo contexto, el resultado es **prueba no completada**, no una conclusión sobre la capacidad de rasterización de la GPU. Un resultado correcto solo valida este programa mínimo: la compatibilidad y velocidad de GLideN64 deben comprobarse con cada juego y en PS4 física.

Para continuar el desarrollo se necesitan el resultado de esta prueba y las pantallas de pausa de Mario/Zelda con medición activada, incluyendo la CPU efectiva. La prueba desktop Mesa no valida Piglet en consola.

## Usar una ROM propia

Copiar un archivo `.nes`, `.sfc`, `.smc`, `.gb`, `.gbc`, `.gba`, `.z64`, `.v64` o `.n64` a `/data/R2N64/roms/`, o a `R2N64/roms/` en un USB montado por la consola. También se admiten las subcarpetas `nes/`, `snes/`, `gb/`, `gbc/`, `gba/` y `n64/`. En **Biblioteca**, pulsar **Triángulo** para escanear, seleccionar la ROM, pulsar **X** para ver sus datos y pulsar **X** nuevamente para iniciar.

El scanner lee la raíz de `/data/R2N64/roms`, `/mnt/usb0/R2N64/roms` y `/mnt/usb1/R2N64/roms`, y sus seis subcarpetas de sistema inmediatas. Admite extensiones en mayúsculas; no recorre otras carpetas ni extrae ZIP. Valida la cabecera y los límites propios de cada sistema; una cabecera válida no garantiza que el contenido sea ejecutable.

Los guardados nativos se restauran al cargar y se escriben al cerrar en `/data/R2N64/saves/<sistema>/*.srm`, con RTC separado cuando el núcleo lo expone. Los antiguos guardados N64 planos se leen como respaldo y se conservan intactos. Se usa escritura mediante archivo temporal; un guardado incompatible se conserva sin sobrescribir. Los estados ofrecen cinco espacios por juego GB/GBC/GBA/NES/SNES y conservan el espacio original de N64 en `states/<sistema>/`; comprueban identidad del juego, versión del núcleo e integridad antes de restaurar. El menú oculta estados no soportados. Cargar un estado GB/GBC/GBA/NES/SNES también restaura su SRAM; al cerrar se persiste esa memoria restaurada. No se garantiza guardado tras un cierre inesperado; usa **Volver a la biblioteca** para cerrar correctamente. El USB se utiliza como origen de lectura de ROMs.

Desktop utiliza `runtime/`, o el directorio indicado mediante `R2N64_DATA`.

## Controles

Durante la emulación:

| Acción | DualShock 4 | Teclado desktop |
|---|---|---|
| A de N64 | Cruz | Z |
| B de N64 | Cuadrado | X |
| Z de N64 | L2 | A |
| L / R de N64 | L1 / R1 | Q / W |
| Cruceta N64 | Cruceta | Flechas |
| Stick N64 | Stick izquierdo | Teclado numérico 4 / 6 / 8 / 2 |
| Botones C | Stick derecho | J / L / I / K |
| Start de N64 | Pulsar panel táctil | Enter |
| Pausar / abrir menú | OPTIONS | Esc |

Para GB/GBC/GBA: Cruz=A, Cuadrado=B, cruceta=direcciones, OPTIONS=Start, panel táctil=Select, L1/R1=L/R en GBA. L3+R3 abre la pausa; en desktop se usan Z/X, flechas, Enter/Tab, Q/W y Esc. El menú ofrece **Continuar**, **Guardar/Cargar estado** cuando estén disponibles, **Reiniciar** y **Volver a la biblioteca**. N64 con audio HLE mantiene los estados desactivados porque ese estado auxiliar aún no se serializa. En PS4 se pausa al detectar la desconexión del mando. Por ahora se conecta un único mando y no hay vibración.

NES conserva Cruz=A y Cuadrado=B. SNES sigue la posición de los botones: Cruz=B,
Círculo=A, Cuadrado=Y, Triángulo=X y L1/R1=L/R. En desktop corresponden a
Z/C/X/S y Q/W. Ambos usan OPTIONS=Start, táctil=Select y L3+R3=pausa.

En el XMB:

| Acción | DualShock 4 | Teclado desktop |
|---|---|---|
| Cambiar categoría | Izquierda / derecha | Flechas izquierda / derecha |
| Seleccionar opción / ROM | Arriba / abajo | Flechas arriba / abajo |
| Abrir / confirmar | X | Enter |
| Buscar ROMs nuevamente | Triángulo | R |
| Filtrar sistema en Biblioteca | L1 / R1 | F / G |
| Descargar ficha / cancelar descarga | OPTIONS | M |
| Diagnóstico de almacenamiento/mando | Cuadrado | D |
| Volver | Círculo | Esc |
| Salir desde Biblioteca | Círculo dos veces | Esc dos veces |

## Perfil experimental

Mupen64Plus-Next está fijado en `12edd2c74a517ff86dfa8cfc71ad75e4c10486d5`. Se compila `NEW_DYNAREC` x64 y se comprueba el permiso de ejecución de su caché real antes de utilizarlo; la CPU vuelve al intérprete con caché si la comprobación falla. El modo manual Intérprete permite comparar resultados. Angrylion utiliza cuatro hilos (uno opcional). El puente de audio HLE reconoce tareas de síntesis compatibles; CXD4 SSE2 sigue procesando gráficos y audio desconocido. No se cambia al plugin RSP HLE completo.

El núcleo entrega video software XRGB8888, presentado por SDL/GLES2; el audio estéreo se convierte a 48 kHz para el dispositivo SDL. El parche del recompilador conserva la memoria compacta y alinea su caché a páginas de 16 KiB en PS4. La disponibilidad de memoria ejecutable en el firmware/GoldHEN del usuario sigue sin comprobarse.

Se mantienen las cuatro categorías XMB, las transiciones y tres niveles de contraste por sesión en Ajustes → Apariencia. El JPG se conserva íntegro y el contraste se aplica en el renderer.

Esta versión cubre carga, vídeo, audio, controles, guardados y cambio entre sistemas. Quedan pendientes la validación física, rewind, remapeo configurable, shaders, favoritos/recientes, más mandos y configuración por juego. Las carátulas/fichas ya están integradas desde v0.4.1. Las preferencias se guardan en `configs/systems/{gb,gbc,gba,nes,snes}.json`; una configuración inválida conserva su archivo y utiliza valores iniciales. Solo se integra un núcleo por sistema; no hay selector manual de alternativas. El diagnóstico Linux y la compilación OpenOrbis no sustituyen las pruebas en PS4.

## Compilar

Inicializar una vez el submódulo fijado:

```powershell
git submodule update --init
```

Desde PowerShell en la raíz del proyecto:

```powershell
.\scripts\build.bat          # Núcleo PS4, aplicación y PKG
.\scripts\build.bat test     # Núcleo Linux, frontend y pruebas
```

Desde WSL/Linux:

```bash
bash scripts/build.sh ps4
bash scripts/build.sh test
./build/desktop/r2n64
```

El wrapper de Windows usa `Ubuntu-24.04`. PS4 requiere la distribución PacBrew de OpenOrbis existente en `/opt/pacbrew/ps4/openorbis`; puede indicarse otra ruta mediante `OO_PS4_TOOLCHAIN`. Desktop usa CMake, compilador C++17, Python 3, pkg-config, SDL2, SDL2_ttf, SDL2_image, libGL, libpng y zlib del entorno instalado. Desde v0.2.2 también se requiere **NASM** en el PATH de WSL para ensamblar el backend x64 del núcleo, tanto Linux como PS4. Es una dependencia adicional del build; se conserva la toolchain OpenOrbis/PacBrew existente. Los scripts comprueban su presencia y no lo instalan automáticamente.

Los scripts compilan el núcleo e incluyen el diagnóstico generado desde `scripts/make_diagnostic_rom.py`. Las fuentes se obtienen del commit fijado con `git archive`; los parches locales se aplican solamente en `build/`, sin alterar el submódulo. No hay descargas automáticas durante el build. `R2N64_BUILD_JOBS` controla el paralelismo del núcleo; el valor predeterminado es ocho.

| Salida | Descripción |
|---|---|
| `build/ps4/R2RETRO.elf` | Ejecutable PS4 con el núcleo enlazado |
| `build/ps4/eboot.bin` | SELF generado por OpenOrbis |
| `dist/R2RETRO-v0.5.5-display-network.pkg` | PKG multisistema con GPU N64 y HLE gráfico opcionales |
| `dist/R2RETRO-v0.5.5-display-network.pkg.sha256` | SHA-256 que genera el empaquetado |
| `dist/build-info.json` | Metadatos y estado de validación |
| `build/desktop/r2n64` | Aplicación Linux/WSL |
| `build/desktop/emulation-preview.png` | Captura de la prueba integrada GLES2 |

Identidad: título **R2RETRO**, Title ID **RNTD00064**, Content ID `IV0001-RNTD00064_00-R2N64APP00000001`. El empaquetado v0.5.5 configura SFO `00.55`; no expresa una versión mínima de firmware verificada. Se conserva la identidad de instalación y datos de los paquetes anteriores.

## Pruebas reproducibles y logs

El banco optativo `scripts/run-rom-matrix.py` ejecuta una ROM local con controles sintéticos, datos aislados y métricas por perfil. No incluye ROMs ni las añade a CTest. Resultados con el archivo de Mario proporcionado por el usuario, límites y comandos: [docs/LOCAL-ROM-TESTS.md](LOCAL-ROM-TESTS.md).

Zelda OoT U V1.2 completó 16 sesiones del banco Release con configuración del
núcleo equivalente a la del PKG. Resultados, diferencias HLE/LLE y límites en
[ZELDA-TESTS.md](ZELDA-TESTS.md); contraste con PS4 original/Slim en
[PS4-ZELDA-CHECK.md](PS4-ZELDA-CHECK.md).

```bash
bash scripts/build.sh test
bash scripts/build-core.sh
bash scripts/check-audio-hle.sh
SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=opengles2 LIBGL_ALWAYS_SOFTWARE=1 \
  SDL_AUDIODRIVER=dummy R2N64_DATA=build/desktop/emulation-smoke-data \
  ./build/desktop/r2n64 --emulation-smoke --accelerated \
  --screenshot build/desktop/emulation-preview.png
```

`--emulation-smoke` realiza dos sesiones completas con el diagnóstico incluido y vuelve al frontend. El audio dummy verifica el flujo de datos, sin reproducir sonido físico. La prueba GLES2 de Mesa no ejecuta Piglet. El laboratorio adicional verifica instrucciones MIPS, bandas RGB, PCM, SI/PIF y restauración de estado en memoria. La interfaz permite guardar y cargar estados cuando el núcleo activo los admite.

Se conserva el orden de arranque confirmado en v0.1.2: SDL, ventana, renderer GLES2 y primer cuadro antes del acceso GoldHEN; SDL administra `USER_SERVICE` y `scePadInit` ocurre después de SDL video. GoldHEN permite acceder a los directorios propios en `/data`.

Ante un fallo, conservar `/data/R2N64/startup.log` o `/download0/R2N64-startup.log`, y `/data/R2N64/logs/r2n64.log` si existe. Indicar si ocurrió al abrir la app, iniciar una ROM, pausar o regresar al XMB. El usuario indicó PS4 original o Slim como referencia; firmware y versión de GoldHEN siguen pendientes.

Estado: [docs/STATUS.md](STATUS.md). Núcleo y evidencia: [docs/CORE-LAB.md](CORE-LAB.md). Plan del usuario: [docs/PROJECT-BRIEF.md](PROJECT-BRIEF.md). Dependencias: [docs/THIRD-PARTY.md](THIRD-PARTY.md).

## Licencia

Código propio: GPL-3.0-or-later, en `LICENSE`. Las dependencias conservan sus licencias y avisos. No se incluyen ROMs comerciales; el único cartucho incluido es el diagnóstico original generado desde su código fuente.
