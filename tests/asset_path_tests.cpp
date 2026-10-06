#include "asset_path.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using r2n64::resolveAssetPath;
using r2n64::readAssetBytes;
namespace {
constexpr const char* title = "TEST12345";
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void marker(const fs::path& directory) {
    fs::create_directories(directory / "certs");
    std::ofstream output(directory / "certs/cacert.pem", std::ios::binary);
    output << "SYNTHETIC CERTIFICATE PLACEHOLDER\n";
    require(output.good(), "Cannot write original marker");
}
struct Fixture {
    fs::path root, original, sandbox;
    int fd = -1;
    Fixture() {
        char temporary[] = "/tmp/r2retro-assets-XXXXXX";
        const auto* created = mkdtemp(temporary);
        require(created != nullptr, "Cannot create fixture");
        root = created; original = root / "before/assets"; sandbox = root / "sandboxes";
        marker(original);
        fs::create_directories(sandbox);
        fd = ::open(original.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        require(fd >= 0, "Cannot keep assets descriptor");
    }
    ~Fixture() { if (fd >= 0) ::close(fd); fs::remove_all(root); }
    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;
    void moveTo(const fs::path& destination) {
        fs::create_directories(destination.parent_path());
        fs::rename(original, destination);
    }
    void retained() const {
        struct stat info{};
        require(::fstat(fd, &info) == 0 && S_ISDIR(info.st_mode), "Resolver closed the borrowed descriptor");
    }
    std::string resolve(std::string& error) const {
        const auto result = resolveAssetPath(fd, original.string(), sandbox.string(), title, error);
        retained();
        return result;
    }
};

void writeBytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(output.good(), "Cannot write binary asset fixture");
}

void assetByteTests() {
    Fixture f;
    const std::string relative = "marcos con espacio/imagen original.png";
    const auto source = f.original / relative;
    std::vector<uint8_t> original(70013);
    for (size_t i = 0; i < original.size(); ++i) original[i] = static_cast<uint8_t>((i * 37) ^ (i >> 8));
    writeBytes(source, original);
    std::vector<uint8_t> cached = {0xff};
    std::string error = "stale error";
    require(readAssetBytes(f.original.string(), relative, original.size(), cached, error) &&
            cached == original && error.empty(), "Complete binary asset at exact size limit was not retained");
    const auto relocated = f.sandbox / "TEST12345_023/app0/assets";
    f.moveTo(relocated);
    std::vector<uint8_t> late = {0xff};
    require(!readAssetBytes(f.original.string(), relative, original.size(), late, error) &&
            late.empty() && !error.empty() && cached == original,
            "Cached bytes did not survive removal of the original pathname");
    // The same pathname now refers to unrelated content. Existing copies must
    // remain the original package image, not depend on a future path lookup.
    const std::vector<uint8_t> replacement = {0xff, 0x00, 0x12, 0x34};
    writeBytes(source, replacement);
    require(readAssetBytes(f.original.string(), relative, original.size(), late, error) &&
            late == replacement && cached == original && error.empty(),
            "Changing the root content modified previously retained asset bytes");
    fs::remove(relocated / relative);
    require(cached == original, "Removing the original file changed its retained bytes");
    require(readAssetBytes(f.original.string() + "//", "marcos con espacio//imagen original.png",
                           replacement.size(), late, error) && late == replacement && error.empty(),
            "Safe normalized paths with spaces could not be read");

    auto rejected = [&](const std::string& root, const std::string& name, size_t limit) {
        late.assign({1, 2, 3}); error = "stale error";
        require(!readAssetBytes(root, name, limit, late, error) && late.empty() &&
                !error.empty() && error != "stale error", "Unsafe/invalid asset did not fail with an empty result: " + name);
    };
    rejected(f.original.string(), relative, replacement.size() - 1);
    require(error.find("limite") != std::string::npos, "Oversize asset lost its limit diagnostic");
    rejected(f.original.string(), relative, 0);
    for (const auto& name : std::vector<std::string>{"", "/etc/passwd", ".", "..", "../outside",
            "marcos con espacio/../imagen original.png", "marcos con espacio/./imagen original.png",
            relative + "/", "marcos con espacio\\imagen original.png", relative + std::string("\0suffix", 7),
            std::string(256, 'x'), std::string(4097, 'y')})
        rejected(f.original.string(), name, 1024 * 1024);
    for (const auto& root : std::vector<std::string>{"", "relative/assets", f.original.string() + "/..",
            f.original.string() + std::string("\0suffix", 7)})
        rejected(root, relative, 1024 * 1024);
    writeBytes(f.original / "empty.png", {});
    rejected(f.original.string(), "empty.png", 1024);
    require(error.find("vacio") != std::string::npos, "Empty asset lost its type diagnostic");
    rejected(f.original.string(), "missing.png", 1024);
    rejected(f.original.string(), "marcos con espacio", 1024);
    const auto fifo = f.original / "fifo.png";
    require(::mkfifo(fifo.c_str(), 0600) == 0, "Cannot create FIFO fixture");
    rejected(f.original.string(), "fifo.png", 1024); // Must not block without a writer.
    fs::create_symlink(source, f.original / "linked.png");
    rejected(f.original.string(), "linked.png", 1024);
    fs::create_directory_symlink(source.parent_path(), f.original / "linked-directory");
    rejected(f.original.string(), "linked-directory/imagen original.png", 1024);
    fs::create_directory_symlink(f.original, f.root / "linked-assets");
    rejected((f.root / "linked-assets").string(), relative, 1024);
    // Errors cannot poison subsequent requests or retain stale output bytes.
    require(readAssetBytes(f.original.string(), relative, replacement.size(), late, error) &&
            late == replacement && error.empty(), "Valid asset failed after rejected file types/paths");
}
}

int main() {
    try {
        std::string error;
        {
            Fixture f;
            error = "stale error";
            require(f.resolve(error) == f.original.string() && error.empty(), "Original assets identity was not retained");
            require(resolveAssetPath(f.fd, f.original.string(), "unused relative path", "", error) == f.original.string() &&
                    error.empty(), "Valid original path incorrectly requires a sandbox/title");
            f.retained();
        }
        {
            Fixture f;
            const auto candidate = f.sandbox / "TEST12345_001/app0/assets";
            f.moveTo(candidate);
            // Same CA filename/content at the old path must not defeat identity.
            marker(f.original);
            require(f.resolve(error) == candidate.string() && error.empty(), "Moved original assets not found in slot 001");
            require(fs::is_regular_file(fs::path(f.resolve(error)) / "certs/cacert.pem"), "Resolved directory lost its marker");
        }
        {
            Fixture f;
            const auto candidate = f.sandbox / "TEST12345_19/app0/assets";
            f.moveTo(candidate);
            marker(f.sandbox / "TEST12345_000/app0/assets");
            marker(f.sandbox / "TEST12345_999/app0/assets");
            require(f.resolve(error) == candidate.string() && error.empty(), "Resolver selected wrong identity or hardcoded slot 000");
        }
        {
            Fixture f;
            f.moveTo(f.root / "unrelated/assets");
            marker(f.sandbox / "TEST12345_001/app0/assets");
            require(f.resolve(error).empty() && error.find("Identidad de assets distinta") != std::string::npos,
                    "A same-named certificate in a different directory was accepted");
        }
        for (const auto* wrongSlot : {"TEST123456_001", "TEST12345_", "TEST12345_1x", "OTHER1234_001"}) {
            Fixture f;
            f.moveTo(f.sandbox / wrongSlot / "app0/assets");
            require(f.resolve(error).empty() && !error.empty(), "Wrong title or nonnumeric slot was selected");
        }
        for (const auto& invalidTitle : std::vector<std::string>{"", "test12345", "TEST1234", "TEST123456", "../TEST12",
                                                                std::string("TEST\0" "2345", 9)}) {
            Fixture f;
            f.moveTo(f.sandbox / "TEST12345_001/app0/assets");
            require(resolveAssetPath(f.fd, f.original.string(), f.sandbox.string(), invalidTitle, error).empty() &&
                    error.find("Titulo de sandbox no valido") != std::string::npos, "Unsafe fallback title was accepted");
            f.retained();
        }
        {
            Fixture f;
            for (const auto& badPath : std::vector<std::string>{"", "relative/assets", f.original.string() + "/..",
                     f.root.string() + "/before/./assets", f.root.string() + "/before\\assets",
                     f.original.string() + std::string("\0suffix", 7), "/" + std::string(4096, 'a')}) {
                require(resolveAssetPath(f.fd, badPath, f.sandbox.string(), title, error).empty() &&
                        error.find("Ruta original") != std::string::npos, "Unsafe original path accepted");
                f.retained();
            }
            f.moveTo(f.sandbox / "TEST12345_001/app0/assets");
            require(resolveAssetPath(f.fd, f.original.string(), "relative", title, error).empty() &&
                    error.find("Ruta de sandbox") != std::string::npos, "Relative fallback root accepted");
        }
        {
            Fixture f;
            require(resolveAssetPath(-1, f.original.string(), f.sandbox.string(), title, error).empty() &&
                    error.find("fstat assets") != std::string::npos && error.find("errno " + std::to_string(EBADF)) != std::string::npos,
                    "Invalid borrowed descriptor lost its fstat error");
            const int regular = ::open((f.original / "certs/cacert.pem").c_str(), O_RDONLY);
            require(regular >= 0, "Cannot open regular fixture");
            const auto result = resolveAssetPath(regular, f.original.string(), f.sandbox.string(), title, error);
            require(result.empty() && error.find("tipo de assets") != std::string::npos && ::fcntl(regular, F_GETFD) >= 0,
                    "Non-directory descriptor accepted or closed");
            ::close(regular);
            f.retained();
        }
        for (unsigned level = 0; level < 4; ++level) {
            Fixture f;
            const auto real = f.root / "real/app0/assets";
            f.moveTo(real);
            fs::path root = f.sandbox;
            if (level == 0) fs::create_directory_symlink(f.root / "real", f.sandbox / "TEST12345_001");
            else if (level == 1) {
                fs::create_directories(f.sandbox / "TEST12345_001");
                fs::create_directory_symlink(f.root / "real/app0", f.sandbox / "TEST12345_001/app0");
            } else if (level == 2) {
                fs::create_directories(f.sandbox / "TEST12345_001/app0");
                fs::create_directory_symlink(real, f.sandbox / "TEST12345_001/app0/assets");
            } else {
                fs::rename(f.root / "real", f.sandbox / "TEST12345_001");
                root = f.root / "sandbox-link";
                fs::create_directory_symlink(f.sandbox, root);
            }
            require(resolveAssetPath(f.fd, f.original.string(), root.string(), title, error).empty() && !error.empty(),
                    "Resolver followed a symlink in the fallback directory chain");
            f.retained();
        }
        {
            Fixture f;
            f.moveTo(f.root / "real/assets");
            fs::create_directory_symlink(f.root / "real", f.root / "linked-before");
            require(resolveAssetPath(f.fd, (f.root / "linked-before/assets").string(), f.sandbox.string(), title, error).empty() &&
                    !error.empty(), "Resolver followed a symlink in the original directory chain");
            f.retained();
        }
        {
            Fixture f;
            f.moveTo(f.root / "unrelated/assets");
            for (unsigned i = 0; i < 1025; ++i) fs::create_directory(f.sandbox / ("unrelated-" + std::to_string(i)));
            require(f.resolve(error).empty() && error.find("1024") != std::string::npos, "Sandbox entry limit was not enforced");
        }
        {
            Fixture f;
            f.moveTo(f.root / "unrelated/assets");
            fs::remove(f.sandbox);
            require(f.resolve(error).empty() && error.find("open directorio") != std::string::npos &&
                    error.find("errno " + std::to_string(ENOENT)) != std::string::npos,
                    "Missing fallback directory lost its opening error");
        }
        assetByteTests();
        std::puts("PASS: asset identity and bounded search; bounded complete byte preload, moved/replaced/deleted source, unsafe paths, symlinks, empty/large files and nonblocking FIFO rejection");
        return 0;
    } catch (const std::exception& exception) {
        std::fprintf(stderr, "FAIL: %s\n", exception.what());
        return 1;
    }
}
