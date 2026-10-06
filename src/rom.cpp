#include "rom.h"
#include "file_ops.h"
#include "frontend/system_detector.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>

namespace r2n64 {
static uint32_t be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
bool parseHeader(std::array<uint8_t, 64> h, Game& game, std::string& error) {
    game = Game{};
    error.clear();
    switch (be32(h.data())) {
    case 0x80371240: game.order = RomOrder::BigEndian; break;
    case 0x37804012:
        game.order = RomOrder::ByteSwapped;
        for (size_t i = 0; i < h.size(); i += 2) std::swap(h[i], h[i + 1]);
        break;
    case 0x40123780:
        game.order = RomOrder::LittleEndian;
        for (size_t i = 0; i < h.size(); i += 4) std::reverse(h.begin() + i, h.begin() + i + 4);
        break;
    default: error = "R2N64-ROM-001: cabecera N64 no reconocida"; return false;
    }
    game.system = SystemType::Nintendo64;
    for (size_t i = 0x20; i < 0x34; ++i)
        game.title += h[i] >= 32 && h[i] <= 126 ? char(h[i]) : ' ';
    const auto last = game.title.find_last_not_of(' ');
    game.title = last == std::string::npos ? "Sin titulo" : game.title.substr(0, last + 1);
    const auto first = game.title.find_first_not_of(' ');
    game.title.erase(0, first);
    game.crc1 = be32(h.data() + 0x10);
    game.crc2 = be32(h.data() + 0x14);
    char id[18];
    std::snprintf(id, sizeof(id), "%08X-%08X", game.crc1, game.crc2);
    game.id = id;
    // Raw cartridge identifier, not an inferred database product code.
    for (size_t i : {size_t(0x3b), size_t(0x3c), size_t(0x3d), size_t(0x3e)})
        game.serial += h[i] >= 32 && h[i] <= 126 ? char(h[i]) : '?';
    switch (h[0x3e]) {
    case 'E': game.region = "USA / NTSC"; break;
    case 'J': game.region = "Japon / NTSC"; break;
    case 'B': game.region = "Brasil / MPAL"; break;
    case 'P': case 'D': case 'F': case 'I': case 'S': case 'U': case 'X': case 'Y':
        game.region = "PAL"; break;
    default: game.region = "Sin identificar"; break;
    }
    return true;
}
namespace {
class File {
public:
    explicit File(int descriptor) : fd(descriptor) {}
    ~File() { if (fd >= 0) ::close(fd); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    int fd;
};
bool readExact(int fd, uint8_t* bytes, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        const auto count = ::read(fd, bytes + offset, length - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        offset += size_t(count);
    }
    return true;
}
std::string titleText(const uint8_t* bytes, size_t length) {
    std::string result;
    for (size_t i = 0; i < length && bytes[i]; ++i)
        result += bytes[i] >= 32 && bytes[i] <= 126 ? char(bytes[i]) : ' ';
    const auto last = result.find_last_not_of(' ');
    if (last == std::string::npos) return "Sin titulo";
    result.resize(last + 1);
    result.erase(0, result.find_first_not_of(' '));
    return result;
}
std::string filenameTitle(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    const size_t start = slash == std::string::npos ? 0 : slash + 1;
    const auto dot = path.find_last_of('.');
    std::string title = path.substr(start, dot == std::string::npos || dot < start ? std::string::npos : dot - start);
    for (char& value : title) if (static_cast<unsigned char>(value) < 32 || value == 127) value = ' ';
    return title.empty() ? "Sin titulo" : title;
}
bool readOpenedRom(int fd, const std::string& path, Game& game, std::string& error,
                   const std::atomic<bool>* cancel = nullptr) {
    game = Game{};
    error.clear();
    const auto system = detectSystemFromExtension(path);
    if (system == SystemType::Unknown) {
        error = "R2N64-ROM-008: extension de ROM no soportada"; return false;
    }
    struct stat info{};
    if (fstat(fd, &info) < 0 || !S_ISREG(info.st_mode) || info.st_size < 0) {
        error = "R2N64-ROM-002: archivo no regular"; return false;
    }
    if (uint64_t(info.st_size) > maximumRomFileSize(system)) {
        error = "R2N64-ROM-002: archivo supera el limite de tamano de este sistema"; return false;
    }
    const size_t headerSize = systemHeaderReadSize(system, uint64_t(info.st_size));
    std::vector<uint8_t> header(headerSize);
    if (uint64_t(info.st_size) < headerSize || !readExact(fd, header.data(), headerSize)) {
        error = "R2N64-ROM-002: archivo truncado"; return false;
    }
    if (!validateSystemHeader(system, header.data(), headerSize, uint64_t(info.st_size), error)) return false;
    if (system == SystemType::Nintendo64) {
        std::array<uint8_t, 64> n64Header{};
        std::copy_n(header.begin(), n64Header.size(), n64Header.begin());
        if (!parseHeader(n64Header, game, error)) return false;
    } else {
        // Content IDs include the complete cartridge, not its title/code. SNES
        // copier metadata is omitted so .smc/.sfc copies share saves/states.
        // Existing GB/GBC/GBA fingerprints retain their exact previous bytes.
        uint64_t hash = UINT64_C(14695981039346656037);
        const auto hashBytes = [&hash](const uint8_t* bytes, size_t count) {
            for (size_t i = 0; i < count; ++i) { hash ^= bytes[i]; hash *= UINT64_C(1099511628211); }
        };
        const size_t skipped = system == SystemType::SuperNintendo ? snesCopierHeaderSize(uint64_t(info.st_size)) : 0;
        hashBytes(header.data() + skipped, headerSize - skipped);
        std::array<uint8_t, 16384> block{};
        uint64_t remaining = uint64_t(info.st_size) - headerSize;
        while (remaining) {
            if (cancel && *cancel) { error = "Escaneo cancelado"; return false; }
            const size_t count = size_t(std::min<uint64_t>(remaining, block.size()));
            if (!readExact(fd, block.data(), count)) { error = "R2N64-ROM-002: archivo truncado"; return false; }
            hashBytes(block.data(), count);
            remaining -= count;
        }
        game.system = system;
        char id[32];
        std::snprintf(id, sizeof(id), "%s-%016llX", systemId(system), static_cast<unsigned long long>(hash));
        game.id = id;
        if (system == SystemType::GameBoyAdvance) {
            game.title = titleText(header.data() + 0xa0, 12);
            game.serial = titleText(header.data() + 0xac, 4);
            game.region = "Sin identificar";
        } else if (system == SystemType::GameBoy || system == SystemType::GameBoyColor) {
            game.title = titleText(header.data() + 0x134, header[0x143] & 0x80 ? 15 : 16);
            game.region = header[0x14a] == 0 ? "Japon / internacional" :
                          header[0x14a] == 1 ? "Internacional" : "Sin identificar";
        } else if (system == SystemType::NintendoEntertainmentSystem) {
            // iNES has no embedded title. The filename is display-only; identity
            // remains content-based when the user renames a cartridge.
            game.title = filenameTitle(path);
            const bool nes2 = (header[7] & 0x0c) == 0x08;
            const unsigned timing = nes2 ? header[12] & 3 : header[9] & 1;
            game.region = timing == 0 ? "NTSC" : timing == 1 ? "PAL" : timing == 2 ? "NTSC / PAL" : "Dendy";
        } else if (system == SystemType::SuperNintendo) {
            size_t offset;
            if (!selectSnesHeader(header.data(), header.size(), uint64_t(info.st_size), offset, error)) return false;
            game.title = titleText(header.data() + offset, 21);
            if (game.title == "Sin titulo") game.title = filenameTitle(path);
            const auto region = header[offset + 0x19] & 0x7f;
            game.region = region == 0 ? "Japon / NTSC" : region == 1 ? "USA / NTSC" :
                          region >= 2 && region <= 12 ? "PAL" : region <= 16 ? "NTSC" : "Sin identificar";
        }
    }
    game.path = path;
    game.size = uint64_t(info.st_size);
    return true;
}
}

bool readRom(const std::string& path, Game& game, std::string& error) {
    game = Game{};
    // Open nonblocking and reject non-regular files before reading (USB and untrusted names).
    File file(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK));
    if (file.fd < 0) { error = "No se puede abrir ROM: " + std::string(std::strerror(errno)); return false; }
    return readOpenedRom(file.fd, path, game, error);
}
ScanResult scanRoms(const std::vector<std::string>& roots, const std::atomic<bool>& cancel) {
    ScanResult result;
    std::set<std::pair<dev_t, ino_t>> seenDirectories;
    const auto warn = [&](const std::string& path, const char* operation, int code) {
        result.warnings.push_back(path + " [" + operation + "]: " + std::strerror(code) +
                                  " (errno " + std::to_string(code) + ")");
    };
    const auto scanDirectory = [&](int fd, const std::string& path) {
        if (cancel) { ::close(fd); return; }
        struct stat info{};
        if (fstat(fd, &info) < 0) {
            const int code = errno;
            ::close(fd);
            warn(path, "fstat directorio", code);
            return;
        }
        if (!S_ISDIR(info.st_mode)) {
            ::close(fd);
            warn(path, "fstat tipo de directorio", ENOTDIR);
            return;
        }
        if (!seenDirectories.emplace(info.st_dev, info.st_ino).second) { ::close(fd); return; }
        DIR* dir = fdopendir(fd); // Own the no-follow descriptor through closedir.
        if (!dir) {
            const int code = errno;
            ::close(fd);
            warn(path, "fdopendir", code);
            return;
        }
        while (!cancel) {
            errno = 0;
            auto* entry = readdir(dir);
            if (!entry) {
                const int code = errno;
                if (code) warn(path, "readdir", code);
                break;
            }
            const std::string name(entry->d_name);
            if (detectSystemFromExtension(name) == SystemType::Unknown) continue;
            const std::string gamePath = path + "/" + name;
            File file(fileops::openAt(dirfd(dir), path, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK));
            if (file.fd < 0) {
                const int code = errno;
                warn(gamePath, "open ROM", code);
                continue;
            }
            Game game;
            std::string error;
            if (readOpenedRom(file.fd, gamePath, game, error, &cancel)) result.games.push_back(std::move(game));
            else if (!cancel) result.warnings.push_back(gamePath + ": " + error);
        }
        closedir(dir);
    };
    for (const auto& root : roots) {
        if (cancel) break;
        File directory(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK));
        if (directory.fd < 0) {
            const int code = errno;
            if (code != ENOENT) warn(root, "open raiz", code);
            continue;
        }
        const int rootScan = ::dup(directory.fd);
        if (rootScan >= 0) scanDirectory(rootScan, root);
        else {
            const int code = errno;
            warn(root, "dup raiz", code);
        }
        for (const auto* name : {"gb", "gbc", "gba", "n64", "nes", "snes"}) {
            if (cancel) break;
            const auto path = root + "/" + name;
            const int sub = fileops::openAt(directory.fd, root, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK);
            if (sub >= 0) scanDirectory(sub, path);
            else {
                const int code = errno;
                if (code != ENOENT) warn(path, "open sistema", code);
            }
        }
    }
    std::sort(result.games.begin(), result.games.end(), [](const Game& a, const Game& b) {
        return a.title == b.title ? a.path < b.path : a.title < b.title;
    });
    return result;
}
}
