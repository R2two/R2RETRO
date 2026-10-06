#include "file_ops.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace r2n64::fileops {
namespace {
bool validName(const char* name) {
    if (!name || !*name || !std::strcmp(name, ".") || !std::strcmp(name, "..") ||
        std::strchr(name, '/') || std::strchr(name, '\\')) {
        errno = EINVAL; return false;
    }
    if (std::strlen(name) > 255) { errno = ENAMETOOLONG; return false; }
    return true;
}

#if defined(R2N64_PS4) || defined(R2N64_PATH_FILEOPS)
int fail(int error) { errno = error; return -1; }
struct Parent {
    std::string path;
    std::vector<struct stat> ancestors;
};
bool sameObject(const struct stat& left, const struct stat& right) {
    return left.st_dev == right.st_dev && left.st_ino == right.st_ino &&
           (left.st_mode & S_IFMT) == (right.st_mode & S_IFMT);
}
bool directoryStat(const std::string& path, struct stat& info) {
    if (::lstat(path.c_str(), &info) < 0) return false;
    if (S_ISLNK(info.st_mode)) { errno = ELOOP; return false; }
    if (!S_ISDIR(info.st_mode)) { errno = ENOTDIR; return false; }
    return true;
}
bool inspectParent(int fd, const std::string& path, Parent& out) {
    if (path.empty() || path.front() != '/' || path.find('\0') != std::string::npos ||
        path.find('\\') != std::string::npos) { errno = EINVAL; return false; }
    // Reject the entire invalid path before touching even its first component.
    std::vector<std::string> parts;
    for (size_t start = 0; start < path.size();) {
        const auto slash = path.find('/', start);
        const auto end = slash == std::string::npos ? path.size() : slash;
        auto part = path.substr(start, end - start);
        start = end + 1;
        if (part.empty()) continue;
        if (!validName(part.c_str())) return false;
        parts.push_back(std::move(part));
    }
    struct stat descriptor{};
    if (::fstat(fd, &descriptor) < 0) return false;
    if (!S_ISDIR(descriptor.st_mode)) { errno = ENOTDIR; return false; }
    out.path = "/";
    out.ancestors.clear();
    struct stat current{};
    if (!directoryStat(out.path, current)) return false;
    out.ancestors.push_back(current);
    for (const auto& part : parts) {
        if (out.path.size() > 1) out.path += '/';
        out.path += part;
        if (!directoryStat(out.path, current)) return false;
        out.ancestors.push_back(current);
    }
    if (!sameObject(descriptor, current)) { errno = EAGAIN; return false; }
    return true;
}
bool unchangedParent(int fd, const Parent& expected) {
    Parent now;
    if (!inspectParent(fd, expected.path, now)) return false;
    if (now.ancestors.size() != expected.ancestors.size()) { errno = EAGAIN; return false; }
    for (size_t i = 0; i < now.ancestors.size(); ++i)
        if (!sameObject(now.ancestors[i], expected.ancestors[i])) { errno = EAGAIN; return false; }
    return true;
}
std::string childPath(const Parent& parent, const char* name) {
    return parent.path + (parent.path == "/" ? "" : "/") + name;
}
bool childStat(const std::string& path, bool missingAllowed, struct stat& info, bool& exists) {
    if (::lstat(path.c_str(), &info) < 0) {
        exists = false;
        return missingAllowed && errno == ENOENT;
    }
    exists = true;
    if (S_ISLNK(info.st_mode)) { errno = ELOOP; return false; }
    if (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode)) { errno = EINVAL; return false; }
    return true;
}
bool regularChild(const std::string& path, bool missingAllowed) {
    struct stat info{};
    bool exists;
    if (!childStat(path, missingAllowed, info, exists)) return false;
    if (exists && (!S_ISREG(info.st_mode) || info.st_nlink != 1)) {
        errno = S_ISDIR(info.st_mode) ? EISDIR : EMLINK; return false;
    }
    return true;
}
int closeFailure(int fd) {
    const int error = errno;
    ::close(fd);
    return fail(error);
}
#endif
}

int openAt(int parentFd, const std::string& parentPath, const char* name, int flags, mode_t mode) {
    if (!validName(name)) return -1;
#if defined(R2N64_PS4) || defined(R2N64_PATH_FILEOPS)
    Parent parent;
    if (!inspectParent(parentFd, parentPath, parent)) return -1;
    const auto path = childPath(parent, name);
    struct stat before{}, opened{}, after{};
    bool existed, exists;
    if (!childStat(path, (flags & O_CREAT) != 0, before, existed)) return -1;
    if (existed && (flags & O_DIRECTORY) && !S_ISDIR(before.st_mode)) return fail(ENOTDIR);
    if (!unchangedParent(parentFd, parent)) return -1;
    // Preserve O_EXCL/O_NONBLOCK and all other caller flags. Never follow the
    // leaf even if a future caller accidentally omitted its no-follow flag.
    const int fd = ::open(path.c_str(), flags | O_NOFOLLOW, mode);
    if (fd < 0) return -1;
    if (::fstat(fd, &opened) < 0) return closeFailure(fd);
    if ((flags & O_DIRECTORY) && !S_ISDIR(opened.st_mode)) {
        errno = ENOTDIR; return closeFailure(fd);
    }
    if (!S_ISREG(opened.st_mode) && !S_ISDIR(opened.st_mode)) {
        errno = EINVAL; return closeFailure(fd);
    }
    if (!unchangedParent(parentFd, parent) || !childStat(path, false, after, exists))
        return closeFailure(fd);
    if (!sameObject(opened, after) || (existed && !sameObject(before, opened))) {
        errno = EAGAIN; return closeFailure(fd);
    }
    return fd;
#else
    (void)parentPath;
    return ::openat(parentFd, name, flags, mode);
#endif
}

int mkdirAt(int parentFd, const std::string& parentPath, const char* name, mode_t mode) {
    if (!validName(name)) return -1;
#if defined(R2N64_PS4) || defined(R2N64_PATH_FILEOPS)
    Parent parent;
    if (!inspectParent(parentFd, parentPath, parent)) return -1;
    const auto path = childPath(parent, name);
    if (!unchangedParent(parentFd, parent)) return -1;
    return ::mkdir(path.c_str(), mode);
#else
    (void)parentPath;
    return ::mkdirat(parentFd, name, mode);
#endif
}

int unlinkAt(int parentFd, const std::string& parentPath, const char* name) {
    if (!validName(name)) return -1;
#if defined(R2N64_PS4) || defined(R2N64_PATH_FILEOPS)
    Parent parent;
    if (!inspectParent(parentFd, parentPath, parent)) return -1;
    const auto path = childPath(parent, name);
    if (!regularChild(path, false) || !unchangedParent(parentFd, parent)) return -1;
    return ::unlink(path.c_str());
#else
    (void)parentPath;
    return ::unlinkat(parentFd, name, 0);
#endif
}

int renameAt(int sourceFd, const std::string& sourceParent, const char* sourceName,
             int destinationFd, const std::string& destinationParent, const char* destinationName) {
    if (!validName(sourceName) || !validName(destinationName)) return -1;
#if defined(R2N64_PS4) || defined(R2N64_PATH_FILEOPS)
    Parent source, destination;
    if (!inspectParent(sourceFd, sourceParent, source) ||
        !inspectParent(destinationFd, destinationParent, destination)) return -1;
    const auto from = childPath(source, sourceName);
    const auto to = childPath(destination, destinationName);
    if (!regularChild(from, false) || !regularChild(to, true) ||
        !unchangedParent(sourceFd, source) || !unchangedParent(destinationFd, destination)) return -1;
    return ::rename(from.c_str(), to.c_str());
#else
    (void)sourceParent; (void)destinationParent;
    return ::renameat(sourceFd, sourceName, destinationFd, destinationName);
#endif
}
}
