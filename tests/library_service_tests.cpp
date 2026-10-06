#include "library_service.h"
#include <SDL2/SDL_image.h>
#include <cassert>
#include <chrono>
#include <cstring>
#include <fstream>
#include <thread>
#include <unistd.h>

namespace {
std::string imagePath;
std::thread::id mainThread;
std::atomic<bool> reached{false};
void finish(r2n64::LibraryService& service, r2n64::LibraryResult& result) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!service.poll(result)) {
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(!service.busy());
}
}
namespace r2n64 {
bool loadCachedMetadata(const std::string&, const Game& game, LibraryMetadata& metadata, std::string& error,
    const std::atomic<bool>* cancel) {
    assert(std::this_thread::get_id() != mainThread);
    metadata = {}; error.clear();
    if (game.title == "slow-cache") {
        reached = true;
        while (cancel && !*cancel) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        error = "Cancelled"; return false;
    }
    if (game.title == "missing") return false;
    metadata.title = "Original diagnostic";
    metadata.coverPath = imagePath;
    return true;
}
bool fetchLibraryMetadata(const std::string&, const std::string&, const Game&,
    const std::atomic<bool>& cancel, LibraryMetadata&, std::string& error) {
    assert(std::this_thread::get_id() != mainThread);
    reached = true;
    while (!cancel) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    error = "Cancelled"; return false;
}
}
int main() {
    using namespace r2n64;
    mainThread = std::this_thread::get_id();
    assert(SDL_Init(0) == 0);
    assert(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG);
    char temporary[] = "/tmp/r2n64-library-service-XXXXXX";
    const char* directory = mkdtemp(temporary); assert(directory);
    imagePath = std::string(directory) + "/original.png";
    auto* surface = SDL_CreateRGBSurfaceWithFormat(0, 32, 48, 32, SDL_PIXELFORMAT_RGBA32);
    assert(surface);
    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 24, 160, 90, 255));
    assert(IMG_SavePNG(surface, imagePath.c_str()) == 0);
    SDL_FreeSurface(surface);
    Game game; game.title = "fixture"; game.id = "original";
    LibraryService service;
    assert(service.start(directory, "", game, false));
    assert(!service.start(directory, "", game, false));
    LibraryResult result;
    finish(service, result);
    assert(result.found && !result.downloaded && !result.cancelled);
    assert(result.artwork && result.artwork->w == 32 && result.artwork->h == 48);
    assert(service.start(directory, "", game, true));
    assert(service.downloading());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!reached) { assert(std::chrono::steady_clock::now() < deadline); std::this_thread::yield(); }
    service.cancel(); finish(service, result);
    assert(result.downloaded && result.cancelled && !result.found && !result.artwork);
    game.title = "slow-cache"; reached = false;
    assert(service.start(directory, "", game, false));
    const auto cacheDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!reached) { assert(std::chrono::steady_clock::now() < cacheDeadline); std::this_thread::yield(); }
    service.cancel(); finish(service, result);
    assert(!result.downloaded && result.cancelled && !result.found);
    game.title = "missing";
    assert(service.start(directory, "", game, false)); finish(service, result);
    assert(!result.found && result.error.empty() && !result.artwork);
    game.title = "fixture";
    // Reject hostile dimensions before SDL_image allocates a decode surface.
    std::fstream png(imagePath, std::ios::binary | std::ios::in | std::ios::out);
    png.seekp(16); const unsigned char huge[] = {0x7f,0xff,0xff,0xff};
    png.write(reinterpret_cast<const char*>(huge), 4); png.close();
    assert(service.start(directory, "", game, false)); finish(service, result);
    assert(result.found && !result.artwork && !result.error.empty());
    assert(service.start(directory, "", game, true));
    service.stop(); assert(!service.busy());
    ::unlink(imagePath.c_str()); ::rmdir(directory);
    IMG_Quit(); SDL_Quit();
}
