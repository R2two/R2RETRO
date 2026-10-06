// Real NES/SNES cores with original CPU/PPU/APU/controller programs only.
#include "emulator.h"
#include "core/libretro_core.h"
#include "log.h"
#include "rom.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <unistd.h>

using namespace r2n64;
namespace fs = std::filesystem;
namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::vector<unsigned char> read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(file), {}};
}
void write(const fs::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    require(bool(file), "Cannot write " + path.string());
}
struct Memory { unsigned char* data; size_t size; };
Memory ram(SystemType system) {
    const auto* api = coreApiFor(system);
    require(api != nullptr, "Console core API unavailable");
    Memory result{static_cast<unsigned char*>(api->get_memory_data(RETRO_MEMORY_SAVE_RAM)),
                  api->get_memory_size(RETRO_MEMORY_SAVE_RAM)};
    require(result.data && result.size > 256, "Console fixture SRAM unavailable");
    return result;
}
std::array<unsigned char, 16> status(SystemType system) {
    std::array<unsigned char, 16> result{};
    std::copy_n(ram(system).data, result.size(), result.begin());
    return result;
}
uint64_t videoHash(const CoreFrame& frame) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (auto pixel : frame.pixels) { hash ^= pixel & 0xffffff; hash *= UINT64_C(1099511628211); }
    return hash;
}
void picture(const fs::path& path, const CoreFrame& frame) {
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
    for (auto pixel : frame.pixels) {
        const char rgb[]{char(pixel >> 16), char(pixel >> 8), char(pixel)};
        out.write(rgb, 3);
    }
    require(bool(out), "Cannot save original diagnostic preview");
}
void advance(Emulator& emulator, const GamepadInput& input, unsigned count, std::string& error) {
    for (unsigned i = 0; i < count; ++i) require(emulator.run(input, error), "Run: " + error);
}
void boot(Emulator& emulator, SystemType system, const fs::path& output, std::string& error) {
    GamepadInput input{}; input.connected = true;
    size_t samples = 0, audible = 0;
    int minimum = 32767, maximum = -32768;
    for (unsigned frame = 0; frame < 360; ++frame) {
        require(emulator.run(input, error), "Boot: " + error);
        require(emulator.audio().size() % 2 == 0, "Console audio is not interleaved stereo");
        if (frame >= 300) {
            samples += emulator.audio().size();
            audible += std::count_if(emulator.audio().begin(), emulator.audio().end(),
                [](int16_t value) { return std::abs(int(value)) > 50; });
            for (auto value : emulator.audio()) {
                minimum = std::min(minimum, int(value)); maximum = std::max(maximum, int(value));
            }
        }
    }
    const auto signature = status(system);
    require(std::memcmp(signature.data(), system == SystemType::SuperNintendo ? "R2SN" : "R2NE", 4) == 0,
            std::string(systemId(system)) + " CPU program did not boot");
    require(signature[6] == 0 && signature[7] == 0, "Console neutral input retained buttons");
    const auto& frame = emulator.frame();
    require(frame.width >= 256 && frame.width <= 512 && frame.height >= 224 && frame.height <= 480 &&
            frame.pixels.size() == size_t(frame.width) * frame.height, "Unexpected console geometry");
    require(std::count_if(frame.pixels.begin(), frame.pixels.end(),
        [](uint32_t pixel) { return (pixel & 0xffffff) != 0; }) > 1000, "Console diagnostic picture is black");
    require(samples > 10000 && audible > 1000, "Missing sustained console tone after boot");
    require(maximum - minimum > 128, "Console tone contains only silence or constant DC");
    require(emulator.sampleRate() >= 22000 && emulator.sampleRate() <= 96000 &&
            emulator.fps() > 59 && emulator.fps() < 61, "Unexpected console timing");
    if (!output.empty()) picture(output / (std::string(systemId(system)) + ".ppm"), frame);
    std::cout << systemId(system) << ": CPU signature, " << frame.width << 'x' << frame.height << ", "
              << audible << '/' << samples << " sustained audible/stereo samples\n";
}
struct Observation {
    uint64_t video;
    std::array<unsigned char, 16> memory;
    bool operator==(const Observation& other) const { return video == other.video && memory == other.memory; }
};
std::vector<Observation> replay(Emulator& emulator, SystemType system, std::string& error) {
    std::vector<Observation> result;
    for (unsigned frame = 0; frame < 60; ++frame) {
        GamepadInput input{}; input.connected = true;
        if (frame >= 8 && frame < 20) input.buttons = 1u << RETRO_DEVICE_ID_JOYPAD_B;
        if (frame >= 24 && frame < 40) { input.start = true; input.faceEast = true; }
        require(emulator.run(input, error), "Replay: " + error);
        result.push_back({videoHash(emulator.frame()), status(system)});
    }
    return result;
}
void inputs(Emulator& emulator, SystemType system, std::string& error) {
    struct Button { unsigned id; unsigned byte; unsigned mask; const char* name; };
    const bool snes = system == SystemType::SuperNintendo;
    const std::array<Button, 12> buttons{{
        {0, 6, 0x80, snes ? "Cross/B" : "Cross/A"},
        {1, 6, 0x40, snes ? "Square/Y" : "Square/B"},
        {2, 6, 0x20, "Select"}, {3, 6, 0x10, "Start"},
        {4, 6, 0x08, "Up"}, {5, 6, 0x04, "Down"},
        {6, 6, 0x02, "Left"}, {7, 6, 0x01, "Right"},
        {8, 7, 0x80, "Circle/A"}, {9, 7, 0x40, "Triangle/X"},
        {10, 7, 0x20, "L1/L"}, {11, 7, 0x10, "R1/R"}
    }};
    GamepadInput neutral{}; neutral.connected = true;
    for (const auto& button : buttons) {
        GamepadInput pressed = neutral;
        if (button.id == 2) pressed.select = true;
        else if (button.id == 3) pressed.start = true;
        else if (button.id == 8) pressed.faceEast = true;
        else if (button.id == 9) pressed.faceNorth = true;
        else pressed.buttons = uint16_t(1u << button.id);
        advance(emulator, pressed, 4, error);
        auto current = status(system);
        const unsigned wanted = !snes && button.byte == 7 ? 0 : button.mask;
        require(current[button.byte] == wanted && current[button.byte == 6 ? 7 : 6] == 0,
                std::string(systemId(system)) + " wrong input " + button.name + ": " +
                std::to_string(current[6]) + "/" + std::to_string(current[7]));
        pressed.connected = false;
        advance(emulator, pressed, 4, error);
        current = status(system);
        require(current[6] == 0 && current[7] == 0, "Disconnected console input remains held");
        advance(emulator, neutral, 2, error);
    }
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "Usage: console_tests fixtures-directory output-directory");
        const fs::path fixtures = fs::absolute(argv[1]), output = fs::absolute(argv[2]);
        fs::create_directories(output);
        const auto pattern = (output / "data-XXXXXX").string();
        std::vector<char> temporary(pattern.begin(), pattern.end()); temporary.push_back(0);
        require(::mkdtemp(temporary.data()) != nullptr, "Cannot create isolated console data");
        const fs::path data(temporary.data());
        fs::create_directories(data / "logs");
        Log log; require(log.open(data.string()), "Cannot open console test log");
        Emulator emulator;
        std::string error;
        GamepadInput neutral{}; neutral.connected = true;
        unsigned consoleIndex = 0;
        for (const auto* fixture : {"diagnostic.nes", "diagnostic-mmc3.nes", "diagnostic.sfc"}) {
            const bool mmc3 = std::strcmp(fixture, "diagnostic-mmc3.nes") == 0;
            const auto system = std::strcmp(fixture, "diagnostic.sfc") == 0 ? SystemType::SuperNintendo :
                SystemType::NintendoEntertainmentSystem;
            const auto rom = fixtures / fixture;
            Game game; require(readRom(rom.string(), game, error) && game.system == system, "Fixture detection: " + error);
            require(emulator.load(rom.string(), data.string(), log, error), "Load: " + error);
            require(emulator.system() == system && emulator.loaded(), "Wrong core selected");
            boot(emulator, system, data, error);
            if (mmc3) {
                const std::array<unsigned char, 7> expected{{0x42,0x51,0x5e,0x49,8,14,0x63}};
                const auto observed = status(system);
                require(std::equal(expected.begin(), expected.end(), observed.begin() + 8),
                        "MMC3 PRG/CHR banking, high bank bits or nametable mirroring failed");
                auto colors = emulator.frame().pixels;
                for (auto& pixel : colors) pixel &= 0xffffff;
                std::sort(colors.begin(), colors.end());
                require(std::unique(colors.begin(), colors.end()) - colors.begin() >= 4,
                        "MMC3 CHR output lost the four-color pattern");
                advance(emulator, neutral, 7, error);
                require(status(system)[15] != observed[15], "MMC3 scanline IRQ counter did not advance");
                picture(data / "nes-mmc3.ppm", emulator.frame());
                std::cout << "MMC3: high PRG/CHR banks, inversion, vertical mirroring and live IRQ passed\n";
            }
            inputs(emulator, system, error);
            const auto marker = static_cast<unsigned char>(0xa0 + consoleIndex++);
            ram(system).data[256] = marker;
            require(emulator.supportsSaveStates(), "Console states unsupported");
            require(!emulator.loadState(error, 4), "Missing state slot accepted");
            std::array<std::array<unsigned char, 16>, 2> slotStatus;
            std::array<std::vector<unsigned char>, 2> containers;
            for (unsigned index = 0; index < 2; ++index) {
                const unsigned slot = index ? 4 : 0;
                advance(emulator, neutral, 11, error);
                slotStatus[index] = status(system);
                require(emulator.saveState(error, slot), "Save state: " + error);
                const auto path = data / "states" / systemId(system) / (game.id + ".slot" + std::to_string(slot) + ".state");
                containers[index] = read(path);
                require(containers[index].size() > 1000, "Empty console state container");
                const auto first = replay(emulator, system, error);
                require(first.back().memory[5] != slotStatus[index][5], "Console CPU frame counter stalled");
                ram(system).data[256] = 0x55;
                require(emulator.loadState(error, slot), "Restore: " + error);
                require(status(system) == slotStatus[index] && ram(system).data[256] == marker,
                        "Console state did not restore active cartridge SRAM");
                require(emulator.frame().pixels.empty() && emulator.audio().empty(), "Restore retained stale frontend AV");
                require(replay(emulator, system, error) == first, "Console state video/RAM replay differs");
                auto corrupted = containers[index]; corrupted.back() ^= 0x20;
                write(path, corrupted);
                const auto before = status(system);
                require(!emulator.loadState(error, slot) && status(system) == before,
                        "Corrupt state accepted or mutated cartridge SRAM");
                write(path, containers[index]);
            }
            require(!emulator.saveState(error, 5) && !emulator.loadState(error, 5), "Invalid slot accepted");
            for (unsigned index = 0; index < 2; ++index) {
                require(emulator.loadState(error, index ? 4 : 0), error);
                require(status(system) == slotStatus[index], "Console numbered slots mixed");
            }
            require(emulator.reset(error), "Reset: " + error);
            require(ram(system).data[256] == marker, "Reset erased battery-backed SRAM");
            boot(emulator, system, {}, error);
            require(ram(system).data[256] == marker, "Reset boot overwrote untouched SRAM");
            emulator.unload();
            const auto battery = data / "saves" / systemId(system) / (game.id + ".srm");
            const auto persisted = read(battery);
            require(persisted.size() > 256 && persisted[256] == marker, "Battery SRAM was not persisted");
            const auto reload = system == SystemType::SuperNintendo ? fixtures / "diagnostic.smc" : rom;
            Game reloaded; require(readRom(reload.string(), reloaded, error), error);
            require(reloaded.id == game.id, "Headered SNES copy changed canonical identity");
            require(emulator.load(reload.string(), data.string(), log, error), "Reload: " + error);
            require(ram(system).data[256] == marker, "Reload did not share persisted battery SRAM");
            boot(emulator, system, {}, error);
            require(emulator.loadState(error), "Slot zero missing after reload: " + error);
            require(status(system) == slotStatus[0] && ram(system).data[256] == marker, "Reloaded state lost SRAM");
            emulator.unload();
            require(!emulator.loaded() && emulator.frame().pixels.empty() && emulator.audio().empty(), "Unload retained console AV");
            // Fresh sessions also compare host-side resampler history. Restoring
            // a core state alone need not restore external audio filter state.
            std::vector<Observation> reference;
            std::vector<int16_t> referencePcm;
            for (unsigned pass = 0; pass < 2; ++pass) {
                const auto speedData = data / (std::string(fixture) + "-speed-" + std::to_string(pass));
                require(emulator.load(rom.string(), speedData.string(), log, error), "Speed comparison: " + error);
                std::vector<Observation> observed;
                std::vector<int16_t> releasedPcm;
                for (unsigned frame = 0; frame < 480; ++frame) {
                    GamepadInput pad = neutral;
                    pad.fastForward = pass && frame >= 360 && frame < 420;
                    if (frame >= 380 && frame < 400) { pad.buttons = 1; pad.faceNorth = true; }
                    require(emulator.run(pad, error), "Speed run: " + error);
                    if (frame >= 360) observed.push_back({videoHash(emulator.frame()), status(system)});
                    if (pad.fastForward) require(emulator.audio().empty(), "Fast-forward copied muted PCM");
                    if (frame >= 420) releasedPcm.insert(releasedPcm.end(), emulator.audio().begin(), emulator.audio().end());
                }
                emulator.unload();
                if (!pass) { reference = std::move(observed); referencePcm = std::move(releasedPcm); }
                else require(observed == reference && !releasedPcm.empty() && releasedPcm == referencePcm,
                             std::string(systemId(system)) + " fast-forward changed video/RAM or resumed PCM");
            }
            std::cout << "PASS " << fixture << ": buttons/disconnect, slots0/4 replay, corrupt rejection, "
                         "reset/SRAM/reload, 120 video/RAM observations + resumed PCM identical\n";
        }
        std::cout << "Console evidence: " << data << '\n';
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: " << exception.what() << '\n';
        return 1;
    }
}
