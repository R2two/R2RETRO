#pragma once
#include "core/system_type.h"
#include <string>

namespace r2n64 {
struct HandheldSettings {
    unsigned fastForward = 2;
    unsigned stateSlot = 0;
    bool integerScaling = true;
    bool linearFilter = false;
    unsigned gbPalette = 0;
    bool overlay = true;
    bool showStats = true;
    unsigned shader = 0; // 0 off, 1 LCD grid, 2 CRT scanlines (GLES2).
    unsigned gbaFrameskip = 0; // Native mGBA renderer: skip 0, 1 or 2 frames.
    bool gbColor = false; // GB only; preserves gbPalette for switching back.
};

// configs/systems/{gb,gbc,gba,nes,snes}.json. NES/SNES default to 4:3; SNES
// artwork defaults on, NES off. GB/GBC/GBA retain integer scaling and artwork.
// An explicit overlay=false in an existing file is always preserved.
// Missing files/directories return system defaults
// successfully; invalid files return defaults and an error without changing disk.
// Version 1 requires the original six fields; shader/gbaFrameskip are optional
// integers 0..2, default off. gbColor is an optional boolean, default false.
// overlay and showStats are optional
// and default to the system values above (showStats always true). Unknown scalar fields are ignored
// and omitted on the next save; duplicate keys, nested values and files above
// 4096 bytes are rejected. Version changes are rejected rather than guessed.
bool loadHandheldSettings(const std::string& root, SystemType system,
                          HandheldSettings& settings, std::string& error);
// Validates first, creates real directories as needed, then fsyncs a unique
// temporary file and atomically renames it. Links/special files are rejected.
bool saveHandheldSettings(const std::string& root, SystemType system,
                          const HandheldSettings& settings, std::string& error);
const char* gbPaletteName(unsigned palette);
}
