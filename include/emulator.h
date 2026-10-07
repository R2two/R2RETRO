#pragma once
#include "core/core_interface.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace r2n64 {

// One core session on the calling thread. All methods, including destruction,
// must run on that same thread because the core uses cooperative stacks.
class Emulator {
public:
    Emulator();
    ~Emulator();
    Emulator(const Emulator&) = delete;
    Emulator& operator=(const Emulator&) = delete;
    bool load(const std::string& romPath, const std::string& dataRoot,
              Log& log, std::string& error, EmulationConfig config = {});
    bool run(const GamepadInput& input, std::string& error);
    void unload();
    bool loaded() const;
    const CoreFrame& frame() const;
    // Signed native-endian 16-bit stereo, interleaved, from the latest run.
    const std::vector<int16_t>& audio() const;
    double fps() const;
    double sampleRate() const;
    SystemType system() const;
    const char* coreName() const;
    uint64_t graphicsHleTasks() const;
    uint64_t graphicsLleTasks() const;
    HardwareTiming hardwareTiming() const;
    EmulationConfig effectiveConfig() const;
    const char* profileName() const;
    bool supportsSaveStates() const;
    bool saveState(std::string& error, unsigned slot = 0);
    bool loadState(std::string& error, unsigned slot = 0);
    // Applies at the next run; does not reset the cartridge or mutate saves.
    bool setGameBoyPalette(unsigned palette, std::string& error);
    bool setGbaFrameskip(unsigned frameskip, std::string& error);
    bool saveBattery(std::string& error);
    bool reset(std::string& error);
    bool usingRecompiler() const;
    // The running backend, including a core-side fallback after load.
    const char* cpuName() const;
    uint64_t audioHleTasks() const;
    R2N64CoreProfile coreProfile() const;
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
