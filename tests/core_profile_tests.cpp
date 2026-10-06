// Integration coverage uses only the project's original synthetic RDP ROM.
#include "emulator.h"
#include "log.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

using namespace r2n64;
namespace fs = std::filesystem;
namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
void empty(const R2N64CoreProfile& p) {
    require(p.abi_version == R2N64_CORE_PROFILE_ABI && p.struct_size == sizeof(p), "Profiler ABI mismatch");
    require(!p.run_us && !p.run_calls && !p.rsp_us && !p.rsp_calls && !p.rdp_us && !p.rdp_calls &&
            !p.scanout_us && !p.scanout_calls && !p.audio_hle_us && !p.audio_hle_calls &&
            !p.timer_failures && !p.dropped_scopes, "Disabled/reset profiler retained counters");
}
void active(const R2N64CoreProfile& p, unsigned runs) {
    require(p.abi_version == R2N64_CORE_PROFILE_ABI && p.struct_size == sizeof(p), "Profiler ABI mismatch");
    require(p.run_calls == runs && p.run_us > 0, "retro_run profiling did not count actual calls");
    require(p.rdp_calls > 0 && p.rdp_us > 0, "Original RDP workload did not reach rasterizer profiling");
    require(p.scanout_calls > 0 && p.scanout_us > 0, "Original RDP workload did not reach scanout profiling");
    require(!p.rsp_calls && !p.audio_hle_calls, "Raw-RDP fixture was mislabeled as an RSP/audio task");
    require(p.rsp_us + p.rdp_us + p.scanout_us + p.audio_hle_us <= p.run_us,
            "Exclusive component elapsed time exceeded inclusive retro_run elapsed time");
    require(!p.timer_failures && !p.dropped_scopes, "Profiler timer or nesting error");
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Usage: core_profile_tests rdp-workload.z64 output-directory");
        const auto data = fs::absolute(argv[2]) / ("data-" + std::to_string(::getpid()));
        fs::create_directories(data / "logs");
        Log log;
        require(log.open(data.string()), "Cannot open isolated test log");
        Emulator core;
        GamepadInput input{};
        input.connected = true;
        std::string error;
        CoreFrame reference;
        for (const bool enabled : {false, true, false}) {
            EmulationConfig config;
            config.workers = 4;
            config.profileCore = enabled;
            require(core.load(argv[1], data.string(), log, error, config), error);
            empty(core.coreProfile());
            for (unsigned vi = 0; vi < 10; ++vi) require(core.run(input, error), error);
            const auto snapshot = core.coreProfile();
            if (enabled) active(snapshot, 10);
            else empty(snapshot);
            require(core.frame().width == 320 && core.frame().height > 200, "Missing actual RDP frame");
            if (reference.pixels.empty()) reference = core.frame();
            else require(reference.width == core.frame().width && reference.height == core.frame().height &&
                         reference.pixels == core.frame().pixels, "Profiling altered RDP output");
            // Sampling/pausing while the libco stack is parked cannot accrue time.
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            const auto paused = core.coreProfile();
            require(paused.run_us == snapshot.run_us && paused.rdp_us == snapshot.rdp_us &&
                    paused.scanout_us == snapshot.scanout_us, "Frontend idle was charged to core components");
            if (enabled) {
                retro_r2n64_profile_reset();
                empty(core.coreProfile());
                for (unsigned vi = 0; vi < 3; ++vi) require(core.run(input, error), error);
                active(core.coreProfile(), 3);
                retro_r2n64_profile_set_enabled(0);
                require(core.run(input, error), error);
                empty(core.coreProfile());
                std::cout << "PROFILE: run_us=" << snapshot.run_us << " rdp_us=" << snapshot.rdp_us
                          << " scanout_us=" << snapshot.scanout_us << " rdp_calls=" << snapshot.rdp_calls << '\n';
            }
            core.unload();
        }
        std::cout << "PASS: actual RDP/scanout scopes, exclusive accounting, disabled/reset counters, identical pixels and idle exclusion.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: " << exception.what() << '\n';
        return 1;
    }
}
