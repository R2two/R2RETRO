#include <cstdio>
extern "C" unsigned fceumm_retro_api_version(void);
extern "C" unsigned bsnes_mercury_retro_api_version(void);
int main() {
    const unsigned nes = fceumm_retro_api_version();
    const unsigned snes = bsnes_mercury_retro_api_version();
    std::printf("FCEUmm libretro API %u; bsnes-mercury libretro API %u\n", nes, snes);
    return (nes == 1 && snes == 1) ? 0 : 1;
}
