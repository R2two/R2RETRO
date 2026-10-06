#include "startup.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef R2N64_PS4
#include <orbis/libkernel.h>
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>
#endif

namespace r2n64 {
namespace {
int logFd = -1;
char history[16384]{};
size_t historySize = 0;
bool primaryLog = false;

void writeAll(int fd, const char* data, size_t size) {
    while (size) {
        const auto written = ::write(fd, data, size);
        if (written <= 0) return;
        data += written; size -= static_cast<size_t>(written);
    }
}

void tryPrimaryLog() {
    if (primaryLog) return;
#ifdef R2N64_PS4
    const char* root = "/data/R2N64";
#else
    const char* root = std::getenv("R2N64_DATA");
    if (!root || !*root) root = "runtime";
#endif
    ::mkdir(root, 0700);
    struct stat info{};
    if (::lstat(root, &info) < 0 || !S_ISDIR(info.st_mode)) return;
    char path[1024];
    const int length = std::snprintf(path, sizeof(path), "%s/startup.log", root);
    if (length < 0 || static_cast<size_t>(length) >= sizeof(path)) return;
    const int fd = ::open(path, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600);
    if (fd < 0) return;
    if (logFd >= 0) ::close(logFd);
    logFd = fd; primaryLog = true;
    // Preserve stages that happened before /data was available.
    writeAll(logFd, history, historySize);
}
}

void startupBegin() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    tryPrimaryLog();
#ifdef R2N64_PS4
    if (logFd < 0)
        logFd = ::open("/download0/R2N64-startup.log", O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600);
#endif
    startupLog("main", "R2RETRO " R2N64_VERSION " boot");
}

void startupLog(const char* stage, const char* detail) {
    char line[1024];
    std::snprintf(line, sizeof(line), "[R2RETRO " R2N64_VERSION "] %s%s%s\n",
                  stage, detail ? ": " : "", detail ? detail : "");
    const size_t length = std::strlen(line);
    std::fputs(line, stdout);
#ifdef R2N64_PS4
    sceKernelDebugOutText(0, "%s", line);
#endif
    if (length <= sizeof(history) - historySize) {
        std::memcpy(history + historySize, line, length); historySize += length;
    }
    if (logFd >= 0) writeAll(logFd, line, length);
}

void startupDataReady() { tryPrimaryLog(); }

int startupFailure(const char* stage, const char* detail) {
    startupLog(stage, detail);
#ifdef R2N64_PS4
    OrbisNotificationRequest request{};
    request.targetId = -1;
    std::snprintf(request.message, sizeof(request.message), "R2RETRO: %s: %.650s",
                  stage, detail ? detail : "Error de inicio");
    sceKernelSendNotificationRequest(0, &request, sizeof(request), 0);
#endif
    return 1;
}

int finishApplication(int result) {
    startupLog("exit", result ? "failed; see startup.log" : "normal");
    if (logFd >= 0) { ::close(logFd); logFd = -1; }
#ifdef R2N64_PS4
    // Run only after App/SDL destructors, including on a handled startup failure.
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
    sceSystemServiceLoadExec("exit", nullptr);
#endif
    return result;
}
}
