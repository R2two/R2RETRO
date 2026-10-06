#include "core/core_manager.h"
#include "core/core_registry.h"
#include "core/libretro_core.h"
#include "frontend/system_detector.h"

namespace r2n64 {
bool CoreManager::load(const std::string& romPath, const std::string& dataRoot,
                       Log& log, std::string& error, EmulationConfig config) {
    unload();
    const auto system = detectSystemFromExtension(romPath);
    const auto* descriptor = CoreRegistry::forSystem(system);
    if (!descriptor) { error = "Extension de ROM no compatible."; return false; }
    const auto* api = coreApiFor(system);
    if (!descriptor->available || !api) {
        error = std::string("Este paquete no incluye el nucleo ") + descriptor->name + ".";
        return false;
    }
    core_.reset(new LibretroCore(*descriptor, *api));
    return core_->load(romPath, dataRoot, log, error, config);
}
void CoreManager::unload() { if (core_) core_->unload(); }
}
