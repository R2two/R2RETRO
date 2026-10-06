// Independent integer oracle and measured real-code x64 recompiler workload.
#include "emulator.h"
#include "log.h"
#include <libretro.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <vector>

using namespace r2n64;
namespace fs = std::filesystem;
namespace {
void require(bool condition, const std::string& error) {
    if (!condition) throw std::runtime_error(error);
}
uint32_t ramWord(size_t offset) {
    require(retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM) >= offset + 4, "Missing RDRAM");
    const auto* ram = static_cast<const unsigned char*>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
    require(ram != nullptr, "Missing RDRAM");
    uint32_t value;
    std::memcpy(&value, ram + offset, sizeof(value));
    return value;
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Usage: cpu_benchmark workload.z64 output-directory");
        const fs::path output = fs::absolute(argv[2]);
        const auto data = output / ("data-" + std::to_string(::getpid()));
        fs::create_directories(data / "logs");
        Log log;
        require(log.open(data.string()), "Cannot open log");
        uint32_t expected = 0x12345678, checksum = 0;
        for (unsigned i = 0; i < 8192; ++i) {
            expected = expected * 1664525u + 1013904223u;
            expected ^= expected >> 13;
            checksum += expected;
        }
        Emulator core;
        GamepadInput input{};
        input.connected = true;
        std::string error;
        std::vector<double> timings;
        std::vector<uint32_t> batches;
        constexpr unsigned SampleFrames = 30;
        for (const auto mode : {CpuMode::CachedInterpreter, CpuMode::Automatic,
                                CpuMode::CachedInterpreter, CpuMode::Automatic}) {
            require(core.load(argv[1], data.string(), log, error, EmulationConfig{1, mode}), error);
            for (unsigned frame = 0; frame < 5; ++frame) require(core.run(input, error), error);
            require(core.usingRecompiler() == (mode == CpuMode::Automatic), "Expected CPU backend did not execute");
            require(ramWord(0x400) == 0x52324350 && ramWord(0x404) > 0, "CPU benchmark did not finish a batch");
            require(ramWord(0x408) == expected && ramWord(0x40c) == checksum, "Integer or branch-delay oracle mismatch");
            const auto counter = ramWord(0x404);
            const auto start = std::chrono::steady_clock::now();
            for (unsigned frame = 0; frame < SampleFrames; ++frame) require(core.run(input, error), error);
            const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            const uint32_t completed = ramWord(0x404) - counter;
            require(completed > SampleFrames, "CPU workload stopped making progress");
            require(ramWord(0x408) == expected && ramWord(0x40c) == checksum, "Integer oracle drifted after warm-up");
            timings.push_back(milliseconds);
            batches.push_back(completed);
            std::cout << "CPU=" << core.cpuName() << " frames=" << SampleFrames << " batches=" << completed
                      << " ms=" << milliseconds << " result=" << std::hex << expected << ":" << checksum << std::dec << '\n';
            core.unload();
        }
        const double cached = (timings[0] + timings[2]) / 2;
        const double dynarec = (timings[1] + timings[3]) / 2;
        const double cachedPerBatch = (timings[0] + timings[2]) / (batches[0] + batches[2]);
        const double dynarecPerBatch = (timings[1] + timings[3]) / (batches[1] + batches[3]);
        std::ofstream json(output / "cpu-benchmark.json");
        json << std::fixed << std::setprecision(3)
             << "{\n  \"workload\": \"original MIPS integer recurrence in RDRAM, desktop\",\n"
             << "  \"sample_frames\": " << SampleFrames << ",\n  \"cached_ms\": " << cached
             << ",\n  \"dynarec_ms\": " << dynarec << ",\n  \"ratio_cached_over_dynarec\": " << cached / dynarec
             << ",\n  \"ratio_per_integer_batch\": " << cachedPerBatch / dynarecPerBatch
             << ",\n  \"sample_order\": [\"cached\", \"dynarec\", \"cached\", \"dynarec\"],\n  \"completed_batches\": ["
             << batches[0] << ", " << batches[1] << ", " << batches[2] << ", " << batches[3] << "]"
             << ",\n  \"integer_oracle_passed\": true,\n  \"ps4_measured\": false\n}\n";
        require(json.good(), "Cannot save benchmark result");
        std::cout << "PASS: cached/JIT integer results match the independent oracle; desktop ratio="
                  << cached / dynarec << ", per batch=" << cachedPerBatch / dynarecPerBatch << ".\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: " << exception.what() << '\n';
        return 1;
    }
}
