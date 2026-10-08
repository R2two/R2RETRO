#include "handheld_settings.h"
#include "file_ops.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace r2n64 {
namespace {
constexpr size_t maximumBytes = 4096;
struct File {
    int fd = -1;
    explicit File(int value = -1) : fd(value) {}
    ~File() { if (fd >= 0) ::close(fd); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
};

bool failure(std::string& error, const std::string& description) {
    error = description;
    return false;
}
bool systemValid(SystemType system) {
    return system == SystemType::GameBoy || system == SystemType::GameBoyColor ||
           system == SystemType::GameBoyAdvance || system == SystemType::NintendoEntertainmentSystem ||
           system == SystemType::SuperNintendo;
}
HandheldSettings defaultsFor(SystemType system) {
    HandheldSettings result;
    if (system == SystemType::NintendoEntertainmentSystem || system == SystemType::SuperNintendo) {
        result.integerScaling = false; // CRT-era consoles use 4:3 by default.
        // NES and SNES ship a console-frame overlay enabled for new preferences;
        // an explicit overlay=false in an existing file is always preserved.
        result.overlay = true;
    }
    return result;
}
bool valuesValid(const HandheldSettings& value) {
    return (value.fastForward == 2 || value.fastForward == 4 || value.fastForward == 8) &&
           value.stateSlot <= 4 && value.gbPalette <= 3 && value.shader <= 2 && value.gbaFrameskip <= 2;
}

// Native desktop walks use descriptors; PS4 uses verified absolute paths through
// fileops (whose header documents the remaining path-operation race window).
// Missing is a benign load result, but mkdir/open failures during save are errors.
int settingsDirectory(const std::string& root, bool create, bool& missing,
                      std::string& openedPath, std::string& error) {
    missing = false;
    if (root.empty() || root.find('\0') != std::string::npos) {
        failure(error, "Ruta de ajustes vacia o invalida");
        return -1;
    }
    // Validate the full root before a missing prefix can return defaults or be
    // created; a later '..' component must never partially create directories.
    size_t check = 0;
    while (check < root.size()) {
        const auto slash = root.find('/', check);
        const auto end = slash == std::string::npos ? root.size() : slash;
        if (root.compare(check, end - check, "..") == 0) {
            failure(error, "La ruta de ajustes no admite '..'");
            return -1;
        }
        check = end + 1;
    }
    File directory(::open(root.front() == '/' ? "/" : ".",
                          O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (directory.fd < 0) {
        failure(error, "No se pudo abrir el directorio de ajustes");
        return -1;
    }
    std::string currentPath = root.front() == '/' ? "/" : ".";
    const std::string path = root + "/configs/systems";
    size_t position = 0;
    while (position < path.size()) {
        const auto slash = path.find('/', position);
        const auto end = slash == std::string::npos ? path.size() : slash;
        const std::string component = path.substr(position, end - position);
        position = end + 1;
        if (component.empty() || component == ".") continue;
        int next = fileops::openAt(directory.fd, currentPath, component.c_str(),
                            O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        if (next < 0 && errno == ENOENT && create) {
            if (fileops::mkdirAt(directory.fd, currentPath, component.c_str(), 0700) < 0 && errno != EEXIST) {
                failure(error, "No se pudo crear el directorio de ajustes: " + std::string(std::strerror(errno)));
                return -1;
            }
            next = fileops::openAt(directory.fd, currentPath, component.c_str(),
                            O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        }
        if (next < 0) {
            if (!create && errno == ENOENT) missing = true;
            else failure(error, "Los ajustes requieren directorios reales, sin enlaces: " +
                                std::string(std::strerror(errno)));
            return -1;
        }
        ::close(directory.fd);
        directory.fd = next;
        if (currentPath != "/") currentPath += '/';
        currentPath += component;
    }
    openedPath = std::move(currentPath);
    const int result = directory.fd;
    directory.fd = -1;
    return result;
}

bool regularFile(int fd) {
    struct stat info{};
    return ::fstat(fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_nlink == 1;
}
bool targetSafe(int directory, const std::string& parentPath, const std::string& name, std::string& error) {
    File file(fileops::openAt(directory, parentPath, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
    if (file.fd < 0) {
        if (errno == ENOENT) return true;
        return failure(error, "No se pudo comprobar el archivo de ajustes; los enlaces no se admiten");
    }
    return regularFile(file.fd) || failure(error, "Los ajustes deben ser un archivo regular sin enlaces");
}

struct Scalar {
    enum class Kind { Other, Integer, Boolean } kind = Kind::Other;
    unsigned integer = 0;
    bool boolean = false;
};

class Json {
public:
    explicit Json(const std::string& source) : text(source) {}
    bool parse(HandheldSettings& result) {
        if (!take('{')) return false;
        unsigned fields = 0;
        std::set<std::string> keys;
        if (take('}')) return false;
        for (;;) {
            std::string key;
            Scalar value;
            if (!string(key) || !keys.insert(key).second || !take(':') || !scalar(value)) return false;
            if (key == "version") {
                if (value.kind != Scalar::Kind::Integer || value.integer != 1) return false;
                fields |= 1;
            } else if (key == "fastForward") {
                if (value.kind != Scalar::Kind::Integer) return false;
                result.fastForward = value.integer;
                fields |= 2;
            } else if (key == "stateSlot") {
                if (value.kind != Scalar::Kind::Integer) return false;
                result.stateSlot = value.integer;
                fields |= 4;
            } else if (key == "integerScaling") {
                if (value.kind != Scalar::Kind::Boolean) return false;
                result.integerScaling = value.boolean;
                fields |= 8;
            } else if (key == "linearFilter") {
                if (value.kind != Scalar::Kind::Boolean) return false;
                result.linearFilter = value.boolean;
                fields |= 16;
            } else if (key == "gbPalette") {
                if (value.kind != Scalar::Kind::Integer) return false;
                result.gbPalette = value.integer;
                fields |= 32;
            } else if (key == "overlay") {
                if (value.kind != Scalar::Kind::Boolean) return false;
                result.overlay = value.boolean;
            } else if (key == "showStats") {
                if (value.kind != Scalar::Kind::Boolean) return false;
                result.showStats = value.boolean;
            } else if (key == "shader") {
                if (value.kind != Scalar::Kind::Integer) return false;
                result.shader = value.integer;
            } else if (key == "gbaFrameskip") {
                if (value.kind != Scalar::Kind::Integer) return false;
                result.gbaFrameskip = value.integer;
            } else if (key == "gbColor") {
                if (value.kind != Scalar::Kind::Boolean) return false;
                result.gbColor = value.boolean;
            }
            if (take('}')) break;
            if (!take(',')) return false;
        }
        space();
        return position == text.size() && fields == 63 && valuesValid(result);
    }
private:
    const std::string& text;
    size_t position = 0;
    void space() {
        while (position < text.size() && (text[position] == ' ' || text[position] == '\n' ||
               text[position] == '\r' || text[position] == '\t')) ++position;
    }
    bool take(char wanted) {
        space();
        if (position == text.size() || text[position] != wanted) return false;
        ++position;
        return true;
    }
    bool literal(const char* value) {
        const auto length = std::strlen(value);
        if (text.compare(position, length, value) != 0) return false;
        position += length;
        return true;
    }
    bool hex4(unsigned& result) {
        result = 0;
        for (unsigned i = 0; i < 4; ++i) {
            if (position == text.size()) return false;
            const auto c = text[position++];
            const int digit = c >= '0' && c <= '9' ? c - '0' :
                              c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                              c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (digit < 0) return false;
            result = (result << 4) | static_cast<unsigned>(digit);
        }
        return true;
    }
    static void utf8(std::string& out, unsigned value) {
        if (value < 0x80) out += static_cast<char>(value);
        else if (value < 0x800) {
            out += static_cast<char>(0xc0 | (value >> 6));
            out += static_cast<char>(0x80 | (value & 63));
        } else if (value < 0x10000) {
            out += static_cast<char>(0xe0 | (value >> 12));
            out += static_cast<char>(0x80 | ((value >> 6) & 63));
            out += static_cast<char>(0x80 | (value & 63));
        } else {
            out += static_cast<char>(0xf0 | (value >> 18));
            out += static_cast<char>(0x80 | ((value >> 12) & 63));
            out += static_cast<char>(0x80 | ((value >> 6) & 63));
            out += static_cast<char>(0x80 | (value & 63));
        }
    }
    bool string(std::string& out) {
        if (!take('"')) return false;
        while (position < text.size()) {
            const unsigned char c = static_cast<unsigned char>(text[position++]);
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c == '\\') {
                if (position == text.size()) return false;
                switch (text[position++]) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned point;
                    if (!hex4(point)) return false;
                    if (point >= 0xd800 && point <= 0xdbff) {
                        unsigned low;
                        if (!literal("\\u") || !hex4(low) || low < 0xdc00 || low > 0xdfff) return false;
                        point = 0x10000 + ((point - 0xd800) << 10) + low - 0xdc00;
                    } else if (point >= 0xdc00 && point <= 0xdfff) return false;
                    utf8(out, point);
                    break;
                }
                default: return false;
                }
            } else if (c < 0x80) out += static_cast<char>(c);
            else {
                unsigned extra, point, minimum;
                if (c >= 0xc2 && c <= 0xdf) { extra = 1; point = c & 31; minimum = 0x80; }
                else if (c >= 0xe0 && c <= 0xef) { extra = 2; point = c & 15; minimum = 0x800; }
                else if (c >= 0xf0 && c <= 0xf4) { extra = 3; point = c & 7; minimum = 0x10000; }
                else return false;
                for (unsigned i = 0; i < extra; ++i) {
                    if (position == text.size()) return false;
                    const unsigned char next = static_cast<unsigned char>(text[position++]);
                    if ((next & 0xc0) != 0x80) return false;
                    point = (point << 6) | (next & 63);
                }
                if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) return false;
                utf8(out, point);
            }
        }
        return false;
    }
    bool scalar(Scalar& out) {
        space();
        if (position == text.size()) return false;
        if (literal("true")) { out.kind = Scalar::Kind::Boolean; out.boolean = true; return true; }
        if (literal("false")) { out.kind = Scalar::Kind::Boolean; return true; }
        if (literal("null")) return true;
        if (text[position] == '"') { std::string ignored; return string(ignored); }
        bool integer = true;
        if (text[position] == '-') { integer = false; ++position; }
        if (position == text.size() || text[position] < '0' || text[position] > '9') return false;
        uint64_t number = 0;
        const bool zero = text[position] == '0';
        do {
            if (number <= std::numeric_limits<unsigned>::max()) number = number * 10 + text[position] - '0';
            ++position;
            if (zero) break;
        } while (position < text.size() && text[position] >= '0' && text[position] <= '9');
        if (position < text.size() && text[position] == '.') {
            integer = false;
            ++position;
            const auto start = position;
            while (position < text.size() && text[position] >= '0' && text[position] <= '9') ++position;
            if (position == start) return false;
        }
        if (position < text.size() && (text[position] == 'e' || text[position] == 'E')) {
            integer = false;
            ++position;
            if (position < text.size() && (text[position] == '+' || text[position] == '-')) ++position;
            const auto start = position;
            while (position < text.size() && text[position] >= '0' && text[position] <= '9') ++position;
            if (position == start) return false;
        }
        if (integer && number <= std::numeric_limits<unsigned>::max()) {
            out.kind = Scalar::Kind::Integer;
            out.integer = static_cast<unsigned>(number);
        }
        return true;
    }
};
}

const char* gbPaletteName(unsigned palette) {
    static constexpr const char* names[] = {"Gris", "Verde cl\xc3\xa1sico", "Oliva", "Turquesa"};
    return palette < 4 ? names[palette] : "Desconocida";
}

bool loadHandheldSettings(const std::string& root, SystemType system,
                          HandheldSettings& settings, std::string& error) {
    settings = defaultsFor(system);
    error.clear();
    if (!systemValid(system)) return failure(error, "Los ajustes corresponden a GB, GBC, GBA, NES o SNES");
    bool missing;
    std::string parentPath;
    File directory(settingsDirectory(root, false, missing, parentPath, error));
    if (directory.fd < 0) return missing;
    const std::string name = std::string(systemId(system)) + ".json";
    File file(fileops::openAt(directory.fd, parentPath, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
    if (file.fd < 0) {
        if (errno == ENOENT) return true;
        return failure(error, "No se pudo abrir el archivo de ajustes; los enlaces no se admiten");
    }
    struct stat info{};
    if (!regularFile(file.fd) || ::fstat(file.fd, &info) < 0 || info.st_size <= 0 ||
        static_cast<uint64_t>(info.st_size) > maximumBytes)
        return failure(error, "Archivo de ajustes invalido: requiere un archivo regular de 1 a 4096 bytes");
    std::string source;
    char buffer[maximumBytes + 1];
    for (;;) {
        const auto count = ::read(file.fd, buffer, sizeof(buffer) - source.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) return failure(error, "No se pudo leer el archivo de ajustes");
        if (count == 0) break;
        source.append(buffer, static_cast<size_t>(count));
        if (source.size() > maximumBytes) return failure(error, "El archivo de ajustes supera 4096 bytes");
    }
    HandheldSettings parsed = defaultsFor(system);
    if (!Json(source).parse(parsed))
        return failure(error, "JSON de ajustes invalido: se requiere version 1 y valores dentro de rango");
    settings = parsed;
    return true;
}

bool saveHandheldSettings(const std::string& root, SystemType system,
                          const HandheldSettings& settings, std::string& error) {
    error.clear();
    if (!systemValid(system)) return failure(error, "Los ajustes corresponden a GB, GBC, GBA, NES o SNES");
    if (!valuesValid(settings)) return failure(error, "Valores de ajustes fuera de rango");
    bool missing;
    std::string parentPath;
    File directory(settingsDirectory(root, true, missing, parentPath, error));
    if (directory.fd < 0) return false;
    const std::string name = std::string(systemId(system)) + ".json";
    if (!targetSafe(directory.fd, parentPath, name, error)) return false;
    static std::atomic<unsigned> sequence{0};
    std::string temporary;
    File file;
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        temporary = "." + name + ".tmp." + std::to_string(::getpid()) + "." + std::to_string(sequence++);
        file.fd = fileops::openAt(directory.fd, parentPath, temporary.c_str(),
                           O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (file.fd >= 0 || errno != EEXIST) break;
    }
    if (file.fd < 0) return failure(error, "No se pudo crear el archivo temporal de ajustes");
    const std::string source = "{\n  \"version\": 1,\n  \"fastForward\": " + std::to_string(settings.fastForward) +
        ",\n  \"stateSlot\": " + std::to_string(settings.stateSlot) +
        ",\n  \"integerScaling\": " + (settings.integerScaling ? "true" : "false") +
        ",\n  \"linearFilter\": " + (settings.linearFilter ? "true" : "false") +
        ",\n  \"gbPalette\": " + std::to_string(settings.gbPalette) +
        ",\n  \"overlay\": " + (settings.overlay ? "true" : "false") +
        ",\n  \"showStats\": " + (settings.showStats ? "true" : "false") +
        ",\n  \"shader\": " + std::to_string(settings.shader) +
        ",\n  \"gbaFrameskip\": " + std::to_string(settings.gbaFrameskip) +
        ",\n  \"gbColor\": " + (settings.gbColor ? "true" : "false") + "\n}\n";
    size_t written = 0;
    bool ok = true;
    while (written < source.size()) {
        const auto count = ::write(file.fd, source.data() + written, source.size() - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        written += static_cast<size_t>(count);
    }
    if (ok) ok = ::fsync(file.fd) == 0;
    if (::close(file.fd) < 0) ok = false;
    file.fd = -1;
    if (ok) ok = targetSafe(directory.fd, parentPath, name, error);
    if (ok) ok = fileops::renameAt(directory.fd, parentPath, temporary.c_str(), directory.fd, parentPath, name.c_str()) == 0;
    if (!ok) {
        fileops::unlinkAt(directory.fd, parentPath, temporary.c_str());
        if (error.empty()) error = "No se pudieron guardar los ajustes; se conserva el archivo anterior";
        return false;
    }
    return true;
}
}
