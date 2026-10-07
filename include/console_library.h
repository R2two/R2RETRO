#pragma once
#include "core/system_catalog.h"
#include "rom.h"

namespace r2n64 {
struct ConsoleLibrary {
    std::array<size_t, librarySystems.size()> counts{};
    std::vector<Game> games;
    // Unknown represents the console directory: it intentionally contains no ROMs.
    void refresh(const std::vector<Game>& all, SystemType selected) {
        counts.fill(0);
        games.clear();
        for (const auto& game : all) {
            const auto folder = libraryFolder(game.system);
            if (!folder) continue;
            ++counts[folder - 1];
            if (game.system == selected) games.push_back(game);
        }
    }
};
}
