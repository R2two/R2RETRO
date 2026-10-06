// Simulate unavailable PacBrew *at entry points. Link with --wrap for openat,
// mkdirat, renameat, unlinkat and fstatat. Plain POSIX fixture operations and
// filesystem calls inside the shared C++ runtime are deliberately untouched.
#include <atomic>
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>

namespace {
std::atomic<unsigned> calls{0};
std::atomic<int> forcedError{0};
int unavailable(int fallback) {
    calls.fetch_add(1, std::memory_order_relaxed);
    const int requested = forcedError.load(std::memory_order_relaxed);
    errno = requested ? requested : fallback;
    return -1;
}
}
extern "C" unsigned r2n64FileOpsAtCalls() { return calls.load(std::memory_order_relaxed); }
extern "C" void r2n64FileOpsResetAtCalls() { calls.store(0, std::memory_order_relaxed); }
extern "C" void r2n64FileOpsSetAtError(int error) { forcedError.store(error, std::memory_order_relaxed); }
extern "C" int __wrap_openat(int, const char*, int, ...) { return unavailable(EINVAL); }
extern "C" int __wrap_mkdirat(int, const char*, mode_t) { return unavailable(ENOSYS); }
extern "C" int __wrap_renameat(int, const char*, int, const char*) { return unavailable(ENOSYS); }
extern "C" int __wrap_unlinkat(int, const char*, int) { return unavailable(ENOSYS); }
extern "C" int __wrap_fstatat(int, const char*, struct stat*, int) { return unavailable(ENOSYS); }
