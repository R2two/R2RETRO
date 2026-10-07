#include "n64_profiles.h"

namespace r2n64 {
const char* applyN64Profile(const std::string& identity, size_t bytes, EmulationConfig& config) {
    if (!config.automaticProfile) return "Manual";
    struct Entry { const char* identity; size_t bytes; const char* label; };
    static constexpr Entry entries[] = {
        {"635A2BFF-8B022326-623B80DDBD00E7B7", 8*1024*1024, "Mario USA · experimental"},
        {"693BA2AE-B7F14E9F-6E8332B2F362A221", 32*1024*1024, "Zelda USA 1.2 · experimental"}
    };
    for (const auto& entry : entries) {
        if (bytes != entry.bytes || identity != entry.identity) continue;
        if (!config.hardware) return "Automático: sin contexto GPU; ajustes base";
        config.graphics = N64Graphics::Gles2;
        config.graphicsHle = true;
        config.cpuMode = CpuMode::Automatic;
        config.audioHle = true;
        // Retain the user's profiling switch and CPU renderer worker count.
        // No emulated-clock, framebuffer, precision or frameskip shortcuts.
        return entry.label;
    }
    return "Automático: sin perfil; ajustes base";
}
}
