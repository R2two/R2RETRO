#include "file_ops.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// These counters belong to file_ops_stubs.cpp. The target must wrap all five
// *at calls and compile file_ops.cpp with R2N64_PATH_FILEOPS=1.
extern "C" unsigned r2n64FileOpsAtCalls();
extern "C" void r2n64FileOpsResetAtCalls();
extern "C" void r2n64FileOpsSetAtError(int);

namespace fs = std::filesystem;
namespace ops = r2n64::fileops;
namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message + " (errno=" + std::to_string(errno) + ")");
}
struct Fd {
    int value;
    explicit Fd(int descriptor) : value(descriptor) {}
    ~Fd() { if (value >= 0) ::close(value); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};
void writeFile(int parent, const std::string& path, const char* name, const std::string& contents) {
    Fd file(ops::openAt(parent, path, name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
    require(file.value >= 0, "Cannot create " + path + "/" + name);
    require(::write(file.value, contents.data(), contents.size()) == static_cast<ssize_t>(contents.size()),
            "Cannot write fixture");
    struct stat info{};
    require(::fstat(file.value, &info) == 0 && S_ISREG(info.st_mode) && !(info.st_mode & 0077),
            "Created settings file must be regular and private");
}
std::string readFile(int parent, const std::string& path, const char* name) {
    Fd file(ops::openAt(parent, path, name, O_RDONLY | O_NONBLOCK | O_NOFOLLOW));
    require(file.value >= 0, "Cannot read " + path + "/" + name);
    std::string result;
    char bytes[256];
    for (;;) {
        const auto count = ::read(file.value, bytes, sizeof(bytes));
        if (count < 0 && errno == EINTR) continue;
        require(count >= 0, "Fixture read failed");
        if (!count) return result;
        result.append(bytes, static_cast<size_t>(count));
    }
}
void rejectedOpen(int parent, const std::string& path, const char* name, int flags) {
    Fd file(ops::openAt(parent, path, name, flags, 0600));
    require(file.value < 0, "Unsafe open accepted for " + path + "/" + (name ? name : "<null>"));
}
void exercise(const fs::path& directory, int stubError) {
    fs::create_directories(directory);
    const std::string root = directory.string();
    Fd parent(::open(root.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW));
    require(parent.value >= 0, "Cannot open test root");
    r2n64FileOpsSetAtError(stubError);
    r2n64FileOpsResetAtCalls();
    // Prove the test executable really intercepts each unavailable function.
    struct stat ignored{};
    require(::openat(parent.value, "anything", O_RDONLY) < 0 && errno == stubError, "openat stub inactive");
    require(::mkdirat(parent.value, "anything", 0700) < 0 && errno == stubError, "mkdirat stub inactive");
    require(::renameat(parent.value, "anything", parent.value, "other") < 0 && errno == stubError, "renameat stub inactive");
    require(::unlinkat(parent.value, "anything", 0) < 0 && errno == stubError, "unlinkat stub inactive");
    require(::fstatat(parent.value, "anything", &ignored, 0) < 0 && errno == stubError, "fstatat stub inactive");
    require(r2n64FileOpsAtCalls() == 5, "Unexpected *at instrumentation count");
    r2n64FileOpsResetAtCalls();

    for (const char* name : {"roms", "configs", "cache", "carpetas con espacios"})
        require(ops::mkdirAt(parent.value, root, name, 0700) == 0, "Cannot create application directory");
    require(ops::mkdirAt(parent.value, root, "roms", 0700) < 0 && errno == EEXIST,
            "Existing directory must report EEXIST without changing it");
    Fd roms(ops::openAt(parent.value, root, "roms", O_RDONLY | O_DIRECTORY | O_NOFOLLOW));
    Fd configs(ops::openAt(parent.value, root, "configs", O_RDONLY | O_DIRECTORY | O_NOFOLLOW));
    Fd cache(ops::openAt(parent.value, root, "cache", O_RDONLY | O_DIRECTORY | O_NOFOLLOW));
    require(roms.value >= 0 && configs.value >= 0 && cache.value >= 0, "Cannot open application directories");
    const std::string romPath = root + "/roms", configPath = root + "/configs", cachePath = root + "/cache";
    require(ops::mkdirAt(roms.value, romPath, "gb", 0700) == 0, "Cannot create GB subdirectory");
    require(ops::mkdirAt(configs.value, configPath, "games", 0700) == 0, "Cannot create game preferences directory");
    require(ops::mkdirAt(cache.value, cachePath, "library", 0700) == 0, "Cannot create library cache directory");
    Fd gb(ops::openAt(roms.value, romPath, "gb", O_RDONLY | O_DIRECTORY | O_NONBLOCK));
    Fd games(ops::openAt(configs.value, configPath, "games", O_RDONLY | O_DIRECTORY | O_NONBLOCK));
    Fd library(ops::openAt(cache.value, cachePath, "library", O_RDONLY | O_DIRECTORY | O_NONBLOCK));
    require(gb.value >= 0 && games.value >= 0 && library.value >= 0, "Scanner/settings/cache child open failed");
    writeFile(gb.value, romPath + "/gb", "Diagnóstico original con espacios.gb", "original header fixture");
    require(readFile(gb.value, romPath + "/gb", "Diagnóstico original con espacios.gb") == "original header fixture",
            "Unicode/spaced filename did not round trip");
    writeFile(games.value, configPath + "/games", "settings.json", "old settings");
    writeFile(library.value, cachePath + "/library", "title.meta", "offline metadata");
    require(readFile(library.value, cachePath + "/library", "title.meta") == "offline metadata", "Cache content changed");
    writeFile(configs.value, configPath, "previous.json", "previous intact");
    writeFile(cache.value, cachePath, ".temporary", "replacement");
    Fd previous(ops::openAt(configs.value, configPath, "previous.json", O_RDONLY));
    require(previous.value >= 0, "Cannot retain old destination reader");
    require(ops::renameAt(cache.value, cachePath, ".temporary", configs.value, configPath, "previous.json") == 0,
            "Cross-directory replacement failed");
    require(readFile(configs.value, configPath, "previous.json") == "replacement" && !fs::exists(directory / "cache/.temporary"),
            "Rename did not replace destination and remove source");
    char old[32]{};
    require(::read(previous.value, old, sizeof(old)) == 15 && std::string(old, 15) == "previous intact",
            "Rename truncated an existing reader instead of replacing its file");
    require(ops::unlinkAt(library.value, cachePath + "/library", "title.meta") == 0 &&
            !fs::exists(directory / "cache/library/title.meta"), "Cache cleanup failed");
    require(ops::unlinkAt(library.value, cachePath + "/library", "absent") < 0 && errno == ENOENT,
            "Missing file should report ENOENT");
    rejectedOpen(configs.value, configPath, "previous.json", O_WRONLY | O_CREAT | O_EXCL);
    require(readFile(configs.value, configPath, "previous.json") == "replacement", "Exclusive create damaged prior file");

    // No caller can reinterpret a name as a second path, escape to a sibling,
    // or accidentally traverse a string containing a NUL in parentPath.
    const std::string tooLong(256, 'a');
    for (const auto& name : {std::string(), std::string("."), std::string(".."), std::string("../escape"),
                            std::string("/absolute"), std::string("nested/file"), std::string("back\\slash"), tooLong}) {
        rejectedOpen(parent.value, root, name.c_str(), O_WRONLY | O_CREAT | O_EXCL);
        require(ops::mkdirAt(parent.value, root, name.c_str(), 0700) < 0, "Unsafe mkdir name accepted");
        require(ops::unlinkAt(parent.value, root, name.c_str()) < 0, "Unsafe unlink name accepted");
        require(ops::renameAt(parent.value, root, name.c_str(), configs.value, configPath, "previous.json") < 0,
                "Unsafe rename source accepted");
        require(ops::renameAt(configs.value, configPath, "previous.json", parent.value, root, name.c_str()) < 0,
                "Unsafe rename destination accepted");
    }
    require(readFile(configs.value, configPath, "previous.json") == "replacement", "Invalid name damaged existing file");
    for (const auto& bad : {std::string("relative"), root + "/..", root + "/.", root + "/../" + directory.filename().string(),
                           root + std::string("\0ignored", 8), root + "\\ignored"}) {
        rejectedOpen(parent.value, bad, "previous.json", O_WRONLY | O_CREAT | O_EXCL);
        require(ops::mkdirAt(parent.value, bad, "must-not-exist", 0700) < 0, "Unsafe parent path created a directory");
    }
    require(readFile(configs.value, configPath + "///", "previous.json") == "replacement",
            "Trailing separators should not change parent identity");
    rejectedOpen(parent.value, configPath, "previous.json", O_WRONLY | O_TRUNC);
    require(ops::mkdirAt(parent.value, configPath, "wrong-parent", 0700) < 0, "Mismatched parent fd allowed mkdir");
    require(ops::unlinkAt(parent.value, configPath, "previous.json") < 0, "Mismatched parent fd allowed unlink");
    require(readFile(configs.value, configPath, "previous.json") == "replacement", "Parent mismatch damaged file");
    rejectedOpen(-1, root, "roms", O_RDONLY | O_DIRECTORY);
    rejectedOpen(previous.value, configPath + "/previous.json", "child", O_RDONLY | O_NONBLOCK);

    writeFile(parent.value, root, "outside.txt", "outside unchanged");
    const auto outside = directory / "outside.txt";
    fs::create_symlink(outside, directory / "configs/file-link");
    fs::create_symlink(directory / "missing-target", directory / "configs/dangling-link");
    fs::create_directory_symlink(directory / "configs", directory / "parent-link");
    fs::create_directory_symlink(directory / "roms/gb", directory / "roms/gb-link");
    // The helper rejects links even if the caller omitted O_NOFOLLOW.
    rejectedOpen(configs.value, configPath, "file-link", O_RDONLY | O_NONBLOCK);
    rejectedOpen(configs.value, configPath, "file-link", O_WRONLY | O_TRUNC | O_NOFOLLOW);
    rejectedOpen(configs.value, configPath, "dangling-link", O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW);
    rejectedOpen(roms.value, romPath, "gb-link", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    rejectedOpen(configs.value, root + "/parent-link", "previous.json", O_RDONLY | O_NONBLOCK);
    rejectedOpen(games.value, root + "/parent-link/games", "settings.json", O_RDONLY | O_NONBLOCK);
    require(ops::mkdirAt(configs.value, root + "/parent-link", "blocked", 0700) < 0, "Linked parent allowed mkdir");
    require(ops::unlinkAt(configs.value, root + "/parent-link", "previous.json") < 0, "Linked parent allowed unlink");
    require(ops::unlinkAt(configs.value, configPath, "file-link") < 0 && fs::is_symlink(directory / "configs/file-link"),
            "Unlink silently accepted a symlink");
    require(ops::mkdirAt(configs.value, configPath, "file-link", 0700) < 0, "mkdir accepted child link");
    require(::mkfifo((directory / "configs/fifo").c_str(), 0600) == 0, "Cannot create FIFO fixture");
    rejectedOpen(configs.value, configPath, "fifo", O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    require(ops::unlinkAt(configs.value, configPath, "fifo") < 0 && fs::is_fifo(directory / "configs/fifo"),
            "Special-file unlink must be refused");
    require(ops::unlinkAt(configs.value, configPath, "games") < 0 && fs::is_directory(directory / "configs/games"),
            "Directory unlink must be refused");
    fs::create_hard_link(outside, directory / "configs/hard-link");
    require(ops::unlinkAt(configs.value, configPath, "hard-link") < 0, "Hard-linked file cleanup must be refused");

    // Every failed replacement preserves both source and the destination. This
    // covers dangling names, unsafe parents and regular-file/link distinctions.
    writeFile(cache.value, cachePath, "new.meta", "new still present");
    require(ops::renameAt(cache.value, cachePath, "absent", configs.value, configPath, "previous.json") < 0,
            "Missing rename source succeeded");
    require(ops::renameAt(parent.value, cachePath, "new.meta", configs.value, configPath, "previous.json") < 0,
            "Mismatched source parent allowed rename");
    require(ops::renameAt(cache.value, cachePath, "new.meta", parent.value, configPath, "previous.json") < 0,
            "Mismatched target parent allowed rename");
    require(ops::renameAt(cache.value, cachePath, "new.meta", configs.value, root + "/parent-link", "previous.json") < 0,
            "Linked target parent allowed rename");
    for (const char* destination : {"file-link", "dangling-link", "hard-link", "fifo", "games"})
        require(ops::renameAt(cache.value, cachePath, "new.meta", configs.value, configPath, destination) < 0,
                "Unsafe destination accepted by rename");
    for (const char* name : {"file-link", "dangling-link", "hard-link", "fifo", "games"})
        require(ops::renameAt(configs.value, configPath, name, configs.value, configPath, "previous.json") < 0,
                "Unsafe source accepted by rename");
    require(readFile(cache.value, cachePath, "new.meta") == "new still present", "Failed rename lost source");
    require(readFile(configs.value, configPath, "previous.json") == "replacement", "Failed rename damaged previous destination");
    require(readFile(parent.value, root, "outside.txt") == "outside unchanged", "Link operation modified outside target");
    require(fs::is_symlink(directory / "configs/file-link") && fs::is_symlink(directory / "configs/dangling-link") &&
            fs::is_directory(directory / "configs/games"), "Rejected operation changed filesystem object type");

    // A descriptor from before a directory replacement must not authorize
    // access to a newly created directory at the same spelling.
    fs::rename(directory / "configs", directory / "configs-original");
    fs::create_directory(directory / "configs");
    {
        std::ofstream victim(directory / "configs/previous.json", std::ios::binary);
        victim << "new parent untouched";
        require(victim.good(), "Cannot seed replaced parent");
    }
    rejectedOpen(configs.value, configPath, "previous.json", O_WRONLY | O_TRUNC);
    require(ops::unlinkAt(configs.value, configPath, "previous.json") < 0, "Stale descriptor allowed unlink");
    require(ops::renameAt(cache.value, cachePath, "new.meta", configs.value, configPath, "previous.json") < 0,
            "Stale descriptor allowed rename");
    {
        std::ifstream victim(directory / "configs/previous.json", std::ios::binary);
        const std::string contents{std::istreambuf_iterator<char>(victim), std::istreambuf_iterator<char>()};
        require(contents == "new parent untouched", "Stale descriptor damaged replacement parent");
    }
    require(r2n64FileOpsAtCalls() == 0, "PS4 path backend called an unavailable *at function");
}
}
int main() {
    char pattern[] = "/tmp/r2n64-fileops-XXXXXX";
    const char* created = ::mkdtemp(pattern);
    if (!created) return 1;
    const fs::path temporary(created);
    try {
        exercise(temporary / "local disk E/R2N64 data/EINVAL", EINVAL);
        exercise(temporary / "local disk E/R2N64 data/ENOSYS", ENOSYS);
        r2n64FileOpsSetAtError(0);
        fs::remove_all(temporary);
        std::cout << "PASS: PS4 path file operations, unavailable *at calls, spaced paths, links and failed replacement guards\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\nFixtures retained: " << temporary << '\n';
        return 1;
    }
}
