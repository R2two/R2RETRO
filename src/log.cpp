#include "log.h"
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
namespace r2n64 {
Log::~Log() { if (file_) std::fclose(file_); }
bool Log::open(const std::string& root) {
    const int fd = ::open((root + "/logs/r2n64.log").c_str(), O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600);
    if (fd < 0) return false;
    file_ = fdopen(fd, "a");
    if (!file_) ::close(fd);
    return file_ != nullptr;
}
void Log::write(const char* level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = std::time(nullptr);
    std::tm tm{};
    localtime_r(&now, &tm);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm);
    std::fprintf(stderr, "[%s] [%s] %s\n", stamp, level, message.c_str());
    if (file_) {
        std::fprintf(file_, "[%s] [%s] %s\n", stamp, level, message.c_str());
        std::fflush(file_);
    }
}
}
