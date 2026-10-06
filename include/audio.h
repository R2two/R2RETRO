#pragma once
#include <SDL2/SDL.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace r2n64 {
class Audio {
public:
    ~Audio();
    Audio() = default;
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;
    bool initialize(std::string& error);
    bool start(double sourceRate, std::string& error);
    void push(const int16_t* samples, size_t frames);
    void clear();
    void pause(bool paused);
    void shutdown();
    bool ready() const { return device_ != 0 && !failed_; }
    const std::string& error() const { return error_; }
    size_t queuedFrames() const;
    static constexpr int OutputRate = 48000;
    static constexpr size_t MaxQueuedFrames = OutputRate / 5; // 200 ms, stereo.
private:
    void fail(const char* operation);
    SDL_AudioDeviceID device_ = 0;
    SDL_AudioStream* stream_ = nullptr;
    int sourceRate_ = 0;
    bool initialized_ = false, attempted_ = false, failed_ = false;
    std::string error_;
};
}
