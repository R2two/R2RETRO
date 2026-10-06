#pragma once
#include "rom.h"
#include "video.h"
#include "menu.h"
#include "library_metadata.h"
#include <string>
#include <vector>
namespace r2n64 {
struct View {
    const std::vector<Game>* games = nullptr;
    const LibraryMetadata* metadata = nullptr; // Only the current library selection.
    Menu menu;
    std::vector<std::string> roots;
    bool scanning = false, connected = false, storage = false, desktop = false;
    bool background = false;
    bool downloading = false;
    float categoryPosition = 0, itemOffset = 0, contentAlpha = 1;
    std::string platform, dataPath, message, version, dataError;
    std::string libraryStatus;
};
void renderUI(Video& video, const View& view);
}
