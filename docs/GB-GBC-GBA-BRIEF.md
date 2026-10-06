# CODEX_R2N64_ADD_GB_GBC_GBA.md

# Extensión de R2N64 para Game Boy, Game Boy Color y Game Boy Advance

## Objetivo

Extender el proyecto actual **R2N64 para PS4** para que, además de Nintendo 64, pueda ejecutar:

- Game Boy
- Game Boy Color
- Game Boy Advance

La ampliación debe preservar el soporte N64 existente y convertir el frontend en una arquitectura multi-sistema basada en cores intercambiables.

No renombrar automáticamente el proyecto. Internamente puede mantenerse `R2N64`; un nombre más amplio como `R2Retro` puede evaluarse después.

---

## Sistemas y extensiones

```text
Game Boy         .gb
Game Boy Color   .gbc
Game Boy Advance .gba
Nintendo 64      .z64 .n64 .v64
```

---

## Cores recomendados

### Game Boy / Game Boy Color

Preferir:

```text
SameBoy
```

Alternativa:

```text
Gambatte
```

### Game Boy Advance

Preferir:

```text
mGBA
```

### Nintendo 64

Mantener:

```text
Mupen64Plus-Next
```

Siempre que sea posible, integrar los cores mediante la misma capa **libretro** para evitar duplicar frontend, audio, video e input.

---

# Arquitectura objetivo

```text
                       R2N64 / R2Retro
                              │
                           Frontend
                              │
                        Core Manager
                              │
           ┌──────────────────┼──────────────────┐
           │                  │                  │
        SameBoy             mGBA        Mupen64Plus-Next
           │                  │                  │
        GB / GBC             GBA                N64
```

El frontend no debe conocer detalles internos de cada core.

---

# Core Manager

Crear:

```text
core/
├── core_interface.h
├── core_manager.h
├── core_manager.cpp
├── core_registry.h
├── core_registry.cpp
├── libretro_core.h
└── libretro_core.cpp
```

Interfaz base:

```cpp
enum class SystemType
{
    Unknown,
    GameBoy,
    GameBoyColor,
    GameBoyAdvance,
    Nintendo64
};

class IEmulationCore
{
public:
    virtual ~IEmulationCore() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual bool loadGame(const std::string& path) = 0;
    virtual void unloadGame() = 0;

    virtual void runFrame() = 0;
    virtual void reset() = 0;

    virtual SystemType systemType() const = 0;
    virtual const char* coreName() const = 0;
};
```

---

# Core Registry

Crear:

```cpp
struct CoreDescriptor
{
    std::string id;
    std::string name;
    std::vector<SystemType> systems;
    std::vector<std::string> extensions;
};
```

Registro esperado:

```text
sameboy
 ├── Game Boy
 └── Game Boy Color

mgba
 └── Game Boy Advance

mupen64plus_next
 └── Nintendo 64
```

---

# Detección automática de sistema

Crear:

```text
frontend/system_detector.h
frontend/system_detector.cpp
```

Mapeo:

```text
.gb   → GameBoy
.gbc  → GameBoyColor
.gba  → GameBoyAdvance
.z64  → Nintendo64
.n64  → Nintendo64
.v64  → Nintendo64
```

Ejemplo:

```cpp
SystemType detectSystemFromExtension(const std::string& ext)
{
    if (ext == ".gb")
        return SystemType::GameBoy;

    if (ext == ".gbc")
        return SystemType::GameBoyColor;

    if (ext == ".gba")
        return SystemType::GameBoyAdvance;

    if (ext == ".z64" || ext == ".n64" || ext == ".v64")
        return SystemType::Nintendo64;

    return SystemType::Unknown;
}
```

Si el core permite validar cabeceras, usar esa validación como segunda etapa.

---

# Estructura de proyecto

```text
R2N64/
│
├── frontend/
├── core/
│   ├── common/
│   ├── libretro/
│   ├── sameboy/
│   ├── mgba/
│   └── mupen64plus/
│
├── platform/
│   ├── common/
│   ├── desktop/
│   └── ps4/
│
├── renderer/
├── audio/
├── input/
├── filesystem/
├── profiler/
├── config/
│
├── assets/
│   ├── systems/
│   │   ├── gb/
│   │   ├── gbc/
│   │   ├── gba/
│   │   └── n64/
│   └── ui/
│
├── shaders/
├── external/
│   ├── libretro/
│   ├── sameboy/
│   ├── mgba/
│   └── mupen64plus-next/
│
├── roms/
│   ├── gb/
│   ├── gbc/
│   ├── gba/
│   └── n64/
│
├── saves/
├── states/
├── screenshots/
├── covers/
├── cache/
├── logs/
└── pkg/
```

---

# ROM Manager

Actualizar el modelo de juego:

```cpp
struct Game
{
    std::string title;
    std::string path;

    SystemType system = SystemType::Unknown;

    std::string gameId;
    std::string region;

    std::uint32_t crc1 = 0;
    std::uint32_t crc2 = 0;

    std::string coverPath;

    bool favorite = false;
    std::uint64_t playTimeSeconds = 0;
};
```

El scanner debe detectar:

```text
.gb
.gbc
.gba
.z64
.n64
.v64
```

---

# Biblioteca multi-sistema

Agregar filtros:

```text
Todos
Game Boy
Game Boy Color
Game Boy Advance
Nintendo 64
Favoritos
Recientes
Más jugados
```

Vista conceptual:

```text
┌──────────────────────────────────────────────┐
│ R2N64 / R2Retro                         ⚙    │
│                                              │
│ CONTINUAR                                    │
│ [GAME]                                       │
│                                              │
│ GAME BOY                                     │
│ [■] [■] [■] [■]                             │
│                                              │
│ GAME BOY COLOR                               │
│ [■] [■] [■]                                  │
│                                              │
│ GAME BOY ADVANCE                             │
│ [■] [■] [■] [■]                             │
│                                              │
│ NINTENDO 64                                  │
│ [■] [■] [■]                                  │
└──────────────────────────────────────────────┘
```

---

# Configuración de cores

Archivo global:

```json
{
  "cores": {
    "gameboy": "sameboy",
    "gameboycolor": "sameboy",
    "gameboyadvance": "mgba",
    "nintendo64": "mupen64plus_next"
  }
}
```

Debe poder existir selección manual desde:

```text
Settings
└── Emulation
    └── Core Selection
```

---

# Saves

Separar por sistema:

```text
saves/
├── gb/
├── gbc/
├── gba/
└── n64/
```

Save states:

```text
states/
├── gb/
├── gbc/
├── gba/
└── n64/
```

No mezclar:

```text
native saves
save states
```

---

# Save Manager común

```cpp
class SaveManager
{
public:
    bool saveNative(const Game& game);
    bool loadNative(const Game& game);

    bool saveState(const Game& game, int slot);
    bool loadState(const Game& game, int slot);

    bool autoSave(const Game& game);
};
```

El frontend debe delegar al core cuando el formato sea específico.

---

# Input Game Boy

| Game Boy | DualShock 4 |
|---|---|
| D-Pad | D-Pad |
| A | X |
| B | Cuadrado |
| Start | Options |
| Select | Touchpad / botón configurable |

---

# Input Game Boy Color

| Game Boy Color | DualShock 4 |
|---|---|
| D-Pad | D-Pad |
| A | X |
| B | Cuadrado |
| Start | Options |
| Select | Touchpad / botón configurable |

---

# Input Game Boy Advance

| Game Boy Advance | DualShock 4 |
|---|---|
| D-Pad | D-Pad |
| A | X |
| B | Cuadrado |
| L | L1 |
| R | R1 |
| Start | Options |
| Select | Touchpad / botón configurable |

Opcional:

```text
L2 → Rewind
R2 → Fast Forward
```

Todo debe poder remapearse.

---

# Quick Menu unificado

```text
Continuar
Guardar estado
Cargar estado
Reiniciar
Fast Forward
Rewind
Controles
Video
Audio
Core
Cerrar juego
```

Ocultar funciones no soportadas por el core.

---

# Fast Forward

Opciones:

```text
1x
2x
4x
8x
```

Configuración:

```json
{
  "fast_forward": {
    "enabled": true,
    "multiplier": 4
  }
}
```

---

# Rewind

Activar inicialmente para:

```text
GB
GBC
GBA
```

si la serialización del core es suficientemente rápida.

Arquitectura:

```text
Frame
 ↓
State Snapshot
 ↓
Circular RAM Buffer
 ↓
Rewind
```

Nunca escribir un save state a disco cada frame.

Configuración:

```json
{
  "rewind": {
    "enabled": true,
    "seconds": 20,
    "snapshot_interval_frames": 2
  }
}
```

Debe existir un límite de memoria.

---

# Resoluciones nativas

## Game Boy

```text
160 × 144
```

## Game Boy Color

```text
160 × 144
```

## Game Boy Advance

```text
240 × 160
```

---

# Escalado

Agregar:

```text
Original
Integer Scaling
Nearest Neighbor
Bilinear
Fit Screen
Stretch
```

Predeterminado para GB/GBC/GBA:

```text
Integer Scaling + Nearest
```

---

# Shaders

Agregar perfiles opcionales:

```text
Nearest
Bilinear
LCD Grid
GB LCD
GBC LCD
GBA LCD
Scanlines
CRT
Sharp
```

No usar shaders costosos por defecto.

---

# Perfil por sistema

Ejemplo:

```json
{
  "video_profiles": {
    "gb": {
      "shader": "gb_lcd",
      "integer_scaling": true
    },

    "gbc": {
      "shader": "gbc_lcd",
      "integer_scaling": true
    },

    "gba": {
      "shader": "gba_lcd",
      "integer_scaling": true
    },

    "n64": {
      "shader": "none",
      "integer_scaling": false
    }
  }
}
```

---

# Game Boy palettes

Si el core lo soporta:

```text
Original Green
Pocket
Light
Gray
Custom Palette
```

Ejemplo:

```json
{
  "gameboy": {
    "palette": "original_green"
  }
}
```

---

# Video API común

```cpp
struct VideoFrame
{
    const void* data = nullptr;

    unsigned width = 0;
    unsigned height = 0;

    std::size_t pitch = 0;

    PixelFormat format;
};
```

Todos los cores deben entregar frames al mismo renderer.

---

# Audio API común

Reutilizar:

```text
Audio Ring Buffer
Audio Thread
PS4 Audio Backend
```

Interfaz:

```cpp
class AudioSink
{
public:
    virtual ~AudioSink() = default;

    virtual void submit(
        const std::int16_t* samples,
        std::size_t frames,
        int sampleRate
    ) = 0;
};
```

---

# Libretro

La capa libretro debe ser compartida.

Callbacks:

```text
retro_environment_t
retro_video_refresh_t
retro_audio_sample_t
retro_audio_sample_batch_t
retro_input_poll_t
retro_input_state_t
```

No crear un frontend libretro independiente para cada sistema.

---

# Rendimiento esperado

Para:

```text
GB
GBC
GBA
```

priorizar:

```text
Accuracy
Compatibility
Stable timing
Low latency
Extra graphical features
```

La PS4 tiene margen suficiente para que estos sistemas no requieran optimizaciones agresivas comparables a N64.

---

# CPU y GPU

Para GB/GBC/GBA:

```text
CPU PS4
 ├── CPU emulada
 ├── PPU
 ├── core logic
 └── audio generation

GPU PS4
 ├── framebuffer output
 ├── scaling
 ├── shaders
 └── filters
```

No intentar mover la emulación de CPU portátil a GPU sin una razón medida.

---

# Profiling

Actualizar overlay:

```text
System
Core
FPS
Frame Time
Core Time
GPU Time
Audio Time
Fast Forward
Rewind Memory
```

Ejemplo:

```text
System          Game Boy Advance
Core            mGBA

FPS             59.73
Frame           16.74 ms

Core             1.42 ms
GPU              0.52 ms
Audio            0.18 ms

Rewind RAM       18 MB
```

No mostrar métricas inventadas.

---

# Configuración por sistema

Crear:

```text
config/systems/
├── gb.json
├── gbc.json
├── gba.json
└── n64.json
```

Ejemplo GBA:

```json
{
  "core": "mgba",

  "video": {
    "integer_scaling": true,
    "filter": "nearest"
  },

  "audio": {
    "latency_ms": 64
  },

  "features": {
    "rewind": true,
    "fast_forward": true
  }
}
```

---

# Configuración por juego

Prioridad:

```text
Global
  ↓
System
  ↓
Game
```

Ruta:

```text
configs/games/<system>/<gameid>.json
```

---

# Carátulas

Ruta:

```text
covers/<system>/<gameid>.png
```

Fallback:

```text
assets/systems/<system>/default_cover.png
```

---

# Screenshots

```text
screenshots/
├── gb/
├── gbc/
├── gba/
└── n64/
```

Formato recomendado:

```text
PNG
```

---

# Metadata

Guardar:

```text
System
Title
Game ID
Region
Path
Core
Favorite
Last Played
Play Time
Cover
```

---

# Flujo de arranque

```text
R2N64
  ↓
Load Config
  ↓
Initialize Platform
  ↓
Load Core Registry
  ↓
Scan Library
  ↓
Frontend
  ↓
Select Game
  ↓
Detect System
  ↓
Select Core
  ↓
Load Core
  ↓
Load ROM
  ↓
Run
```

---

# Cambio entre sistemas

No reiniciar toda la aplicación.

Al cambiar:

```text
GBA
 ↓
N64
```

hacer:

1. detener core activo;
2. guardar si corresponde;
3. liberar ROM;
4. limpiar audio;
5. liberar recursos específicos;
6. descargar core;
7. seleccionar nuevo core;
8. cargar configuración del nuevo sistema;
9. cargar ROM;
10. iniciar.

---

# Core Manager API

```cpp
class CoreManager
{
public:
    bool initialize();

    bool loadCoreForSystem(SystemType system);
    bool loadGame(const Game& game);

    void runFrame();
    void reset();

    void closeGame();
    void unloadCore();

    IEmulationCore* activeCore();

private:
    std::unique_ptr<IEmulationCore> m_core;
};
```

---

# Memoria

Cada core debe liberar:

```text
ROM data
audio buffers
video buffers
temporary allocations
save-state buffers
core-specific memory
```

No mantener varios cores pesados cargados simultáneamente en el MVP.

---

# Nintendo 64

No eliminar ni degradar las optimizaciones ya planificadas para N64.

Mantener:

```text
x86-64 Dynarec
RSP optimizado
GPU RDP
Shader Cache
Pipeline Cache
Texture Cache
Frame Scheduler
Profiling
```

La ampliación multi-sistema debe envolver el core N64 existente dentro de la nueva arquitectura.

---

# Compatibilidad

Estados:

```text
Perfect
Playable
In Game
Boots
Broken
Untested
```

Ejemplo:

```json
{
  "system": "gba",
  "game_id": "EXAMPLE",
  "status": "perfect",
  "core": "mgba",
  "notes": ""
}
```

---

# Logs

Ejemplo:

```text
[INFO] System detected: GBA
[INFO] Selected core: mGBA
[INFO] Loading core
[INFO] Loading ROM
[INFO] Video initialized: 240x160
[INFO] Audio initialized
[INFO] Game started
```

Errores deben identificar:

```text
System
Core
Error Code
```

---

# Dependencias

Agregar preferentemente como submodules:

```text
external/sameboy/
external/mgba/
```

Mantener:

```text
external/mupen64plus-next/
external/libretro/
```

---

# Licencias

Generar:

```text
THIRD_PARTY_LICENSES.md
```

Verificar licencias de:

```text
SameBoy
mGBA
Mupen64Plus-Next
libretro
SDL
OpenOrbis
```

---

# Contenido no incluido

No incluir:

```text
ROMs comerciales
BIOS propietarias
firmware propietario
keys
assets protegidos sin permiso
```

Si un core admite BIOS opcional, el usuario debe proveerla legalmente.

---

# Sprint 1 — Abstracción multi-core

Implementar:

```text
[ ] SystemType
[ ] CoreDescriptor
[ ] CoreRegistry
[ ] SystemDetector
[ ] CoreManager
[ ] detección de .gb
[ ] detección de .gbc
[ ] detección de .gba
[ ] mantener .z64/.n64/.v64
```

Criterio:

```text
La biblioteca detecta GB, GBC, GBA y N64 sin romper N64.
```

---

# Sprint 2 — Adaptar N64

Crear:

```text
Mupen64Core
```

que implemente:

```text
IEmulationCore
```

Criterio:

```text
Nintendo 64 funciona igual que antes,
pero ahora lo administra CoreManager.
```

---

# Sprint 3 — Game Boy

Integrar:

```text
SameBoy
```

Implementar:

```text
[ ] ROM loading
[ ] video
[ ] audio
[ ] input
[ ] native save
[ ] save states
[ ] integer scaling
```

---

# Sprint 4 — Game Boy Color

Usar SameBoy.

Implementar:

```text
[ ] .gbc
[ ] modo color
[ ] saves
[ ] save states
[ ] shader opcional
```

---

# Sprint 5 — Game Boy Advance

Integrar:

```text
mGBA
```

Implementar:

```text
[ ] .gba
[ ] 240x160 video
[ ] audio
[ ] L/R input
[ ] saves
[ ] save states
[ ] rewind
[ ] fast forward
```

---

# Sprint 6 — Biblioteca multi-sistema

```text
[ ] filtros por sistema
[ ] iconos por sistema
[ ] carátulas
[ ] metadata
[ ] favoritos
[ ] recientes
[ ] tiempo jugado
```

---

# Sprint 7 — Video portátil

```text
[ ] Integer Scaling
[ ] Nearest
[ ] Bilinear
[ ] GB LCD shader
[ ] GBC LCD shader
[ ] GBA LCD shader
[ ] perfiles por sistema
```

---

# Sprint 8 — Funciones avanzadas

```text
[ ] Rewind
[ ] Fast Forward
[ ] Auto Resume
[ ] Paletas GB
[ ] Config por juego
```

No bloquear el MVP por estas funciones.

---

# MVP

El MVP está completo cuando:

```text
[ ] N64 sigue funcionando
[ ] GB funciona
[ ] GBC funciona
[ ] GBA funciona
[ ] ROM scanner detecta todos los sistemas
[ ] Core Manager selecciona el core correcto
[ ] Video funciona
[ ] Audio funciona
[ ] DualShock funciona
[ ] Saves funcionan
[ ] Save states funcionan
[ ] Se puede cerrar un juego
[ ] Se puede abrir otro sistema sin reiniciar la app
```

---

# Matrix de pruebas

| Sistema | Boot | Video | Audio | Input | Save | State | Exit |
|---|---:|---:|---:|---:|---:|---:|---:|
| GB | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GBC | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GBA | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| N64 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

---

# Pruebas de regresión

Cada cambio en:

```text
Core Manager
Audio
Video
Input
Filesystem
Config
```

debe volver a probar Nintendo 64.

No aceptar una nueva función si rompe el core N64 existente.

---

# Orden recomendado

```text
1. SystemType
2. Core Registry
3. System Detector
4. Core Manager
5. Adaptar N64 existente
6. SameBoy
7. Game Boy
8. Game Boy Color
9. mGBA
10. Game Boy Advance
11. Biblioteca multi-sistema
12. Config por sistema
13. Rewind
14. Fast Forward
15. Shaders
```

---

# Primera orden para Codex

Antes de modificar código:

1. analizar la estructura actual;
2. identificar cómo está integrado Mupen64Plus-Next;
3. localizar video, audio, input, saves y ROM scanner;
4. no eliminar funcionalidad N64;
5. crear la abstracción mínima multi-core;
6. compilar;
7. ejecutar pruebas de regresión N64.

Primera implementación concreta:

```text
SystemType
CoreDescriptor
CoreRegistry
SystemDetector
```

y actualizar el scanner para:

```text
.gb
.gbc
.gba
.z64
.n64
.v64
```

Todavía no integrar SameBoy ni mGBA en ese primer cambio.

---

# Reporte esperado de Codex

Después de cada iteración mostrar:

```text
Changed files
Systems affected
Build status
N64 regression status
GB/GBC/GBA status
Tests executed
Known issues
Next recommended task
```

---

# Regla arquitectónica final

El proyecto debe pasar de:

```text
R2N64
   │
   └── Mupen64Plus
```

a:

```text
Frontend
   │
   ▼
Core Manager
   │
   ├── SameBoy
   ├── mGBA
   └── Mupen64Plus-Next
```

sin convertir el frontend en código específico de cada emulador.

Principio:

```text
UN FRONTEND
MÚLTIPLES CORES
MÚLTIPLES SISTEMAS
UNA SOLA EXPERIENCIA DE USUARIO
```
