#include "rom.h"
#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

// These wrappers exercise the real scanner and desktop fileops implementation.
// Only one operation on one fixture directory fails; other roots remain real.
enum class Fault { None, OpenDirectory, Stat, DirectoryType, Fdopendir,
                   ReadBefore, ReadAfter, FileBeforeEof };
static Fault fault = Fault::None;
static struct stat parentIdentity{}, directoryIdentity{};
static unsigned triggers = 0, directoryCloses = 0, directoryStreamCloses = 0;
static bool readGoodEntry = false;
extern "C" int __real_openat(int, const char*, int, ...);
extern "C" int __real_fstat(int, struct stat*);
extern "C" DIR* __real_fdopendir(int);
extern "C" struct dirent* __real_readdir(DIR*);
extern "C" int __real_close(int);
extern "C" int __real_closedir(DIR*);

static bool matches(int fd, const struct stat& expected) {
    const int saved = errno;
    struct stat info{};
    const bool same = __real_fstat(fd, &info) == 0 && S_ISDIR(info.st_mode) &&
                      info.st_dev == expected.st_dev && info.st_ino == expected.st_ino;
    errno = saved;
    return same;
}
extern "C" int __wrap_openat(int fd, const char* name, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list arguments;
        va_start(arguments, flags);
        mode = static_cast<mode_t>(va_arg(arguments, int));
        va_end(arguments);
    }
    if (fault == Fault::OpenDirectory && matches(fd, parentIdentity) &&
        (flags & O_DIRECTORY) && std::strcmp(name, "gb") == 0) {
        ++triggers; errno = EINVAL; return -1;
    }
    if (fault == Fault::FileBeforeEof && matches(fd, directoryIdentity) &&
        !(flags & O_DIRECTORY) && std::strcmp(name, "second.gb") == 0) {
        ++triggers; errno = EINVAL; return -1;
    }
    return __real_openat(fd, name, flags, mode);
}
extern "C" int __wrap_fstat(int fd, struct stat* info) {
    const int result = __real_fstat(fd, info);
    if (result != 0 || !matches(fd, directoryIdentity)) return result;
    if (fault == Fault::Stat) { ++triggers; errno = EIO; return -1; }
    if (fault == Fault::DirectoryType) {
        ++triggers;
        info->st_mode = (info->st_mode & ~S_IFMT) | S_IFREG;
        errno = EINVAL; // A successful call's stale errno must not be reported.
    }
    return result;
}
extern "C" DIR* __wrap_fdopendir(int fd) {
    if (fault == Fault::Fdopendir && matches(fd, directoryIdentity)) {
        ++triggers; errno = EMFILE; return nullptr;
    }
    return __real_fdopendir(fd);
}
extern "C" struct dirent* __wrap_readdir(DIR* directory) {
    const bool target = matches(dirfd(directory), directoryIdentity);
    if (target && (fault == Fault::ReadBefore || (fault == Fault::ReadAfter && readGoodEntry))) {
        ++triggers; errno = EIO; return nullptr;
    }
    auto* entry = __real_readdir(directory);
    if (target && entry && std::strcmp(entry->d_name, "good.gb") == 0) readGoodEntry = true;
    return entry;
}
extern "C" int __wrap_close(int fd) {
    const bool target = matches(fd, directoryIdentity);
    if (target) ++directoryCloses;
    const int result = __real_close(fd);
    // POSIX does not promise a successful close preserves errno. Confirm that
    // scanner diagnostics retain the earlier failing operation's error code.
    if (target && (fault == Fault::Fdopendir || fault == Fault::Stat)) errno = EINVAL;
    return result;
}
extern "C" int __wrap_closedir(DIR* directory) {
    if (matches(dirfd(directory), directoryIdentity)) ++directoryStreamCloses;
    return __real_closedir(directory);
}

using namespace r2n64;
namespace fs = std::filesystem;
static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
static void fixture(const fs::path& path, bool color = false) {
    std::array<uint8_t, 0x150> bytes{};
    std::memcpy(bytes.data() + 0x134, "ORIGINAL TEST", 13);
    bytes[0x143] = color ? 0x80 : 0;
    bytes[0x14a] = 1;
    for (size_t i = 0x134; i <= 0x14c; ++i)
        bytes[0x14d] = static_cast<uint8_t>(bytes[0x14d] - bytes[i] - 1);
    // A synthetic header and zero padding, no game program or borrowed logo.
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    output.seekp(32767); output.put('\0');
    require(output.good(), "Cannot write original fixture");
}
static bool contains(const ScanResult& result, const fs::path& path) {
    return std::any_of(result.games.begin(), result.games.end(), [&](const Game& game) {
        return game.path == path.string();
    });
}
static void expectWarning(const ScanResult& result, const fs::path& path, const char* operation, int code) {
    require(result.warnings.size() == 1, "Expected exactly one injected failure warning");
    const auto& warning = result.warnings.front();
    require(warning.find(path.string() + " [" + operation + "]:") == 0,
            "Warning lost the path or failing operation: " + warning);
    require(warning.find("(errno " + std::to_string(code) + ")") != std::string::npos &&
            warning.find(std::strerror(code)) != std::string::npos,
            "Warning lost the original error code: " + warning);
}

int main() {
    char temporary[] = "/tmp/r2retro-scan-failures-XXXXXX";
    const auto* created = mkdtemp(temporary);
    if (!created) return 1;
    const fs::path root(created), first = root / "first", second = root / "second";
    try {
        fs::create_directories(first / "gb");
        fs::create_directories(first / "gbc");
        fs::create_directories(second);
        fixture(first / "gb/good.gb");
        fixture(first / "gb/second.gb");
        fixture(first / "gbc/color.gbc", true);
        fixture(second / "root.gb");
        require(::stat(first.c_str(), &parentIdentity) == 0 &&
                ::stat((first / "gb").c_str(), &directoryIdentity) == 0, "Cannot identify test directories");
        const std::atomic<bool> cancel{false};
        const auto run = [&](Fault injected) {
            fault = injected;
            triggers = directoryCloses = directoryStreamCloses = 0;
            readGoodEntry = false;
            errno = 0;
            auto result = scanRoms({first.string(), second.string()}, cancel);
            fault = Fault::None;
            require(contains(result, first / "gbc/color.gbc") && contains(result, second / "root.gb"),
                    "A failed folder prevented scanning another folder or root");
            return result;
        };
        auto result = run(Fault::None);
        require(result.games.size() == 4 && result.warnings.empty() && directoryStreamCloses == 1,
                "Baseline scan failed");

        result = run(Fault::OpenDirectory);
        expectWarning(result, first / "gb", "open sistema", EINVAL);
        require(result.games.size() == 2 && triggers == 1 && directoryCloses == 0 && directoryStreamCloses == 0,
                "Failed openat unexpectedly read or closed a directory");

        result = run(Fault::Fdopendir);
        expectWarning(result, first / "gb", "fdopendir", EMFILE);
        require(result.games.size() == 2 && triggers == 1 && directoryCloses == 1 && directoryStreamCloses == 0,
                "Failed fdopendir leaked or double-closed its descriptor");

        result = run(Fault::Stat);
        expectWarning(result, first / "gb", "fstat directorio", EIO);
        require(result.games.size() == 2 && triggers == 1 && directoryCloses == 1 && directoryStreamCloses == 0,
                "Failed fstat was not handled before listing");

        result = run(Fault::DirectoryType);
        expectWarning(result, first / "gb", "fstat tipo de directorio", ENOTDIR);
        require(result.games.size() == 2 && triggers == 1 && directoryCloses == 1 && directoryStreamCloses == 0,
                "A non-directory was listed or not closed");

        result = run(Fault::ReadBefore);
        expectWarning(result, first / "gb", "readdir", EIO);
        require(result.games.size() == 2 && triggers == 1 && directoryStreamCloses == 1,
                "Failed readdir did not close its directory stream");

        result = run(Fault::ReadAfter);
        expectWarning(result, first / "gb", "readdir", EIO);
        require(contains(result, first / "gb/good.gb") && triggers == 1 && directoryStreamCloses == 1,
                "Late readdir failure discarded an already-read ROM or leaked its stream");

        result = run(Fault::FileBeforeEof);
        expectWarning(result, first / "gb/second.gb", "open ROM", EINVAL);
        require(result.games.size() == 3 && contains(result, first / "gb/good.gb") && triggers == 1 &&
                directoryStreamCloses == 1, "File errno contaminated a later normal directory EOF");

        // A new scan must not retain failed descriptors, fault state or results.
        result = run(Fault::None);
        require(result.games.size() == 4 && result.warnings.empty(), "Scan after injected errors failed");
        fs::remove_all(root);
        std::puts("PASS: scanner injected openat/fdopendir/fstat/readdir failures, errno preservation, descriptor ownership, EOF and other-root continuity");
        return 0;
    } catch (const std::exception& error) {
        fault = Fault::None;
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        fs::remove_all(root);
        return 1;
    }
}
