#pragma once
#include "log.h"
#include "platform.h"
#include "rom.h"
#include "video.h"
#include "emulator.h"
#include "audio.h"
#include "library_service.h"
#include "update.h"
#include <atomic>
#include <thread>
namespace r2n64 {
class App {
public:
    ~App();
    int run(bool smoke, const std::string& screenshot, bool emulationSmoke = false, bool accelerated = false,
            bool gpuSmoke = false, const std::string& romSmoke = "",
            const std::string& librarySmoke = "", bool libraryOffline = false, bool n64Gpu = false, bool n64GraphicsHle = true, bool n64Auto = false);
private:
    void scan();
    bool gpuDiagnostic(std::string& error, bool smoke = false, const std::string& screenshot = "");
    bool play(const std::string& path, const std::string& title, std::string& error,
              bool smoke = false, const std::string& screenshot = "", unsigned smokeFrames = 90);
    Platform platform_;
    Video video_;
    Log log_;
    Audio audio_;
    Emulator emulator_;
    EmulationConfig emulationConfig_{};
    LibraryService library_;
    UpdateService updater_;
    std::string scanOverride_;
    std::thread worker_;
    std::atomic<bool> cancel_{false}, ready_{false};
    ScanResult pending_;
    bool platformReady_ = false;
    bool quitting_ = false;
};
}
