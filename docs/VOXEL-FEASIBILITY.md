# Evaluación de DramaticShapeVoxelMod para R2N64

Revisión del 5 de octubre de 2026. Estado: laboratorio nativo con Pueblo
Paleta, las dos plantas de la casa de Red y el laboratorio de Oak, texturas,
sprites animados, objetos 3D, cámara de seguimiento y ventanas de diálogo/menú
sobre 3D. F1 alterna 3D/2D y recuerda la preferencia al salir. Probado con
SameBoy en Linux; no integrado en el PKG. La aplicación normal avanzó después
a v0.3.2 con marcos GB/GBC/GBA; ese paquete tampoco incluye este laboratorio 3D.
La sección «Personajes, entornos y vista opcional» documenta el estado más reciente; los hitos
anteriores se conservan como referencia de la evolución y sus mediciones.

## Qué ofrece el proyecto enlazado

[DramaticShapeVoxelMod](https://github.com/absol89/DramaticShapeVoxelMod)
construye escenarios tridimensionales para Pokémon Red, Blue y Yellow dentro
del motor Gen1Recomp. El motor anfitrión es una recreación en Lua/LÖVE que
importa datos de una ROM del usuario; no utiliza la emulación SameBoy/mGBA
de R2N64. Fuente: [Gen1Recomp](https://github.com/bryanthaboi/gen1recomp).

Se consultó la revisión `303b522867f333b87cbfe9974ec078a6f45184bb`. Su
[manifest](https://github.com/absol89/DramaticShapeVoxelMod/blob/303b522867f333b87cbfe9974ec078a6f45184bb/manifest.json)
declara versión 1.11.1, entrada `main.lua`, `games: ["gen1"]` y acceso
`engine_internals`. El README conserva referencias a una versión anterior;
no debe usarse para afirmar soporte de generaciones adicionales.

## Diferencia con el renderer actual

En R2N64, `core/core_interface.h::CoreFrame` contiene píxeles RGB y dimensiones.
`core/libretro_core.cpp::State::video` recibe la imagen final del núcleo.
`src/video.cpp::Video::gameFrame` la sube a una textura y la presenta con SDL.
Este contrato no entrega mapas del mundo, alturas, objetos, colisiones,
posiciones de personajes ni capas separadas de texto e imagen.

El [punto de entrada del mod](https://github.com/absol89/DramaticShapeVoxelMod/blob/303b522867f333b87cbfe9974ec078a6f45184bb/main.lua)
registra un renderer de mundo y espera objetos `state.map`, `state.camera` y
`state.player`, además de eventos del motor. Su
[renderer](https://github.com/absol89/DramaticShapeVoxelMod/blob/303b522867f333b87cbfe9974ec078a6f45184bb/lib/Voxel3D.lua)
usa las API de shaders, mallas y superficies con profundidad de LÖVE.
Por ello no se puede incorporar como una opción de filtrado de la textura
existente: faltan tanto el contrato de datos como el entorno de ejecución.

La [guía del propio mod](https://github.com/absol89/DramaticShapeVoxelMod/blob/303b522867f333b87cbfe9974ec078a6f45184bb/docs/GEN1_GEN2_DIFFERENCES.md)
explica que incluso Pokémon Gold necesita adaptaciones específicas para
mapas, movimiento, batallas y menús. No hay soporte general GB/GBC/GBA.

## Dos alcances distintos

| Resultado buscado | Trabajo necesario | Límite |
|---|---|---|
| Mundo 3D con edificios y cámara | Portar el entorno Gen1Recomp/LÖVE y el mod, o crear un adaptador de datos por juego y un renderer propio | Empezar con una revisión concreta de Pokémon Red; no extender automáticamente a otros juegos |
| Relieve visual sobre el cuadro 2D | Desarrollar un efecto original aplicado a la textura de salida | No recupera geometría oculta, edificios reales ni una cámara libre del mundo |

El usuario eligió **investigar 3D real empezando por Pokémon Red**. Se creó
un laboratorio nativo aislado que comprueba extracción del mapa y geometría
propia. Un efecto de relieve no debe presentarse como una integración de
DramaticShapeVoxelMod.

Para una adaptación 3D, el primer hito es un mapa estático con profundidad
y cámara limitada, con pruebas originales y extracción de datos de la ROM
autorizada en una herramienta aislada. El paso siguiente sería sincronizar
jugador y mapas conservando la emulación y los controles.
Portar Gen1Recomp completo sería otra arquitectura, con su propia validación
de partidas y comportamiento; no sustituir SameBoy sin elegir ese alcance.

## Condiciones de integración

- Verificar primero shaders y profundidad en Piglet/PS4 con una prueba manual;
  la prueba GLES2 de escritorio no confirma esas capacidades en consola.
- Medir tiempos de núcleo y efecto por separado; no atribuir al mod una
  mejora de velocidad. La geometría y los pases adicionales añaden trabajo.
- Mantener el efecto opcional, con salida 2D cuando falten datos o recursos,
  y preservar arranque, XMB, N64, guardados y preferencias v0.3.1.
- La revisión del árbol y metadatos de GitHub no identificó un archivo de
  licencia del mod. Aclarar los términos de reutilización antes de incorporar
  código o assets de terceros. No se han importado fuentes ni assets del mod.
- No distribuir ROMs ni gráficos comerciales. Utilizar patrones originales
  para las pruebas automáticas y la ROM autorizada solo de forma local.

No se han medido rendimiento ni compatibilidad de este mod en PS4, y no se
ha generado un PKG nuevo para esta evaluación.

## Prototipo comprobado

`scripts/pokemon_red_map.py` lee la ROM autorizada desde Downloads y exige
identificación completa de la revisión: SHA-1
`ea9bcae617fdf159b045185467ae58b2e4a48b9a` y SHA-256
`5ca7ba01642a3b27b0cc0b5349b52792795b62d3ed977e98a09390659af96b7b`.
La ROM permanece intacta. Los formatos se contrastaron con
[pret/pokered](https://github.com/pret/pokered/tree/d2704a63c26f9ba046ade877445216b3de0519a4).
Los [símbolos fijados](https://raw.githubusercontent.com/pret/pokered/3f618d59edf43918f48f5e558c34e04cb2fc5619/pokered.sym)
también se verifican por hash y se guardan solamente en `build/`.

Se decodificó Pueblo Paleta: 10 × 9 bloques, 40 × 36 tiles, 320 × 288 píxeles,
50 tiles diferentes, tres objetos, tres salidas y cuatro carteles. El JSON
contiene sus datos estáticos y procedencia. No evalúa los flags que determinan
si un personaje debe aparecer durante la partida.

`scripts/pokemon_red_mesh.py` aplica un perfil propio de materiales y alturas.
Las dos casas y el laboratorio tienen volumen, techo de dos pendientes,
fachada vertical y laterales. La imagen del techo y fachada se obtiene del
mapa autorizado; las alturas, tintes y caras que no aparecen en el juego son
decisiones artísticas propias. El resto del mapa conserva la distribución
decodificada de suelo, agua, cercas, carteles y flores. No es una extrusión
por brillo del framebuffer ni geometría 3D que existiera dentro de la ROM.

`tools/pokemon3d/` contiene el visor C++/SDL2/GLES2 independiente. Subida de
malla y compilación de shaders una sola vez; una llamada de dibujo por vista.
Tiene cámara orbitable y prueba de profundidad, sin leer la GPU cada cuadro
en el modo interactivo. Para este primer prototipo el color de cada zona de
píxeles se representa mediante vértices; un atlas de texturas reduciría la
geometría en una integración posterior.

| Medición del mapa estático | Resultado |
|---|---:|
| Edificios | 3 |
| Vértices | 244.176 |
| Triángulos | 81.392 |
| VBO | 5.860.224 bytes |
| Profundidad en Mesa/GLES2 | 24 bits |

Capturas inspeccionadas: `build/pokemon-red-3d/pallet-3d-front.png` y
`pallet-3d-side.png`. Ambas muestran profundidad y edificios desde ángulos
distintos. La vista 2D de referencia es `pallet-topdown.png`, junto con
`pallet-tiles.png`, el JSON decodificado y `pallet-town.report.json`.
Los datos derivados del cartucho quedan en `build/`, ignorados por Git,
fuera de los assets de la aplicación y del PKG.

### Pruebas realizadas

- Ocho pruebas Python del extractor con cabeceras, gráficos y eventos
  sintéticos originales: orden de bits, bancos, límites, dimensiones y
  registros variables. No requieren la ROM comercial.
- Diez pruebas Python de geometría: alturas por identidad del tile,
  paredes compartidas, agua, edificios con techo inclinado y frente vertical,
  determinismo, entradas inválidas y límite de vértices.
- Cargador nativo: payload LE válido y quince entradas inválidas/ausentes.
  La validación también pasó ASan/UBSan.
- Renderer nativo: invertir el orden de triángulos superpuestos conserva la
  imagen correcta; cambiar el ángulo modifica 85.231 píxeles de la escena
  sintética. Cámara inválida rechazada.
- Cuatro grupos CTest aprobados. Registro reproducible en
  `build/pokemon3d/lab-tests.log`.
- `scene.cpp` y `renderer.cpp` compilaron como objetos OpenOrbis con `-Werror`.
  Esto no constituye enlace, arranque ni validación gráfica en PS4.

No se modificaron la aplicación ni el empaquetado. El SHA-256 de v0.3.1 sigue
siendo `df669d30c24b100362cf8e3364f002dd1cdd81d2fa1d837279050451148bfb52`.

### Reproducir en WSL

Desde la raíz del repositorio, con la ROM propia y los símbolos indicados:

```sh
cmake -S tools/pokemon3d -B build/pokemon3d -DCMAKE_BUILD_TYPE=Release
cmake --build build/pokemon3d --parallel 2
ctest --test-dir build/pokemon3d --output-on-failure

# ROM debe apuntar al archivo propio fuera del repositorio.
python3 scripts/pokemon_red_map.py --rom "$ROM" \
  --symbols build/pokemon-red-3d/pokered.sym \
  --output build/pokemon-red-3d/pallet-town.json
python3 scripts/pokemon_red_mesh.py \
  --map build/pokemon-red-3d/pallet-town.json \
  --profile tools/pokemon3d/pallet_height_profile.json \
  --output build/pokemon-red-3d/pallet-town.r2scene
env LIBGL_ALWAYS_SOFTWARE=1 MESA_GLES_VERSION_OVERRIDE=2.0 \
  build/pokemon3d/pokemon3d \
  --scene build/pokemon-red-3d/pallet-town.r2scene \
  --screenshot build/pokemon-red-3d/pallet-3d-front.png \
  --yaw 25 --pitch 50 --zoom 1.25
```

La captura no necesita una ventana visible. Para exploración interactiva,
usar `--interactive` en una sesión SDL con pantalla: flechas rotan/elevan,
rueda cambia zoom y Escape cierra. `--png` y `--atlas` del extractor son
opcionales y requieren Pillow; el JSON y las pruebas no lo necesitan.

## Partida viva: primer adaptador — hito anterior

`tools/pokemon3d/live.cpp` reutiliza `Emulator` y el SameBoy existentes. Exige
la revisión exacta por tamaño y SHA-1 antes de cargar. Después de cada cuadro
vuelve a solicitar `RETRO_MEMORY_SYSTEM_RAM` y copia sus 8 KiB; no conserva
punteros del núcleo ni modifica ROM, memoria, colisiones o eventos del juego.
Todos los guardados y registros del laboratorio quedan dentro de `build/`.

`red_state.cpp` interpreta mapa, coordenadas y slots visibles de personajes.
Las direcciones se contrastaron con los símbolos fijados y la
[definición de WRAM](https://github.com/pret/pokered/blob/d2704a63c26f9ba046ade877445216b3de0519a4/ram/wram.asm).
El adaptador verifica dimensiones, tileset, puntero y número de objetos de
Pueblo Paleta; otros mapas, texto/menús, batalla o estado incompleto muestran
la imagen 2D original. Exige dos muestras consecutivas válidas al volver a
3D para descartar el cuadro transitorio con coordenadas anteriores.

El jugador aparece como marcador azul y los NPC visibles como marcadores
naranjas. Son prismas de diagnóstico, todavía sin modelos ni animaciones de
personaje, y avanzan por coordenadas de casilla sin interpolación. Los NPC
fuera de la pantalla original se omiten. La imagen Game Boy se conserva en
una esquina durante el 3D y ocupa la vista al entrar en modo 2D. Los shaders
y la malla estática se reutilizan; solo los marcadores y la textura de la
imagen original se actualizan durante el dibujo.

### Validación local del adaptador

`scripts/pokemon-red-live-smoke.py` crea una sesión nueva y recorre la
introducción, dormitorio, casa y Pueblo Paleta mediante entradas del mando
simulado. Lee la ROM autorizada de Downloads; no la copia ni distribuye.

Prueba reproducida: `build/red-live-smoke-q9n2jiau/summary.json`.

| Comprobación | Resultado |
|---|---:|
| Cuadros de las cuatro fases | 16.420 |
| Recorrido final | 1.020 cuadros |
| Posiciones diferentes en Pueblo Paleta, recorrido final | 11 |
| Cuadros 3D / 2D, recorrido final | 748 / 272 |
| Entrada y salida de la casa y menú | Aprobadas |
| WRAM idéntica tras carga, reset+carga y reapertura+carga | 3 comprobaciones aprobadas |
| SHA-256 de la ROM antes/después | Idéntico |

Se corrigió una suposición detectada con la ROM: el estado de movimiento
del jugador puede ser cero durante juego normal; no usa la misma condición
de disponibilidad que los NPC. También se corrigió el cuadro transitorio de
entrada al mapa descrito arriba.

Cinco grupos CTest aprobados: extractor, malla, lector de estado, cargador y
renderer. Las pruebas sintéticas cubren identidad de ROM, WRAM incompleta,
límites, NPC ocultos, menú/batalla, transición estable, orientación/color del
cuadro 2D, restauración del estado GLES después del fallback y desaparición
de marcadores antiguos. `renderer.cpp` y `red_state.cpp` también compilaron
como objetos OpenOrbis con `-Werror`; no se enlazó ni ejecutó este laboratorio
en PS4. Las cifras de cuadros son cobertura funcional, no medidas de velocidad
de consola. El audio interactivo tampoco se ha validado físicamente.

Captura inspeccionada:
`build/red-live-smoke-q9n2jiau/session/run-1791218345446662/pallet-live-1019.png`.

### Ejecutar el laboratorio conectado

Desde la raíz en WSL, con los núcleos existentes ya compilados y la escena
generada con los comandos anteriores:

```sh
cmake -S . -B build/ps4-parity-linux -DCMAKE_BUILD_TYPE=Release \
  -DR2N64_POKEMON3D_LAB=ON
cmake --build build/ps4-parity-linux --target pokemon3d_live \
  pokemon3d_tests pokemon3d_red_state_tests --parallel 2
ctest --test-dir build/ps4-parity-linux/tools/pokemon3d --output-on-failure
python3 scripts/pokemon-red-live-smoke.py --rom "$ROM" \
  --scene build/pokemon-red-3d/pallet-town.r2scene \
  --exe build/ps4-parity-linux/tools/pokemon3d/pokemon3d_live
```

El script devuelve su directorio único de evidencia. Para continuar esa
partida en una sesión con pantalla, pasar su subdirectorio `session` como
`--data`, además de `--rom`, `--scene`, `--resume --interactive --frames 36000`
al ejecutable `pokemon3d_live`. Flechas mueven al jugador, Z=A, X=B,
Enter=Start, Tab=Select, Q/E orbitan la cámara y Escape cierra. El laboratorio
interactivo usa teclado, sin integración de DS4. Al salir guarda un estado
aislado; `--no-save` evita escribir ese checkpoint, aunque el núcleo sigue
guardando SRAM/RTC dentro de los datos de prueba. La opción CMake está apagada
por defecto y se rechaza al compilar para PS4.

## Ampliación visual — interiores, sprites y texturas

Implementada la referencia visual del usuario para un recorrido concreto:
Pueblo Paleta (mapa 0), casa de Red 1F (37) y dormitorio 2F (38). No se ha
portado el mod: el renderer y los perfiles geométricos son propios.

- `scripts/pokemon_red_world.py` extrae los tres mapas de la revisión exacta,
  combina sus píxeles en atlas y exporta superficies texturizadas. El perfil
  `tools/pokemon3d/world_profile.json` define mobiliario y alturas artísticas.
  Hay paredes posteriores, escaleras, mesas, sillas, armarios, cama y objetos;
  exteriores con fachadas, ventanas, puertas, aleros y chimeneas.
- `R2SCENE2` contiene vértices xyz/rgb/uv y RGBA; el cargador mantiene
  compatibilidad con `R2SCENE1` y rechaza tamaños, UV, payload o valores
  inválidos. Los archivos derivados siguen confinados a `build/`.
- El atlas de personajes se lee directamente de la ROM verificada: 72
  identidades, seis imágenes por identidad y transparencia. Los personajes
  son sprites orientados hacia la cámara, no modelos volumétricos. Dirección,
  fase, espejo y desplazamiento entre casillas proceden de WRAM. No se
  extrapola el movimiento usando el reloj del renderer. La paleta es propia.
- La cámara sigue suavemente al jugador y limita el desplazamiento dentro
  de las habitaciones. Un recorte tramado localizado reduce la ocultación
  por geometría elevada entre cámara y jugador. No cambia colisiones.
- Sombreado por cara y sombras sencillas de contacto; sin shadow maps ni
  iluminación física. Los pies y sombras se apoyan visualmente sobre
  superficies bajas de la malla, corrigiendo la ocultación por sillas y
  escaleras sin alterar la posición que conserva SameBoy.
- El adaptador reconoce ventanas completas en `wTileMap` y recorta sus
  píxeles de la imagen original: diálogo abajo, menú al costado. Espera
  brevemente la transferencia de tiles a VRAM antes de presentarlas. Si una
  ventana no puede identificarse conserva un panel con la imagen original.
  La detección se basa en la estructura de
  [TextBoxBorder](https://github.com/pret/pokered/blob/d2704a63c26f9ba046ade877445216b3de0519a4/home/text.asm).
- Escenas y sprites se cargan antes de emular; no se leen assets en el bucle.
  La malla se sube al cambiar de mapa y se reutiliza durante sus cuadros.
  La telemetría se acumula en memoria. El modo interactivo usa el reloj
  fraccional del emulador sin una segunda espera de vsync.

### Coste de geometría y memoria

| Escena | Triángulos | VBO | Textura RGBA | Archivo |
|---|---:|---:|---:|---:|
| Paleta anterior, colores en geometría | 81.392 | 5.860.224 B | — | 5.860.236 B |
| Paleta texturizada | 3.700 | 355.200 B | 1.048.576 B | 1.403.796 B |
| Casa 1F | 688 | 66.048 B | 262.144 B | 328.212 B |
| Casa 2F | 516 | 49.536 B | 262.144 B | 311.700 B |

Reducción de triángulos de Paleta: **95,45 %**. Los atlas del mundo son 512²
y 256²; el de sprites ocupa 96 × 1152 RGBA. Se consulta el límite de textura
del contexto GLES2. Estas cifras no equivalen a una mejora medida de FPS ni
validan velocidad en PS4. Informe: `build/pokemon-red-world/world-report.json`.

### Validación de esta ampliación

Seis grupos CTest aprobados, incluyendo 25 casos Python originales. Cubren
mapas interiores, topología/UV, límites del formato, texturas, transparencia,
espejo, seguimiento, apoyo sobre muebles, recorte de oclusión, ventanas de
diálogo/menú y restauración del estado GLES después de dibujar UI.

Recorrido optativo final con la ROM del usuario:
`build/red-live-smoke-6us62e9s/summary.json`. **16.776 cuadros** desde partida
nueva, pasando por ambas plantas, conversación con la madre, salida a
Paleta, vuelta a la casa y menú. Las cuatro fases posteriores a la introducción
dibujan cada cuadro (2.776); la introducción emula todos sus cuadros y captura
solo puntos seleccionados. Se comprobaron seis imágenes de animación del
jugador y 82 posiciones en píxeles frente a 12 posiciones de casilla en el
recorrido final. Tres comprobaciones de WRAM idéntica tras cargar, reiniciar
y reabrir la sesión con su estado. SHA-256 de la ROM conservado.

Capturas finales inspeccionadas dentro de ese directorio:

- `session/run-1791220151602284/world-0-1019.png`: exterior y personajes.
- `session/run-1791220147602766/world-37-899.png`: interior y escaleras.
- `session/run-1791220149229395/overlay-37-355.png`: conversación con la madre.
- `session/run-1791220151602284/overlay-0-680.png`: menú original sobre 3D.

`scene.cpp`, `red_state.cpp` y `renderer.cpp` compilaron como objetos OpenOrbis
con `-Werror`. Eso no es enlace ni ejecución en PS4. El SHA-256 del PKG v0.3.1
sigue siendo `df669d30c24b100362cf8e3364f002dd1cdd81d2fa1d837279050451148bfb52`.

### Reproducir la ampliación

Con la toolchain y los núcleos locales existentes, desde WSL:

```sh
python3 scripts/pokemon_red_world.py --rom "$ROM" \
  --symbols build/pokemon-red-3d/pokered.sym \
  --output-dir build/pokemon-red-world
cmake -S . -B build/ps4-parity-linux -DCMAKE_BUILD_TYPE=Release \
  -DR2N64_POKEMON3D_LAB=ON
cmake --build build/ps4-parity-linux --target pokemon3d_live \
  pokemon3d_tests pokemon3d_red_state_tests --parallel 2
ctest --test-dir build/ps4-parity-linux/tools/pokemon3d --output-on-failure
python3 scripts/pokemon-red-live-smoke.py --rom "$ROM" \
  --world build/pokemon-red-world \
  --exe build/ps4-parity-linux/tools/pokemon3d/pokemon3d_live
```

Para continuar una sesión de prueba, usar el subdirectorio `session` del
resultado como `--data` del ejecutable, con `--rom`, `--world`, `--resume
--interactive --frames 36000`. Flechas mueven a Red; Z/X=A/B; Enter=Start;
Tab=Select; Q/E orbitan; W/S acercan/alejan; Escape sale y guarda el checkpoint
aislado. El estado final del recorrido deja a Red en Pueblo Paleta.

## Personajes, entornos y vista opcional

Mejora posterior a la captura del laboratorio de Oak aportada por el usuario.
Se mantienen el renderer propio y SameBoy como autoridad de la partida.

- Paletas por identidad, zona corporal y dirección: Red con gorra y ropa
  rojas, rival de azul, Oak con bata clara y madre de lila. Pelo, piel y ropa
  tienen tonos diferenciados. Se preservan silueta y transparencia del
  cartucho; los colores son decisiones artísticas. El renderer añade un
  sombreado suave dentro del contorno y sombras de contacto más discretas.
- El mapa 40 añade el laboratorio de Oak: suelo de tablas, estanterías,
  ordenadores, pantallas, mostradores y mesa de los iniciales. La cabecera
  se valida contra la revisión exacta: 5 × 6 bloques, tileset 5, puntero
  `0x41C0`, once objetos. Las Poké Balls usan una malla esférica propia de
  432 triángulos, compartida y almacenada una sola vez en GPU (41.472 B).
  Su presencia procede del actor 61 de SameBoy; no están horneadas en el
  mapa. Los Pokédex también permanecen como objetos dinámicos.
- NPC y objetos fuera del encuadre Game Boy pueden aparecer en la cámara
  3D cuando WRAM y las tablas de visibilidad son válidas. Se contrastan
  `wMissableObjectList` (`D5CE`) y sus flags (`D5A6`) con los objetos esperados
  de cada mapa. No se inventan NPC a partir de posiciones estáticas ni se
  muestran objetos retirados por eventos; se descartan listas incompletas.
- Casas con frontón frontal, buhardilla, marcos y maceteros; laboratorio
  exterior con azotea y claraboyas. Interiores con suelos más neutros,
  escaleras, cama y planta revisadas. Corregida la imagen del ordenador
  duplicada en la pared del dormitorio.

### Activar y desactivar

En `pokemon3d_live --interactive`, **F1** cambia entre el escenario 3D y la
pantalla original 2D. No recarga la ROM ni reinicia SameBoy. El título de la
ventana indica la opción activa. En mapas o estados no soportados se sigue
usando 2D aunque la opción esté activada.

La preferencia se guarda al salir en
`<data>/configs/pokemon3d-view.txt`, dentro de los datos aislados de `build/`.
La sustitución del archivo es atómica y se realiza fuera del bucle de
emulación. Sin preferencia se inicia en 3D; un archivo inválido produce un
diagnóstico y usa ese valor por defecto. `--view 2d` o `--view 3d` permite
elegir explícitamente la vista inicial y actualizar la preferencia al salir.
`--no-save` evita el checkpoint de la partida, no esta preferencia visual.
Para pruebas programadas, `1 TOGGLE3D` equivale a una pulsación sin enviar
ningún botón al juego. F1 pertenece al laboratorio de teclado; todavía no
es una opción del XMB ni un botón del mando PS4.

### Geometría actual

| Escena | Triángulos estáticos | VBO | Atlas RGBA | Archivo |
|---|---:|---:|---:|---:|
| Pueblo Paleta | 3.910 | 375.360 B | 1.048.576 B | 1.423.956 B |
| Casa 1F | 980 | 94.080 B | 262.144 B | 356.244 B |
| Casa 2F | 740 | 71.040 B | 262.144 B | 333.204 B |
| Laboratorio de Oak | 1.300 | 124.800 B | 262.144 B | 386.964 B |

Informe: `build/pokemon-red-world-style2/world-report.json`. La geometría de
Paleta sigue siendo un 95,20 % menor que el primer prototipo de 81.392
triángulos. Esto mide geometría, no una mejora de FPS. Los objetos dinámicos
y personajes se dibujan aparte y reutilizan sus recursos.

### Validación final

- **7/7 grupos CTest aprobados**, con 27 casos Python originales. Nuevos
  casos de mapa 40, paletas, visibilidad fuera de pantalla, objetos retirados,
  profundidad/color/desaparición de la esfera y persistencia de la vista.
- Prueba optativa del atlas contra la ROM: las 72 identidades × seis cuadros
  conservan exactamente el alfa y la silueta originales.
- `build/red-live-smoke-ibm15zia/summary.json`: **22.844 cuadros** desde partida
  nueva, ambas plantas, conversación con la madre, menú, Paleta, evento de
  Oak y llegada al laboratorio. Después de la introducción se dibujan todos
  los cuadros (8.844). Se comprueban seis restauraciones con WRAM idéntica:
  cargar, reiniciar+cargar y reabrir+cargar, en Paleta y en el laboratorio.
  El recorrido termina ante Oak con el diálogo cerrado.
- `build/red-view-smoke-d7dmj1qj/summary.json` (Paleta) y
  `build/red-view-smoke-__5nw_eb/summary.json` (Oak, compilación final): dos
  ejecuciones de 120 cuadros desde cada checkpoint, una fija en 3D y otra
  con tres alternancias.
  WRAM final e imagen original de SameBoy idénticas byte por byte. Se
  comprueban persistencia de 2D y sobreescritura mediante `--view 3d`.
- SHA-256 del cartucho intacto. El PKG v0.3.1 conserva su hash anterior.
  `red_state.cpp` y `renderer.cpp` compilan como objetos OpenOrbis con
  `-Werror`; esto no confirma enlace, funcionamiento gráfico ni velocidad
  del laboratorio en PS4. La ejecución verificada es Linux/Mesa/GLES2.

Capturas del recorrido final, revisadas visualmente:

- `build/red-live-smoke-ibm15zia/session/run-1791221499930973/world-40-2255.png`:
  laboratorio, Oak, rival, Red y tres Poké Balls.
- `build/red-live-smoke-ibm15zia/session/run-1791221485825467/world-0-1019.png`:
  fachada, exteriores y personajes.

Reproducción desde WSL con la ROM propia en `$ROM`:

```sh
python3 scripts/pokemon_red_world.py --rom "$ROM" \
  --symbols build/pokemon-red-3d/pokered.sym \
  --output-dir build/pokemon-red-world-style2
cmake -S . -B build/ps4-parity-linux -DCMAKE_BUILD_TYPE=Release \
  -DR2N64_POKEMON3D_LAB=ON
cmake --build build/ps4-parity-linux --target pokemon3d_live pokemon3d_tests \
  pokemon3d_red_state_tests pokemon3d_view_settings_tests --parallel 2
ctest --test-dir build/ps4-parity-linux/tools/pokemon3d --output-on-failure
python3 scripts/pokemon-red-live-smoke.py --rom "$ROM" \
  --world build/pokemon-red-world-style2 \
  --exe build/ps4-parity-linux/tools/pokemon3d/pokemon3d_live
python3 scripts/pokemon-red-view-smoke.py --rom "$ROM" \
  --world build/pokemon-red-world-style2 \
  --session build/red-live-smoke-ibm15zia/session \
  --exe build/ps4-parity-linux/tools/pokemon3d/pokemon3d_live
```

Para abrir el recorrido comprobado en una sesión con pantalla:

```sh
env LIBGL_ALWAYS_SOFTWARE=1 MESA_GLES_VERSION_OVERRIDE=2.0 \
  build/ps4-parity-linux/tools/pokemon3d/pokemon3d_live --rom "$ROM" \
  --world build/pokemon-red-world-style2 \
  --data build/red-live-smoke-ibm15zia/session \
  --resume --interactive --frames 36000
```

Flechas=mover, Z/X=A/B, Enter=Start, Tab=Select, Q/E=orbitar,
W/S=zoom, F1=3D/2D, Escape=salir y guardar en la sesión de prueba.

## Límites y siguiente hito

Esta versión cubre cuatro mapas de una única revisión de Pokémon Red caminando.
Otros mapas,
batallas, bicicleta y surf conservan la presentación 2D. Sombras, colores,
alturas y detalles arquitectónicos son una interpretación propia. No es
una reproducción exacta de los recursos o técnicas del mod de Windows.

Falta validar navegación interactiva prolongada/audio/DS4, más eventos y
mapas, y coste en hardware. Antes de
integrar el renderer en el PKG se debe comprobar profundidad, shaders y
transiciones en Piglet/PS4 física. No cambia Blue/Yellow, GBC ni GBA.
