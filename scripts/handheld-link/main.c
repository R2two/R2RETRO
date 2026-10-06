#include <stdio.h>
unsigned sameboy_retro_api_version(void);
unsigned mgba_retro_api_version(void);
int main(void) {
    const unsigned gb = sameboy_retro_api_version();
    const unsigned gba = mgba_retro_api_version();
    printf("SameBoy libretro API %u; mGBA libretro API %u\n", gb, gba);
    return (gb == 1 && gba == 1) ? 0 : 1;
}
