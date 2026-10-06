# R2N64 PS4

## Emulador Nintendo 64 para PlayStation 4

**R2N64** será una aplicación homebrew para PlayStation 4 orientada a ejecutar software de Nintendo 64 mediante un frontend propio y un núcleo de emulación basado en **Mupen64Plus-Next**.

El objetivo no es desarrollar la emulación de Nintendo 64 desde cero, sino portar e integrar un núcleo maduro, creando alrededor de él una aplicación optimizada para PS4 con:

- Interfaz gráfica propia.
- Soporte DualShock 4.
- Biblioteca visual de juegos.
- Carátulas.
- Configuración individual por juego.
- Save States.
- SRAM.
- Resolución configurable.
- Estadísticas.
- Favoritos.
- Historial.
- Capturas de pantalla.
- Soporte USB.
- Soporte almacenamiento interno.
- `.PKG` instalable en PS4 homebrew.

---

# 1. Objetivo

Crear:

```text
R2N64.pkg
```

que aparezca como una aplicación independiente:

```text
PlayStation 4
      │
      ▼
┌─────────────────────────┐
│        R2N64            │
│                         │
│    Nintendo 64          │
│      Emulator           │
│                         │
│        START            │
└─────────────────────────┘
```

No depender visualmente de RetroArch.

La aplicación tendrá su propia interfaz, configuraciones y sistema de biblioteca.

---

# 2. Arquitectura general

```text
                  R2N64 PS4
                      │
         ┌────────────┴────────────┐
         │                         │
         ▼                         ▼
     Frontend                 Emulator Core
       R2N64                  Mupen64Plus
         │                         │
         │                         │
 ┌───────┼─────────┐        ┌──────┼──────┐
 │       │         │        │      │      │
 ▼       ▼         ▼        ▼      ▼      ▼
GUI    Input     Audio      CPU    RSP    RDP
 │       │         │
 ▼       ▼         ▼
SDL2     DS4      PS4
         │
         ▼
      Controller
```

---

# 3. Tecnologías

## Lenguaje principal

```text
C++
```

También habrá partes en:

```text
C
Python
Shell
Makefile
```

---

# 4. Toolchain

Utilizar:

```text
OpenOrbis PS4 Toolchain
```

OpenOrbis proporciona:

- compilador para PS4;
- headers;
- libc;
- libc++;
- linker;
- creación ELF;
- creación de `eboot.bin`;
- herramientas PKG;
- soporte SDL;
- ejemplos PS4.

---

# 5. Emulador

Utilizar:

```text
Mupen64Plus-Next
```

Preferiblemente a través de:

```text
libretro API
```

Arquitectura:

```text
R2N64
   │
   ▼
Libretro frontend
   │
   ▼
Mupen64Plus-Next
   │
   ├── R4300 CPU
   ├── RSP
   ├── RDP
   ├── Memory
   ├── Audio
   └── Controllers
```

---

# 6. Renderizado

Inicialmente priorizar:

```text
Angrylion
```

o el renderer que esté probado y estable en PS4.

Posteriormente investigar:

```text
GLideN64
ParaLLEl-RDP
Vulkan
```

La prioridad inicial deberá ser:

```text
Compatibilidad
     >
Estabilidad
     >
Rendimiento
     >
Mejoras gráficas
```

---

# 7. Estructura del proyecto

Crear:

```text
R2N64/
│
├── README.md
│
├── LICENSE
│
├── Makefile
│
├── build/
│
├── dist/
│
├── external/
│   │
│   ├── libretro/
│   │
│   └── mupen64plus-next/
│
├── include/
│   │
│   ├── app.h
│   ├── emulator.h
│   ├── input.h
│   ├── audio.h
│   ├── video.h
│   ├── rom.h
│   ├── config.h
│   ├── saves.h
│   └── ui.h
│
├── src/
│   │
│   ├── main.cpp
│   ├── app.cpp
│   ├── emulator.cpp
│   ├── input.cpp
│   ├── audio.cpp
│   ├── video.cpp
│   ├── rom.cpp
│   ├── config.cpp
│   ├── saves.cpp
│   └── ui.cpp
│
├── assets/
│   │
│   ├── logo.png
│   ├── background.png
│   ├── default_cover.png
│   ├── fonts/
│   └── icons/
│
├── shaders/
│
├── config/
│   └── default.json
│
├── pkg/
│   │
│   ├── param.sfo
│   ├── icon0.png
│   └── sce_sys/
│
└── scripts/
    ├── build.sh
    ├── build.bat
    └── package.py
```

---

# 8. Flujo de ejecución

Cuando R2N64 se ejecute:

```text
main()
 │
 ├── Inicializar PS4
 │
 ├── Inicializar SDL
 │
 ├── Inicializar video
 │
 ├── Inicializar audio
 │
 ├── Inicializar DualShock 4
 │
 ├── Leer configuración
 │
 ├── Buscar ROMs
 │
 ├── Construir biblioteca
 │
 └── Abrir interfaz
```

Cuando se seleccione un juego:

```text
Seleccionar ROM
      │
      ▼
Leer cabecera N64
      │
      ▼
Identificar juego
      │
      ▼
Cargar configuración
      │
      ▼
Inicializar Mupen64Plus
      │
      ▼
Cargar ROM
      │
      ▼
Iniciar emulación
```

---

# 9. Primer main.cpp

Ejemplo conceptual:

```cpp
#include "app.h"
#include "video.h"
#include "input.h"
#include "audio.h"
#include "ui.h"

int main()
{
    App app;

    if (!app.initialize())
        return -1;

    while (app.isRunning())
    {
        app.processInput();
        app.update();
        app.render();
    }

    app.shutdown();

    return 0;
}
```

---

# 10. Clase principal

## app.h

```cpp
#pragma once

class App
{
public:

    bool initialize();

    void processInput();

    void update();

    void render();

    void shutdown();

    bool isRunning() const;

private:

    bool running = true;
};
```

---

# 11. SDL2

SDL puede utilizarse inicialmente para:

- ventana/renderizado;
- texturas;
- audio;
- entrada;
- fuentes;
- interfaz.

Inicialización conceptual:

```cpp
#include <SDL2/SDL.h>

bool initSDL()
{
    if (SDL_Init(
        SDL_INIT_VIDEO |
        SDL_INIT_AUDIO |
        SDL_INIT_GAMECONTROLLER
    ) != 0)
    {
        return false;
    }

    return true;
}
```

---

# 12. Resolución

Inicialmente usar:

```text
1920 × 1080
```

Interfaz:

```text
1080p
```

La resolución interna del N64 será independiente.

Ejemplo:

```text
N64 original
320×240
     │
     ▼
Emulador
640×480
     │
     ▼
Upscaling
1920×1080
```

---

# 13. Aspect Ratio

Opciones:

```text
Original 4:3
16:9 Stretch
16:9 Widescreen Hack
Pixel Perfect
```

Predeterminado:

```text
4:3
```

---

# 14. Integración Libretro

Crear:

```text
src/libretro_frontend.cpp
```

El frontend deberá implementar callbacks como:

```cpp
retro_environment_t
retro_video_refresh_t
retro_audio_sample_t
retro_audio_sample_batch_t
retro_input_poll_t
retro_input_state_t
```

Ejemplo conceptual:

```cpp
static void videoCallback(
    const void* data,
    unsigned width,
    unsigned height,
    size_t pitch)
{
    video.updateFrame(
        data,
        width,
        height,
        pitch
    );
}
```

---

# 15. Entrada

Mapeo inicial de Nintendo 64 a DualShock 4:

| Nintendo 64 | DualShock 4 |
|---|---|
| Stick N64 | Stick izquierdo |
| A | X |
| B | Cuadrado |
| Z | L2 |
| L | L1 |
| R | R1 |
| C-Up | Stick derecho arriba |
| C-Down | Stick derecho abajo |
| C-Left | Stick derecho izquierda |
| C-Right | Stick derecho derecha |
| Start | Options |
| D-Pad | D-Pad |

---

# 16. Atajos

Crear:

```text
OPTIONS + X
```

para abrir:

```text
Quick Menu
```

Otro ejemplo:

```text
OPTIONS + TRIANGLE
```

Guardar estado.

```text
OPTIONS + SQUARE
```

Cargar estado.

```text
OPTIONS + CIRCLE
```

Salir al menú.

---

# 17. Menú rápido

Durante la emulación:

```text
┌─────────────────────────────┐
│          R2N64              │
├─────────────────────────────┤
│ Continuar                   │
│ Guardar estado              │
│ Cargar estado               │
│ Reiniciar                   │
│ Controles                   │
│ Video                       │
│ Audio                       │
│ Cheats                      │
│ Cerrar juego                │
└─────────────────────────────┘
```

---

# 18. ROM Manager

Crear:

```text
src/rom.cpp
```

Responsable de detectar:

```text
.n64
.z64
.v64
```

---

# 19. Directorios

Por ejemplo:

```text
/data/R2N64/roms/
```

También buscar almacenamiento externo disponible.

Estructura:

```text
R2N64/
│
├── roms/
├── covers/
├── saves/
├── states/
├── screenshots/
├── cache/
├── configs/
└── logs/
```

---

# 20. ROM Scanner

Pseudo código:

```cpp
for (const auto& file : directory)
{
    if (
        extension == ".z64" ||
        extension == ".n64" ||
        extension == ".v64"
    )
    {
        addRom(file);
    }
}
```

---

# 21. Identificación del juego

Leer información de la ROM:

```text
ROM Name
CRC1
CRC2
Country
Region
Game ID
Size
```

Crear estructura:

```cpp
struct Game
{
    std::string title;

    std::string path;

    std::string serial;

    uint32_t crc1;

    uint32_t crc2;

    std::string region;

    std::string cover;
};
```

---

# 22. Base de datos local

Para el MVP utilizar:

```text
JSON
```

Ejemplo:

```json
{
    "games": [
        {
            "title": "Example Game",
            "path": "/data/R2N64/roms/example.z64",
            "favorite": false,
            "play_time": 0,
            "last_played": null
        }
    ]
}
```

Posteriormente:

```text
SQLite
```

---

# 23. Interfaz

La pantalla principal:

```text
┌────────────────────────────────────────────────────┐
│ R2N64                                      ⚙  🔍   │
│                                                    │
│ Nintendo 64                                       │
│                                                    │
│ ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐       │
│ │        │ │        │ │        │ │        │       │
│ │ COVER  │ │ COVER  │ │ COVER  │ │ COVER  │       │
│ │        │ │        │ │        │ │        │       │
│ └────────┘ └────────┘ └────────┘ └────────┘       │
│                                                    │
│ Example Game                                       │
│                                                    │
│ X Jugar    △ Opciones    □ Favorito    O Atrás    │
└────────────────────────────────────────────────────┘
```

---

# 24. Diseño visual

Estilo:

```text
PS4
+
Nintendo 64
+
Interfaz moderna
```

Evitar una interfaz similar a:

```text
lista tradicional de archivos
```

Priorizar:

```text
Carátulas
Animaciones
Transiciones
Blur
Fondos dinámicos
Grid
```

---

# 25. Biblioteca

Permitir:

```text
Todos
Favoritos
Recientes
Región
Multijugador
Orden alfabético
Más jugados
Últimos jugados
```

---

# 26. Página del juego

```text
┌──────────────────────────────────────────────┐
│                                              │
│   ┌──────────┐      SUPER MARIO 64           │
│   │          │                               │
│   │  COVER   │      Nintendo 64              │
│   │          │                               │
│   └──────────┘      ▶ JUGAR                  │
│                                              │
│   Tiempo jugado: 15h 35m                     │
│   Última partida: Hoy                        │
│                                              │
│   Configuración                              │
│   Save States                                │
│   Capturas                                   │
│                                              │
└──────────────────────────────────────────────┘
```

---

# 27. Configuración gráfica

Configuración global:

```text
Renderer
Internal Resolution
Aspect Ratio
VSync
Frame Limit
Texture Filtering
Anti-Aliasing
Frame Buffer
Widescreen
Shader
```

---

# 28. Configuración por juego

Crear archivos:

```text
configs/games/
```

Ejemplo:

```text
configs/games/CRC1-CRC2.json
```

Contenido:

```json
{
    "renderer": "default",
    "resolution": "2x",
    "aspect_ratio": "4:3",
    "vsync": true,
    "frameskip": 0
}
```

---

# 29. Saves

Separar:

```text
SRAM
EEPROM
FlashRAM
Controller Pak
Save State
```

Directorio:

```text
R2N64/saves/
```

---

# 30. Save States

Crear:

```text
states/
   gameid/
      state0.sav
      state1.sav
      state2.sav
```

La interfaz debería mostrar:

```text
Slot
Fecha
Hora
Screenshot
```

Ejemplo:

```text
┌─────────────────────────┐
│ SAVE STATE 1            │
│                         │
│ [ screenshot ]          │
│                         │
│ 04/10/2026 23:46        │
└─────────────────────────┘
```

---

# 31. Auto Save

Al cerrar el juego:

```text
Crear save state automático
```

Al iniciarlo nuevamente:

```text
¿Continuar partida?
```

---

# 32. Capturas

Permitir:

```text
Screenshot
```

Guardar en:

```text
R2N64/screenshots/
```

Formato:

```text
PNG
```

---

# 33. Tiempo jugado

Al iniciar:

```cpp
startTime = currentTime();
```

Al cerrar:

```cpp
playTime += currentTime() - startTime;
```

Guardar:

```json
{
    "play_time": 55832
}
```

---

# 34. Logs

Crear:

```text
R2N64/logs/r2n64.log
```

Ejemplo:

```text
[INFO] R2N64 started
[INFO] SDL initialized
[INFO] DS4 detected
[INFO] ROM scanner started
[INFO] 31 ROMs detected
[INFO] Loading game
[INFO] Core initialized
```

Errores:

```text
[ERROR]
[WARNING]
[CRITICAL]
```

---

# 35. Pantalla de diagnóstico

Agregar:

```text
Settings
   │
   └── Developer
```

Mostrar:

```text
R2N64 Version
Core Version
FPS
Frame Time
Audio Buffer
CPU Core
Renderer
RAM usage
Loaded ROM
CRC
Resolution
```

---

# 36. FPS counter

Opcional:

```text
FPS: 60.00
Frame: 16.67 ms
```

---

# 37. Rendimiento

El loop no debe bloquear la interfaz.

Separar:

```text
Thread principal
     │
     ├── UI
     ├── Input
     └── Render

Emulation Thread
     │
     └── Mupen64Plus

Audio Thread
     │
     └── Audio Buffer
```

---

# 38. Audio

Pipeline:

```text
Mupen64Plus
     │
     ▼
Libretro audio callback
     │
     ▼
R2N64 Audio Buffer
     │
     ▼
SDL Audio
     │
     ▼
PS4
```

---

# 39. Buffer de audio

Utilizar un ring buffer:

```text
Emulator
   │
   ▼
┌──────────────────┐
│ Ring Audio Buffer│
└──────────────────┘
   │
   ▼
Audio Output
```

Esto ayuda a evitar:

```text
stuttering
crackling
audio skipping
```

---

# 40. Render pipeline

```text
Mupen64Plus
      │
      ▼
N64 framebuffer
      │
      ▼
Video callback
      │
      ▼
SDL Texture
      │
      ▼
PS4 framebuffer
      │
      ▼
TV
```

---

# 41. Sincronización

Objetivo:

```text
NTSC → ~60 Hz
PAL  → ~50 Hz
```

El emulador debe respetar la región de la ROM.

---

# 42. Compilación de prueba

Antes de integrar N64:

Crear una aplicación:

```text
Hello R2N64
```

que muestre:

```text
R2N64
PS4 Nintendo 64 Emulator

X - Start
```

Si esto funciona en la PS4, continuar con el proyecto.

---

# 43. Etapas de desarrollo

## Fase 1

Compilar homebrew PS4.

Objetivo:

```text
R2N64 Hello World
```

---

## Fase 2

SDL.

Objetivo:

```text
mostrar interfaz 1080p
```

---

## Fase 3

DualShock 4.

Objetivo:

```text
moverse por el menú
```

---

## Fase 4

Filesystem.

Objetivo:

```text
detectar ROMs
```

---

## Fase 5

Mupen64Plus.

Objetivo:

```text
arrancar una ROM
```

---

## Fase 6

Video.

Objetivo:

```text
mostrar correctamente framebuffer N64
```

---

## Fase 7

Audio.

Objetivo:

```text
audio sincronizado
```

---

## Fase 8

Controles.

Objetivo:

```text
jugar correctamente
```

---

## Fase 9

Saves.

Objetivo:

```text
guardar/cargar partida
```

---

## Fase 10

Interfaz.

Objetivo:

```text
biblioteca completa
```

---

# 44. Juegos iniciales de prueba

Empezar con juegos conocidos por ser relativamente sencillos para la emulación.

Crear una matriz:

| Juego | Boot | Video | Audio | Input | FPS | Estado |
|---|---:|---:|---:|---:|---:|---|
| Juego A | ✅ | ✅ | ✅ | ✅ | 60 | Working |
| Juego B | ✅ | ✅ | ✅ | ✅ | 60 | Working |
| Juego C | ✅ | ⚠ | ✅ | ✅ | 55 | Playable |

Utilizar únicamente imágenes de juegos que tengas derecho a ejecutar.

---

# 45. Estados de compatibilidad

Utilizar:

```text
Perfect
Playable
In Game
Boots
Broken
Untested
```

---

# 46. Compatibility Database

Crear:

```text
compatibility.json
```

Ejemplo:

```json
{
    "CRC1-CRC2": {
        "status": "playable",
        "renderer": "angrylion",
        "resolution": "1x",
        "notes": ""
    }
}
```

---

# 47. Overrides automáticos

Al detectar ciertos juegos:

```cpp
if (game.crc == GAME_CRC)
{
    config.renderer = RENDERER_ACCURATE;
}
```

Preferiblemente no hardcodear estas opciones y leerlas desde:

```text
compatibility.json
```

---

# 48. Sistema de carátulas

Identificar juegos mediante:

```text
CRC
Game ID
ROM Header
```

Buscar:

```text
covers/<gameid>.png
```

Si no existe:

```text
default_cover.png
```

---

# 49. Cache

Crear:

```text
cache/library.json
```

Así no será necesario escanear todas las ROMs en cada arranque.

---

# 50. Configuración global

Archivo:

```text
config/r2n64.json
```

Ejemplo:

```json
{
    "language": "es",
    "theme": "dark",
    "view": "grid",

    "video": {
        "resolution": "1080p",
        "vsync": true
    },

    "emulator": {
        "renderer": "default",
        "internal_resolution": "1x"
    },

    "ui": {
        "animations": true,
        "show_fps": false
    }
}
```

---

# 51. Temas

Soportar:

```text
Dark
Light
N64
PS4
Custom
```

Estructura:

```text
themes/
    default/
    n64/
    ps4/
```

---

# 52. Home Screen

Añadir:

```text
Continue Playing
Recently Played
Favorites
All Games
```

Ejemplo:

```text
R2N64

CONTINUAR JUGANDO

[Game]

RECIENTES

[Game] [Game] [Game]

BIBLIOTECA

[Game] [Game] [Game] [Game]
```

---

# 53. Splash Screen

Al iniciar:

```text
██████╗ ██████╗ ███╗   ██╗ ██████╗ ██╗  ██╗
██╔══██╗╚════██╗████╗  ██║██╔════╝ ██║  ██║
██████╔╝ █████╔╝██╔██╗ ██║███████╗ ███████║
██╔══██╗██╔═══╝ ██║╚██╗██║██╔═══██╗╚════██║
██║  ██║███████╗██║ ╚████║╚██████╔╝     ██║
╚═╝  ╚═╝╚══════╝╚═╝  ╚═══╝ ╚═════╝      ╚═╝

Nintendo 64 Emulator for PS4
```

---

# 54. Control Manager

Crear:

```text
src/controller_manager.cpp
```

Permitir:

```text
Controller 1
Controller 2
Controller 3
Controller 4
```

Objetivo futuro:

```text
4 jugadores locales
```

---

# 55. Profiles

Permitir perfiles:

```text
Default
Game specific
Custom
```

---

# 56. Remapeo

Ejemplo:

```text
N64 A
   ↓
DualShock X
```

Debe ser editable.

---

# 57. Vibración

Mapear:

```text
N64 Rumble Pak
```

a:

```text
DualShock 4 vibration
```

---

# 58. Memory Pak

Implementar almacenamiento persistente:

```text
Controller Pak
```

por juego o por perfil.

---

# 59. Cheats

Opcional para una fase posterior.

Formato:

```text
GameShark
```

No es necesario para el MVP.

---

# 60. Shader Manager

Fase posterior:

```text
Nearest
Bilinear
CRT
Scanlines
Sharp
```

---

# 61. Modo Pixel Perfect

Crear:

```text
Pixel Perfect
```

para quienes prefieran el aspecto N64 original.

---

# 62. Mejoras gráficas

Fases futuras:

```text
2x resolution
3x resolution
4x resolution

Widescreen

Texture Packs

Anti-Aliasing

Anisotropic Filtering
```

Siempre mantener una opción:

```text
Original
```

---

# 63. Seguridad del programa

R2N64 nunca debe escribir fuera de sus directorios.

Validar:

```text
ROM paths
Config paths
Cover paths
Save paths
```

Evitar:

```text
../
```

path traversal.

---

# 64. Manejo de errores

Nunca cerrar silenciosamente.

Mostrar:

```text
No se pudo cargar el juego.

Código:
R2N64-ROM-001
```

y escribir el detalle técnico en:

```text
logs/r2n64.log
```

---

# 65. Crash Handler

Cuando sea posible guardar:

```text
Timestamp
Current ROM
Core
Renderer
Configuration
Last log lines
```

---

# 66. Development Mode

Agregar:

```text
Developer Mode
```

Opciones:

```text
FPS
Frame Time
Memory
Core Logging
Verbose Logs
ROM Info
Audio Debug
```

---

# 67. Makefile

Estructura conceptual:

```makefile
TARGET := R2N64

SOURCES := \
    src/main.cpp \
    src/app.cpp \
    src/video.cpp \
    src/audio.cpp \
    src/input.cpp \
    src/rom.cpp \
    src/config.cpp \
    src/saves.cpp \
    src/ui.cpp

OBJECTS := $(SOURCES:.cpp=.o)

all:
	@echo "Building R2N64"

clean:
	rm -rf build/*
```

Adaptar posteriormente a las reglas del OpenOrbis Toolchain.

---

# 68. Variable OpenOrbis

La compilación deberá comprobar:

```text
OO_PS4_TOOLCHAIN
```

Ejemplo Linux:

```bash
export OO_PS4_TOOLCHAIN=/opt/OpenOrbis
```

---

# 69. Dependencias Linux

Como base:

```bash
sudo apt update

sudo apt install \
    clang \
    lld \
    cmake \
    make \
    git \
    python3
```

---

# 70. Dependencias Windows

Instalar:

```text
Git
Python
LLVM/Clang
CMake
OpenOrbis Toolchain
```

Opcional:

```text
Visual Studio Code
Visual Studio
```

---

# 71. Clonar componentes

El repositorio deberá contener o descargar:

```text
OpenOrbis
Mupen64Plus-Next
Libretro headers
```

Preferiblemente mediante:

```text
git submodule
```

Ejemplo conceptual:

```text
external/
├── libretro/
└── mupen64plus-next/
```

---

# 72. Primer objetivo de compilación

Antes del emulador completo conseguir:

```text
build/R2N64.elf
```

Luego generar:

```text
eboot.bin
```

Finalmente:

```text
R2N64.pkg
```

Pipeline:

```text
C/C++
  │
  ▼
Clang
  │
  ▼
ELF
  │
  ▼
OpenOrbis tools
  │
  ▼
eboot.bin
  │
  ▼
PKG builder
  │
  ▼
R2N64.pkg
```

---

# 73. Contenido PKG

Conceptualmente:

```text
R2N64.pkg
│
├── eboot.bin
│
├── sce_sys/
│   ├── param.sfo
│   └── icon0.png
│
└── assets/
```

---

# 74. Icono PS4

Preparar:

```text
icon0.png
```

Diseño:

```text
R2
N64
```

con estética moderna inspirada en la generación N64 sin utilizar material gráfico protegido innecesariamente.

---

# 75. Versionado

Formato:

```text
R2N64 v0.1.0
```

Usar:

```text
MAJOR.MINOR.PATCH
```

Ejemplo:

```text
0.1.0
```

Primer boot.

```text
0.2.0
```

Primer juego ejecutable.

```text
0.3.0
```

Audio + controles.

```text
0.5.0
```

Biblioteca.

```text
1.0.0
```

Release estable.

---

# 76. Roadmap

## R2N64 0.1

```text
[ ] OpenOrbis compilando
[ ] SDL
[ ] GUI básica
[ ] DualShock
```

## R2N64 0.2

```text
[ ] ROM scanner
[ ] Mupen64Plus
[ ] Primer juego
```

## R2N64 0.3

```text
[ ] Audio
[ ] Input completo
[ ] Saves
```

## R2N64 0.4

```text
[ ] Save States
[ ] Game settings
[ ] Compatibility DB
```

## R2N64 0.5

```text
[ ] Biblioteca
[ ] Carátulas
[ ] Favoritos
[ ] Recientes
```

## R2N64 0.6

```text
[ ] 4 jugadores
[ ] Rumble
[ ] Controller Pak
```

## R2N64 0.7

```text
[ ] Renderer mejorado
[ ] Upscaling
[ ] Shaders
```

## R2N64 1.0

```text
[ ] interfaz terminada
[ ] estabilidad
[ ] optimizaciones
[ ] documentación
[ ] PKG release
```

---

# 77. MVP

No intentar implementar todo inmediatamente.

El MVP debe conseguir únicamente:

```text
1. R2N64 abre en PS4
2. Muestra menú
3. Detecta DualShock
4. Detecta una ROM
5. Carga Mupen64Plus
6. Ejecuta la ROM
7. Muestra imagen
8. Produce audio
9. Recibe controles
10. Guarda partida
```

Cuando estos diez elementos funcionen, desarrollar el resto.

---

# 78. Primera milestone

## Milestone: R2N64 Boots

Resultado:

```text
PS4
 │
 ▼
R2N64
 │
 ▼
SDL2
 │
 ▼
GUI
```

---

# 79. Segunda milestone

## Milestone: First ROM

```text
R2N64
   │
   ▼
ROM Browser
   │
   ▼
example.z64
   │
   ▼
Mupen64Plus
   │
   ▼
Nintendo 64 game running
```

Éste será el primer gran objetivo del proyecto.

---

# 80. Estrategia recomendada para el port

No modificar inmediatamente cientos de archivos de Mupen64Plus.

Crear primero una capa:

```text
R2N64 Platform Layer
```

Ejemplo:

```text
platform/
│
├── ps4/
│   ├── filesystem.cpp
│   ├── audio.cpp
│   ├── video.cpp
│   ├── threads.cpp
│   ├── timer.cpp
│   └── input.cpp
│
└── desktop/
```

Esto permitirá desarrollar partes del frontend en PC.

---

# 81. Desktop Development

Crear también:

```text
R2N64-PC
```

para probar:

```text
GUI
ROM scanner
Database
Covers
Configuration
Save Manager
```

sin tener que instalar un PKG para cada modificación.

Posteriormente:

```text
Desktop Backend
       ↓
Platform abstraction
       ↓
PS4 Backend
```

---

# 82. Capa de plataforma

Ejemplo:

```cpp
class Platform
{
public:

    virtual bool initializeVideo() = 0;

    virtual bool initializeAudio() = 0;

    virtual bool initializeInput() = 0;

    virtual std::string dataPath() = 0;

    virtual uint64_t getTime() = 0;
};
```

Implementaciones:

```text
PlatformDesktop
PlatformPS4
```

---

# 83. Ventajas

Esto permite desarrollar:

```text
80% del frontend en Windows/Linux
```

y reservar para PS4:

```text
input específico
audio específico
filesystem específico
render específico
packaging
```

---

# 84. Prioridad técnica

Orden recomendado:

```text
OpenOrbis
    ↓
Hello World
    ↓
SDL
    ↓
Input
    ↓
Filesystem
    ↓
Libretro
    ↓
Mupen64Plus
    ↓
Video
    ↓
Audio
    ↓
Saves
    ↓
UI final
```

No comenzar por la interfaz avanzada antes de conseguir ejecutar correctamente el core.

---

# 85. Proyecto inicial

El primer repositorio debería contener:

```text
R2N64/
├── frontend/
├── platform/
├── external/
├── assets/
├── pkg/
└── tools/
```

---

# 86. Filosofía

R2N64 debe ser:

```text
Simple
Rápido
Visual
Estable
Optimizado para mando
```

y evitar convertirse en una interfaz con cientos de opciones técnicas visibles.

Configuración avanzada:

```text
Settings
   ↓
Advanced
```

---

# 87. Objetivo de experiencia

El usuario debería poder:

```text
Instalar R2N64.pkg
        ↓
Copiar sus ROMs legales
        ↓
Abrir R2N64
        ↓
Ver automáticamente los juegos
        ↓
Seleccionar carátula
        ↓
X
        ↓
Jugar
```

---

# 88. Consideraciones legales

R2N64 debe distribuir únicamente:

```text
código del emulador
frontend
assets propios
configuraciones
```

No distribuir:

```text
ROMs comerciales
BIOS protegidas
keys
material propietario de juegos
```

Los usuarios deberán utilizar dumps que tengan derecho a utilizar.

Además deberán respetarse las licencias de:

```text
Mupen64Plus-Next
Libretro
SDL
OpenOrbis
```

y de cualquier otra biblioteca incorporada.

---

# 89. Referencias técnicas

Investigar principalmente:

```text
OpenOrbis PS4 Toolchain

OpenOrbis SDL-PS4

OpenOrbis LibOrbisPkg

Mupen64Plus-Next

Libretro API

RetroArch PS4

ps4-retrobox
```

`ps4-retrobox` es especialmente interesante como referencia porque demuestra el funcionamiento del core:

```text
mupen64plus_next
```

en PS4.

---

# 90. Resultado final esperado

```text
                    R2N64

          Nintendo 64 Emulator for PS4


        ┌──────┐ ┌──────┐ ┌──────┐
        │ GAME │ │ GAME │ │ GAME │
        │      │ │      │ │      │
        └──────┘ └──────┘ └──────┘


                    X JUGAR
```

Generado finalmente como:

```text
R2N64-v1.0.0.pkg
```

---

# 91. Primer Sprint

## Sprint 1 — Foundation

Crear:

```text
[ ] repositorio R2N64
[ ] toolchain
[ ] Makefile
[ ] main.cpp
[ ] SDL
[ ] resolución 1080p
[ ] sistema de logs
[ ] DualShock
[ ] estructura assets
```

Resultado esperado:

```text
R2N64 arranca en PS4.
```

---

# 92. Sprint 2 — Emulator Core

```text
[ ] agregar libretro headers
[ ] agregar Mupen64Plus-Next
[ ] implementar callbacks
[ ] cargar core
[ ] cargar ROM
[ ] video callback
[ ] audio callback
[ ] input callback
```

Resultado:

```text
Primera ROM de N64 funcionando.
```

---

# 93. Sprint 3 — Usabilidad

```text
[ ] ROM scanner
[ ] biblioteca
[ ] carátulas
[ ] save manager
[ ] configuración
[ ] game overrides
[ ] favoritos
[ ] recientes
```

---

# 94. Sprint 4 — Optimización

```text
[ ] benchmarking
[ ] corregir frame pacing
[ ] audio latency
[ ] optimización del renderer
[ ] probar juegos difíciles
[ ] compatibility database
```

---

# 95. Definición de éxito

R2N64 v1.0 estará listo cuando:

```text
✓ instala mediante PKG
✓ inicia correctamente
✓ reconoce DualShock
✓ descubre ROMs
✓ ejecuta N64
✓ audio estable
✓ controles estables
✓ saves funcionan
✓ save states funcionan
✓ interfaz estable
✓ vuelve al menú sin crash
✓ configuraciones persisten
✓ biblioteca persiste
```

---

# R2N64

```text
PS4
+
OpenOrbis
+
SDL2
+
Libretro
+
Mupen64Plus-Next
=
R2N64
```

**Objetivo: crear una experiencia Nintendo 64 nativa y sencilla para PlayStation 4 homebrew.**