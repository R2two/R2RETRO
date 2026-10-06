#pragma once
#include "library_metadata.h"
#include "rom.h"
#include <SDL2/SDL.h>
#include <atomic>
#include <memory>
#include <thread>

namespace r2n64 {
struct LibrarySurfaceDeleter { void operator()(SDL_Surface* surface) const { if (surface) SDL_FreeSurface(surface); } };
struct LibraryResult {
    Game game;
    LibraryMetadata metadata;
    std::unique_ptr<SDL_Surface, LibrarySurfaceDeleter> artwork;
    std::string error;
    bool found = false, downloaded = false, cancelled = false;
};
// One worker owns network, ROM hashing, cache I/O and image decoding. SDL
// textures remain exclusively on the main thread. Never start while playing.
class LibraryService {
public:
    ~LibraryService();
    bool start(const std::string& dataRoot, const std::string& caFile, const Game& game, bool download);
    bool poll(LibraryResult& result);
    void cancel() { cancelled_ = true; }
    void stop(); // Shutdown only; ordinary cancellation stays nonblocking.
    bool busy() const { return worker_.joinable(); }
    bool downloading() const { return busy() && downloading_; }
private:
    std::thread worker_;
    std::atomic<bool> cancelled_{false}, ready_{false};
    bool downloading_ = false;
    LibraryResult pending_;
};
}
