#pragma once
#include "rom.h"
#include <atomic>
#include <string>

namespace r2n64 {
struct LibraryMetadata {
    std::string title, region, developer, publisher, genre, year, players;
    std::string coverPath, screenshotPath, titleScreenPath, match;
};
// Both functions belong on a worker: cache validation hashes the local ROM.
// A missing cache returns false with an empty error. Failures clear the output.
bool loadCachedMetadata(const std::string& dataRoot, const Game& game,
                        LibraryMetadata& metadata, std::string& error,
                        const std::atomic<bool>* cancel = nullptr);
// Official Libretro databases/thumbnails only, requested explicitly by the user.
// Success may have missing images; optional-image warnings are returned in error.
// No approximate title matching. Neither Game nor any save/state ID is modified.
bool fetchLibraryMetadata(const std::string& dataRoot, const std::string& caFile,
                          const Game& game, const std::atomic<bool>& cancel,
                          LibraryMetadata& metadata, std::string& error);
}
