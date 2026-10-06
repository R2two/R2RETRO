// Original RDP workload: exact 1/4-worker framebuffer oracle and timed samples.
#include "emulator.h"
#include "log.h"
#include <libretro.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

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
uint64_t digest(const CoreFrame& frame) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const uint32_t pixel : frame.pixels) {
        hash ^= pixel;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Usage: rdp_benchmark workload.z64 output-directory");
        const fs::path output = fs::absolute(argv[2]);
        const auto data = output / ("data-" + std::to_string(::getpid()));
        fs::create_directories(data / "logs");
        Log log;
        require(log.open(data.string()), "Cannot open log");
        Emulator core;
        GamepadInput input{};
        input.connected = true;
        std::string error;
        CoreFrame reference;
        std::vector<double> timings;
        constexpr unsigned SampleFrames = 40;
        for (const unsigned workers : {1u, 4u, 1u, 4u}) {
            require(core.load(argv[1], data.string(), log, error, EmulationConfig{workers}), error);
            for (unsigned frame = 0; frame < 5; ++frame) require(core.run(input, error), error);
            require(ramWord(0x400) == 0x52324450, "RDP workload CPU signature missing");
            const auto counter = ramWord(0x404);
            const auto start = std::chrono::steady_clock::now();
            for (unsigned frame = 0; frame < SampleFrames; ++frame) require(core.run(input, error), error);
            const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            require(ramWord(0x404) - counter >= SampleFrames - 2, "RDP workload stopped submitting frames");
            const auto& frame = core.frame();
            require(frame.width == 320 && frame.height > 200, "RDP video missing or malformed");
            unsigned red = 0, green = 0, blue = 0;
            for (const auto pixel : frame.pixels) {
                const auto r = (pixel >> 16) & 255, g = (pixel >> 8) & 255, b = pixel & 255;
                red += r > 160 && g < 80 && b < 80;
                green += g > 160 && r < 80 && b < 80;
                blue += b > 160 && r < 80 && g < 80;
            }
            require(red > 1000 && green > 1000 && blue > 1000, "RDP did not rasterize RGB bands");
            if (reference.pixels.empty()) reference = frame;
            else require(reference.width == frame.width && reference.height == frame.height && reference.pixels == frame.pixels,
                         "RDP worker profiles produced different pixels");
            timings.push_back(milliseconds);
            std::cout << "RDP workers=" << workers << " frames=" << SampleFrames << " ms=" << milliseconds
                      << " digest=" << std::hex << digest(frame) << std::dec << '\n';
            core.unload();
        }
        const double one = (timings[0] + timings[2]) / 2, four = (timings[1] + timings[3]) / 2;
        std::ofstream json(output / "rdp-benchmark.json");
        json << std::fixed << std::setprecision(3)
             << "{\n  \"workload\": \"original RDP one-cycle rectangles, desktop\",\n"
             << "  \"sample_frames\": " << SampleFrames << ",\n  \"workers_1_ms\": " << one
             << ",\n  \"workers_4_ms\": " << four << ",\n  \"ratio_1_over_4\": " << one / four
             << ",\n  \"same_pixels\": true,\n  \"ps4_measured\": false\n}\n";
        std::cout << "PASS: four 1/4-worker sessions, identical RDP output; desktop workload ratio=" << one / four << ".\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: " << exception.what() << '\n';
        return 1;
    }
}
