// Link validation only. No SELF/PKG is generated from this executable.
// Pulling libretro.o also checks resolution of its ROM/run/state dependencies.
#include <libretro.h>
int main() {
    retro_system_info info{};
    retro_get_system_info(&info);
    return retro_api_version() == RETRO_API_VERSION && info.library_name ? 0 : 1;
}
