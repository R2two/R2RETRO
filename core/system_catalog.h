#pragma once
#include "core/system_type.h"
#include <array>
#include <cstddef>

namespace r2n64 {
// Library navigation order, independent of persisted SystemType numeric IDs.
inline constexpr std::array<SystemType, 6> librarySystems{{
    SystemType::Nintendo64, SystemType::NintendoEntertainmentSystem,
    SystemType::SuperNintendo, SystemType::GameBoy,
    SystemType::GameBoyColor, SystemType::GameBoyAdvance
}};
constexpr unsigned libraryFolder(SystemType system) {
    for (size_t i = 0; i < librarySystems.size(); ++i)
        if (librarySystems[i] == system) return static_cast<unsigned>(i + 1);
    return 0;
}
}
