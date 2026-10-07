#pragma once
#include "rom.h"
#include "video.h"
#include "menu.h"
#include "library_metadata.h"
#include "update.h"
#include <string>
#include <vector>
namespace r2n64 {
struct View {
    const std::vector<Game>* games = nullptr;
    const std::vector<Game>* allGames = nullptr;
    std::array<size_t, librarySystems.size()> consoleCounts{};
    const LibraryMetadata* metadata = nullptr; // Only the current library selection.
    Menu menu;
    std::vector<std::string> roots;
    bool scanning = false, connected = false, storage = false, desktop = false;
    bool background = false;
    bool downloading = false;
    float categoryPosition = 0, itemOffset = 0, contentAlpha = 1;
    std::string platform, dataPath, message, version, dataError;
    std::string libraryStatus;
    bool updatesOpen=false, updateAvailable=false, updateDownloaded=false, updateBusy=false, updateConfirm=false;
    size_t updateSelection=0;
    UpdatePreferences updatePreferences;
    UpdateRelease updateRelease;
    std::string updateStatus="Busca nuevas versiones de R2RETRO.";
    std::string updateDiagnostic;
    bool updateInstalling=false;
};
void renderUI(Video& video, const View& view);
}
