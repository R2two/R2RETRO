#include "storage.h"
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
namespace r2n64 {
static bool directory(const std::string& path, std::string& error) {
    if (mkdir(path.c_str(), 0700) < 0 && errno != EEXIST) {
        error = path + ": " + std::strerror(errno); return false;
    }
    struct stat info{};
    if (lstat(path.c_str(), &info) < 0 || !S_ISDIR(info.st_mode)) {
        error = path + ": se requiere un directorio real, sin enlace simbolico"; return false;
    }
    return true;
}
bool prepareStorage(const std::string& root, std::string& error) {
    error.clear();
    if (root.empty() || !directory(root, error)) return false;
    for (const auto* name : {"roms", "covers", "saves", "states", "screenshots", "cache", "configs", "logs"})
        if (!directory(root + "/" + name, error)) return false;
    if (!directory(root + "/configs/games", error)) return false;
    // Keep existing N64 files in their legacy locations; do not migrate saves.
    for (const auto* name : {"roms", "covers", "saves", "states", "screenshots", "configs/games"})
        for (const auto* system : {"gb", "gbc", "gba", "n64", "nes", "snes"})
            if (!directory(root + "/" + name + "/" + system, error)) return false;
    return true;
}
}
