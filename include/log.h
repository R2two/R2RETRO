#pragma once
#include <cstdio>
#include <mutex>
#include <string>
namespace r2n64 {
class Log {
public:
    ~Log();
    bool open(const std::string& root);
    void write(const char* level, const std::string& message);
private:
    FILE* file_ = nullptr;
    std::mutex mutex_;
};
}

