#pragma once

namespace r2n64 {
// Append systems: persisted states use the existing numeric values.
enum class SystemType { Unknown, GameBoy, GameBoyColor, GameBoyAdvance, Nintendo64,
                        NintendoEntertainmentSystem, SuperNintendo };

constexpr const char* systemId(SystemType system) {
    switch (system) {
    case SystemType::GameBoy: return "gb";
    case SystemType::GameBoyColor: return "gbc";
    case SystemType::GameBoyAdvance: return "gba";
    case SystemType::Nintendo64: return "n64";
    case SystemType::NintendoEntertainmentSystem: return "nes";
    case SystemType::SuperNintendo: return "snes";
    default: return "unknown";
    }
}
constexpr const char* systemName(SystemType system) {
    switch (system) {
    case SystemType::GameBoy: return "Game Boy";
    case SystemType::GameBoyColor: return "Game Boy Color";
    case SystemType::GameBoyAdvance: return "Game Boy Advance";
    case SystemType::Nintendo64: return "Nintendo 64";
    case SystemType::NintendoEntertainmentSystem: return "Nintendo Entertainment System";
    case SystemType::SuperNintendo: return "Super Nintendo";
    default: return "Sin identificar";
    }
}
}
