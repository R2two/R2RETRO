#include "asset_path.h"
#include "file_ops.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <new>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace r2n64 {
namespace {
constexpr int directoryFlags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC;
constexpr size_t pathLimit = 4096, entryLimit = 1024;
class Fd {
public:
    explicit Fd(int value = -1) : value_(value) {}
    ~Fd() { if (value_ >= 0) ::close(value_); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const { return value_; }
    int release() { const int value = value_; value_ = -1; return value; }
    void reset(int value) { if (value_ >= 0) ::close(value_); value_ = value; }
private:
    int value_;
};
struct CloseDirectory { void operator()(DIR* directory) const { ::closedir(directory); } };
std::string failure(const char* stage, const std::string& path, int code) {
    return std::string(stage) + " " + path + ": " + std::strerror(code) +
           " (errno " + std::to_string(code) + ")";
}
bool normalize(const std::string& path, std::string& normalized, std::vector<std::string>& components) {
    if (path.empty() || path.size() > pathLimit || path.front() != '/' ||
        path.find('\0') != std::string::npos || path.find('\\') != std::string::npos) return false;
    normalized = "/";
    components.clear();
    for (size_t begin = 1; begin < path.size();) {
        const auto slash = path.find('/', begin);
        const auto end = slash == std::string::npos ? path.size() : slash;
        const auto component = path.substr(begin, end - begin);
        begin = end + 1;
        if (component.empty()) continue;
        if (component == "." || component == ".." || component.size() > 255 || components.size() >= 128)
            return false;
        if (normalized.size() > 1) normalized += '/';
        normalized += component;
        components.push_back(component);
    }
    return true;
}
std::string append(const std::string& parent, const std::string& name) {
    return parent + (parent == "/" ? "" : "/") + name;
}
int openChild(int parent, const std::string& parentPath, const std::string& name, std::string& error) {
    const auto path = append(parentPath, name);
    if (path.size() > pathLimit) { error = failure("open directorio", path, ENAMETOOLONG); return -1; }
    const int child = fileops::openAt(parent, parentPath, name.c_str(), directoryFlags);
    if (child < 0) { const int code = errno; error = failure("open directorio", path, code); }
    return child;
}
int openAbsolute(const std::vector<std::string>& components, std::string& error) {
    Fd current(::open("/", directoryFlags));
    if (current.get() < 0) { const int code = errno; error = failure("open directorio", "/", code); return -1; }
    std::string path = "/";
    for (const auto& component : components) {
        const int next = openChild(current.get(), path, component, error);
        if (next < 0) return -1;
        current.reset(next);
        path = append(path, component);
    }
    return current.release();
}
bool identity(int fd, const std::string& path, struct stat& result, std::string& error) {
    if (::fstat(fd, &result) < 0) {
        const int code = errno; error = failure("fstat assets", path, code); return false;
    }
    if (!S_ISDIR(result.st_mode)) { error = failure("tipo de assets", path, ENOTDIR); return false; }
    if (!result.st_ino) { error = "Identidad de assets no disponible: " + path; return false; }
    return true;
}
bool sameIdentity(int fd, const std::string& path, const struct stat& expected, std::string& error) {
    struct stat found{};
    if (!identity(fd, path, found, error)) return false;
    if (expected.st_dev != found.st_dev || expected.st_ino != found.st_ino) {
        error = "Identidad de assets distinta: " + path;
        return false;
    }
    return true;
}
bool validTitle(const std::string& title) {
    if (title.size() != 9) return false;
    for (const char c : title) if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return false;
    return true;
}
bool titleSlot(const std::string& name, const std::string& title) {
    if (name.size() <= title.size() + 1 || name.compare(0, title.size(), title) || name[title.size()] != '_') return false;
    for (size_t i = title.size() + 1; i < name.size(); ++i) if (name[i] < '0' || name[i] > '9') return false;
    return true;
}
}

std::string resolveAssetPath(int assetsFd, const std::string& original,
                             const std::string& sandboxRoot, const std::string& titleId,
                             std::string& error) {
    error.clear();
    std::string normalizedOriginal;
    std::vector<std::string> components;
    if (!normalize(original, normalizedOriginal, components)) { error = "Ruta original de assets no valida"; return {}; }
    struct stat expected{};
    if (!identity(assetsFd, "descriptor conservado", expected, error)) return {};
    std::string originalError;
    {
        Fd candidate(openAbsolute(components, originalError));
        if (candidate.get() >= 0 && sameIdentity(candidate.get(), normalizedOriginal, expected, originalError))
            return normalizedOriginal;
    }
    if (!validTitle(titleId)) { error = "Titulo de sandbox no valido; " + originalError; return {}; }
    std::string normalizedRoot;
    if (!normalize(sandboxRoot, normalizedRoot, components)) { error = "Ruta de sandbox no valida"; return {}; }
    Fd root(openAbsolute(components, error));
    if (root.get() < 0) { error += "; original: " + originalError; return {}; }
    DIR* stream = ::fdopendir(root.get());
    if (!stream) { const int code = errno; error = failure("fdopendir sandbox", normalizedRoot, code); return {}; }
    root.release();
    std::unique_ptr<DIR, CloseDirectory> directory(stream);
    std::string candidateError;
    size_t entries = 0;
    while (true) {
        errno = 0;
        auto* entry = ::readdir(directory.get());
        if (!entry) {
            const int code = errno;
            if (code) { error = failure("readdir sandbox", normalizedRoot, code); return {}; }
            break;
        }
        if (++entries > entryLimit) { error = "El sandbox supera el limite de 1024 entradas"; return {}; }
        const std::string name(entry->d_name);
        if (!titleSlot(name, titleId)) continue;
        const auto slotPath = append(normalizedRoot, name);
        Fd slot(openChild(dirfd(directory.get()), normalizedRoot, name, candidateError));
        if (slot.get() < 0) continue;
        Fd app(openChild(slot.get(), slotPath, "app0", candidateError));
        if (app.get() < 0) continue;
        const auto appPath = append(slotPath, "app0");
        Fd assets(openChild(app.get(), appPath, "assets", candidateError));
        if (assets.get() < 0) continue;
        const auto assetPath = append(appPath, "assets");
        if (sameIdentity(assets.get(), assetPath, expected, candidateError)) return assetPath;
    }
    error = "No se encontro el directorio de assets conservado; original: " + originalError;
    if (!candidateError.empty()) error += "; candidato: " + candidateError;
    return {};
}

bool readAssetBytes(const std::string& assetRoot, const std::string& relativePath,
                    size_t maxBytes, std::vector<uint8_t>& bytes, std::string& error) {
    bytes.clear();
    error.clear();
    std::string normalizedRoot, path;
    std::vector<std::string> components;
    if (!maxBytes || !normalize(assetRoot, normalizedRoot, components) || relativePath.empty() ||
        relativePath.front() == '/' || relativePath.back() == '/' ||
        !normalize(append(normalizedRoot, relativePath), path, components)) {
        error = "Ruta o limite de lectura de assets no valido";
        return false;
    }
    const auto filename = components.back();
    components.pop_back();
    Fd parent(openAbsolute(components, error));
    if (parent.get() < 0) return false;
    const auto slash = path.rfind('/');
    const auto parentPath = slash == 0 ? std::string("/") : path.substr(0, slash);
    Fd file(fileops::openAt(parent.get(), parentPath, filename.c_str(),
                          O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
    if (file.get() < 0) { const int code = errno; error = failure("open asset", path, code); return false; }
    struct stat info{};
    if (::fstat(file.get(), &info) < 0) {
        const int code = errno; error = failure("fstat asset", path, code); return false;
    }
    if (!S_ISREG(info.st_mode)) { error = failure("tipo de asset", path, EINVAL); return false; }
    if (info.st_size <= 0) { error = failure("asset vacio", path, EINVAL); return false; }
    if (static_cast<uintmax_t>(info.st_size) > maxBytes) {
        error = failure("limite de asset", path, EFBIG); return false;
    }
    std::vector<uint8_t> loaded;
    try { loaded.resize(static_cast<size_t>(info.st_size)); }
    catch (const std::bad_alloc&) { error = failure("memoria de asset", path, ENOMEM); return false; }
    size_t offset = 0;
    while (offset < loaded.size()) {
        const auto count = ::read(file.get(), loaded.data() + offset,
                                  std::min<size_t>(loaded.size() - offset, 64 * 1024));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            const int code = count == 0 ? EIO : errno;
            error = failure("read asset", path, code); return false;
        }
        offset += static_cast<size_t>(count);
    }
    // Reject a file that grew during the read instead of retaining a prefix.
    uint8_t extra;
    ssize_t count;
    do { count = ::read(file.get(), &extra, 1); } while (count < 0 && errno == EINTR);
    if (count != 0) {
        const int code = count < 0 ? errno : EFBIG;
        error = failure("fin de asset", path, code); return false;
    }
    bytes.swap(loaded);
    return true;
}
}
