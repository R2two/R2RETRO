#include "core/core_registry.h"

namespace r2n64 {
bool CoreDescriptor::supports(SystemType system) const {
    return system != SystemType::Unknown && (systems[0] == system || systems[1] == system);
}
const std::array<CoreDescriptor, 5>& CoreRegistry::all() {
    static const std::array<CoreDescriptor, 5> entries{{
        {"mupen64plus_next", "Mupen64Plus-Next", {SystemType::Nintendo64, SystemType::Unknown}, "z64|n64|v64", true},
        {"sameboy", "SameBoy", {SystemType::GameBoy, SystemType::GameBoyColor}, "gb|gbc",
#ifdef R2N64_HAS_SAMEBOY
            true
#else
            false
#endif
        },
        {"mgba", "mGBA", {SystemType::GameBoyAdvance, SystemType::Unknown}, "gba",
#ifdef R2N64_HAS_MGBA
            true
#else
            false
#endif
        },
        {"fceumm", "FCEUmm", {SystemType::NintendoEntertainmentSystem, SystemType::Unknown}, "nes",
#ifdef R2N64_HAS_FCEUMM
            true
#else
            false
#endif
        },
        {"bsnes_mercury", "bsnes-mercury", {SystemType::SuperNintendo, SystemType::Unknown}, "sfc|smc",
#ifdef R2N64_HAS_BSNES_MERCURY
            true
#else
            false
#endif
        }
    }};
    return entries;
}
const CoreDescriptor* CoreRegistry::forSystem(SystemType system) {
    for (const auto& core : all()) if (core.supports(system)) return &core;
    return nullptr;
}
}
