#pragma once
#include "platform.h"
#include "core_profile.h"
#include "core/system_type.h"
#include "core/hardware_video.h"
#include <cstdint>
#include <string>
#include <vector>

namespace r2n64 {
class Log;
enum class CpuMode { Automatic, CachedInterpreter };
constexpr unsigned SaveStateSlotCount = 5;
constexpr unsigned GameBoyPaletteCount = 4;
struct EmulationConfig {
    // N64-specific options. Portable cores retain their native CPU/video paths.
    // Workers are requested Angrylion workers, not an available-CPU claim.
    unsigned workers = 4;
    // Automatic first probes actual executable-cache permission after startup.
    CpuMode cpuMode = CpuMode::Automatic;
    // Known audio microcodes only; unknown tasks and graphics remain LLE.
    bool audioHle = true;
    // Optional component timers, disabled during normal performance runs.
    bool profileCore = false;
    // SameBoy DMG palette: grey, lime, olive, teal. Other systems ignore this.
    unsigned gbPalette = 0;
    N64Graphics graphics = N64Graphics::Software;
    HardwareVideoHost* hardware = nullptr; // Borrowed; ignored in software mode.
    bool graphicsHle = true; // Only GLideN64: recognized graphics, CXD4 otherwise.
    bool automaticProfile = false; // Frontend opt-in; explicit test/core callers stay manual.
    unsigned gbaFrameskip = 0; // mGBA only; preserve CPU/APU execution while reducing drawing.
};
struct HardwareTiming { double beginMs = 0, endMs = 0; uint64_t calls = 0; };
struct CoreFrame {
    // Frontend-owned numeric 0x00RRGGBB, regardless of the core's source format.
    std::vector<uint32_t> pixels;
    unsigned width = 0, height = 0;
    bool hardware = false;
    bool bottomLeftOrigin = false;
};

// All calls remain on the frontend thread; N64 uses cooperative stacks.
class IEmulationCore {
public:
    virtual ~IEmulationCore() = default;
    virtual bool load(const std::string& romPath, const std::string& dataRoot,
                      Log& log, std::string& error, EmulationConfig config) = 0;
    virtual bool run(const GamepadInput& input, std::string& error) = 0;
    virtual void unload() = 0;
    virtual bool loaded() const = 0;
    virtual const CoreFrame& frame() const = 0;
    virtual const std::vector<int16_t>& audio() const = 0;
    virtual double fps() const = 0;
    virtual double sampleRate() const = 0;
    virtual SystemType system() const = 0;
    virtual const char* coreName() const = 0;
    virtual bool usingRecompiler() const = 0;
    virtual const char* cpuName() const = 0;
    virtual uint64_t audioHleTasks() const = 0;
    virtual R2N64CoreProfile coreProfile() const = 0;
    virtual uint64_t graphicsHleTasks() const { return 0; }
    virtual uint64_t graphicsLleTasks() const { return 0; }
    virtual HardwareTiming hardwareTiming() const { return {}; }
    virtual EmulationConfig effectiveConfig() const { return {}; }
    virtual const char* profileName() const { return "Manual"; }
    virtual bool supportsSaveStates() const = 0;
    virtual bool saveState(std::string& error, unsigned slot = 0) = 0;
    virtual bool loadState(std::string& error, unsigned slot = 0) = 0;
    virtual bool setGameBoyPalette(unsigned palette, std::string& error) = 0;
    virtual bool setGbaFrameskip(unsigned, std::string& error) { error = "Salto de cuadros no disponible."; return false; }
    virtual bool saveBattery(std::string& error) { error = "Guardado de cartucho no disponible."; return false; }
    virtual bool reset(std::string& error) = 0;
};
}
