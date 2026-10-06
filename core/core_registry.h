#pragma once
#include "core/system_type.h"
#include <array>
#include <cstddef>

namespace r2n64 {
struct CoreDescriptor {
    const char* id;
    const char* name;
    std::array<SystemType, 2> systems;
    const char* extensions;
    bool available;
    bool supports(SystemType system) const;
};

class CoreRegistry {
public:
    static const std::array<CoreDescriptor, 5>& all();
    // Returns a descriptor even when its optional library is not compiled in.
    // This makes an unsupported build an explicit error, never an N64 fallback.
    static const CoreDescriptor* forSystem(SystemType system);
};
}
