#include "core/core_registry.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace r2n64;
int main() {
    try {
        const auto require = [](bool ok, const char* why) { if (!ok) throw std::runtime_error(why); };
        const auto* n64 = CoreRegistry::forSystem(SystemType::Nintendo64);
        const auto* gb = CoreRegistry::forSystem(SystemType::GameBoy);
        const auto* gbc = CoreRegistry::forSystem(SystemType::GameBoyColor);
        const auto* gba = CoreRegistry::forSystem(SystemType::GameBoyAdvance);
        const auto* nes = CoreRegistry::forSystem(SystemType::NintendoEntertainmentSystem);
        const auto* snes = CoreRegistry::forSystem(SystemType::SuperNintendo);
        require(n64 && n64->available && std::string(n64->id) == "mupen64plus_next", "N64 registry changed");
        require(gb && gb == gbc && std::string(gb->id) == "sameboy", "GB/GBC must share SameBoy");
        require(gba && gba != gb && std::string(gba->id) == "mgba", "GBA must select mGBA");
        require(nes && std::string(nes->id) == "fceumm" && std::string(nes->extensions) == "nes", "NES must select FCEUmm without FDS");
        require(snes && std::string(snes->id) == "bsnes_mercury" && std::string(snes->extensions) == "sfc|smc",
                "SNES must select bsnes-mercury");
        require(CoreRegistry::all().size() == 5, "registry must include five distinct cores");
        require(static_cast<int>(SystemType::Unknown) == 0 && static_cast<int>(SystemType::GameBoy) == 1 &&
                static_cast<int>(SystemType::GameBoyColor) == 2 && static_cast<int>(SystemType::GameBoyAdvance) == 3 &&
                static_cast<int>(SystemType::Nintendo64) == 4 && static_cast<int>(SystemType::NintendoEntertainmentSystem) == 5 &&
                static_cast<int>(SystemType::SuperNintendo) == 6, "persisted system ordinals changed");
#ifdef R2N64_HAS_FCEUMM
        require(nes->available, "compiled NES core not available");
#else
        require(!nes->available, "missing NES library reported available");
#endif
#ifdef R2N64_HAS_BSNES_MERCURY
        require(snes->available, "compiled SNES core not available");
#else
        require(!snes->available, "missing SNES library reported available");
#endif
        require(!CoreRegistry::forSystem(SystemType::Unknown), "Unknown system must not select a core");
        for (const auto& entry : CoreRegistry::all())
            require(!entry.supports(SystemType::Unknown), "Empty registry slot accepted as a system");
        std::cout << "PASS: explicit core selection and unsupported-system rejection\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
