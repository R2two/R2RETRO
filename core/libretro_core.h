#pragma once
#include "core/core_interface.h"
#include "core/core_registry.h"
#include <libretro.h>
#include <memory>

namespace r2n64 {
// Each linked core supplies the same libretro ABI, with isolated export names.
// Optional N64 diagnostics stay outside the common libretro callbacks.
struct CoreApi {
    decltype(&retro_init) init;
    decltype(&retro_deinit) deinit;
    decltype(&retro_api_version) api_version;
    decltype(&retro_get_system_info) get_system_info;
    decltype(&retro_get_system_av_info) get_system_av_info;
    decltype(&retro_set_environment) set_environment;
    decltype(&retro_set_video_refresh) set_video_refresh;
    decltype(&retro_set_audio_sample) set_audio_sample;
    decltype(&retro_set_audio_sample_batch) set_audio_sample_batch;
    decltype(&retro_set_input_poll) set_input_poll;
    decltype(&retro_set_input_state) set_input_state;
    decltype(&retro_set_controller_port_device) set_controller_port_device;
    decltype(&retro_reset) reset;
    decltype(&retro_run) run;
    decltype(&retro_load_game) load_game;
    decltype(&retro_unload_game) unload_game;
    decltype(&retro_serialize_size) serialize_size;
    decltype(&retro_serialize) serialize;
    decltype(&retro_unserialize) unserialize;
    decltype(&retro_get_memory_data) get_memory_data;
    decltype(&retro_get_memory_size) get_memory_size;
    int (*dynarec_available)() = nullptr;
    int (*dynarec_active)() = nullptr;
    void (*set_audio_hle)(int) = nullptr;
    uint64_t (*audio_hle_tasks)() = nullptr;
    void (*profile_set_enabled)(int) = nullptr;
    void (*profile_reset)() = nullptr;
    void (*profile_read)(R2N64CoreProfile*) = nullptr;
    uint64_t (*graphics_hle_tasks)() = nullptr;
    uint64_t (*graphics_lle_tasks)() = nullptr;
};
const CoreApi* coreApiFor(SystemType system);

class LibretroCore final : public IEmulationCore {
public:
    LibretroCore(const CoreDescriptor& descriptor, const CoreApi& api);
    ~LibretroCore() override;
    LibretroCore(const LibretroCore&) = delete;
    LibretroCore& operator=(const LibretroCore&) = delete;
    bool load(const std::string&, const std::string&, Log&, std::string&, EmulationConfig) override;
    bool run(const GamepadInput&, std::string&) override;
    void unload() override;
    bool loaded() const override;
    const CoreFrame& frame() const override;
    const std::vector<int16_t>& audio() const override;
    double fps() const override;
    double sampleRate() const override;
    SystemType system() const override;
    const char* coreName() const override;
    bool usingRecompiler() const override;
    const char* cpuName() const override;
    uint64_t audioHleTasks() const override;
    R2N64CoreProfile coreProfile() const override;
    uint64_t graphicsHleTasks() const override;
    uint64_t graphicsLleTasks() const override;
    HardwareTiming hardwareTiming() const override;
    bool supportsSaveStates() const override;
    bool saveState(std::string&, unsigned slot = 0) override;
    bool loadState(std::string&, unsigned slot = 0) override;
    bool setGameBoyPalette(unsigned, std::string&) override;
    bool reset(std::string&) override;
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
