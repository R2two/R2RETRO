#include "library_service.h"
#include <SDL2/SDL_image.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace r2n64 {
namespace {
uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
SDL_Surface* decodeArtwork(const std::string& path, const std::atomic<bool>& cancel, std::string& error) {
    if (path.empty() || cancel) return nullptr;
    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) { error = "No se pudo abrir la imagen guardada."; return nullptr; }
    struct Close { int fd; ~Close() { ::close(fd); } } close{fd};
    struct stat st{};
    constexpr size_t limit = 8 * 1024 * 1024;
    if (::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 33 || uint64_t(st.st_size) > limit) {
        error = "La imagen guardada supera los limites admitidos."; return nullptr;
    }
    std::vector<uint8_t> bytes(size_t(st.st_size), 0);
    size_t offset = 0;
    while (offset < bytes.size()) {
        if (cancel) return nullptr;
        const auto count = ::read(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { error = "La imagen guardada esta incompleta."; return nullptr; }
        offset += size_t(count);
    }
    const uint8_t png[] = {137,80,78,71,13,10,26,10};
    const uint32_t width = be32(bytes.data() + 16), height = be32(bytes.data() + 20);
    if (std::memcmp(bytes.data(), png, 8) || be32(bytes.data() + 8) != 13 ||
        std::memcmp(bytes.data() + 12, "IHDR", 4) || !width || !height || width > 2048 || height > 2048) {
        error = "La imagen guardada no es un PNG compatible."; return nullptr;
    }
    auto* stream = SDL_RWFromConstMem(bytes.data(), int(bytes.size()));
    if (!stream) { error = "No se pudo preparar la imagen."; return nullptr; }
    auto* surface = IMG_Load_RW(stream, 1);
    if (!surface) error = "No se pudo decodificar la imagen guardada.";
    return surface;
}
}
LibraryService::~LibraryService() {
    stop();
}
void LibraryService::stop() {
    cancelled_ = true;
    if (worker_.joinable()) worker_.join();
}
bool LibraryService::start(const std::string& root, const std::string& ca, const Game& game, bool download) {
    if (busy()) return false;
    cancelled_ = false;
    ready_ = false;
    downloading_ = download;
    pending_ = LibraryResult{};
    try {
        worker_ = std::thread([this, root, ca, game, download]() {
            pending_.game = game;
            pending_.downloaded = download;
            try {
                pending_.found = download ? fetchLibraryMetadata(root, ca, game, cancelled_, pending_.metadata, pending_.error) :
                    loadCachedMetadata(root, game, pending_.metadata, pending_.error, &cancelled_);
                if (pending_.found && !cancelled_) {
                    const auto& m = pending_.metadata;
                    const std::string paths[] = {m.coverPath, m.screenshotPath, m.titleScreenPath};
                    for (const auto& path : paths) {
                        if (path.empty() || cancelled_) continue;
                        std::string decodeError;
                        pending_.artwork.reset(decodeArtwork(path, cancelled_, decodeError));
                        if (pending_.artwork) break;
                        if (!decodeError.empty() && pending_.error.empty()) pending_.error = decodeError;
                    }
                }
            } catch (const std::exception&) {
                pending_.found = false;
                pending_.error = "No se pudo preparar la ficha del juego.";
            } catch (...) {
                pending_.found = false;
                pending_.error = "Error al preparar la ficha del juego.";
            }
            pending_.cancelled = cancelled_;
            ready_ = true;
        });
    } catch (...) { downloading_ = false; return false; }
    return true;
}
bool LibraryService::poll(LibraryResult& result) {
    if (!busy() || !ready_) return false;
    worker_.join();
    result = std::move(pending_);
    downloading_ = false;
    ready_ = false;
    return true;
}
}
