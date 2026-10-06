// Original synthetic metadata/CPU loop; no commercial ROM or firmware data.
#include "../../external/bsnes-mercury/target-libretro/libretro.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
void bsnes_mercury_retro_set_environment(retro_environment_t);
void bsnes_mercury_retro_set_video_refresh(retro_video_refresh_t);
void bsnes_mercury_retro_set_audio_sample_batch(retro_audio_sample_batch_t);
void bsnes_mercury_retro_set_input_poll(retro_input_poll_t);
void bsnes_mercury_retro_set_input_state(retro_input_state_t);
void bsnes_mercury_retro_init();
void bsnes_mercury_retro_deinit();
bool bsnes_mercury_retro_load_game(const retro_game_info*);
void bsnes_mercury_retro_unload_game();
void bsnes_mercury_retro_run();
void bsnes_mercury_retro_reset();
size_t bsnes_mercury_retro_serialize_size();
bool bsnes_mercury_retro_serialize(void*, size_t);
bool bsnes_mercury_retro_unserialize(const void*, size_t);
void* bsnes_mercury_retro_get_memory_data(unsigned);
size_t bsnes_mercury_retro_get_memory_size(unsigned);
}
static std::string systemDirectory, errors;
static void logger(retro_log_level level, const char* format, ...) {
    char buffer[4096]; va_list args; va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
    if (level >= RETRO_LOG_ERROR) errors += buffer;
}
static bool environment(unsigned command, void* data) {
    switch (command) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(data) = systemDirectory.c_str(); return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback*>(data)->log = logger; return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto& option = *static_cast<retro_variable*>(data);
        if (!strcmp(option.key, "bsnes_violate_accuracy")) option.value = "enabled";
        else if (!strcmp(option.key, "bsnes_chip_hle")) option.value = "HLE";
        else return false;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(data) = false; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: return true;
    default: return false;
    }
}
static void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
static void run(unsigned frames) { while (frames--) bsnes_mercury_retro_run(); }
static std::vector<uint8_t> wram() {
    const size_t size = bsnes_mercury_retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    const auto* data = static_cast<uint8_t*>(bsnes_mercury_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
    require(data && size == 128 * 1024, "SNES WRAM unavailable");
    return {data, data + size};
}
static std::string manifest(const std::string& chip = "") {
    std::string text = "cartridge region=NTSC\n"
        "  rom name=program.rom size=0x8000\n"
        "  map id=rom address=00-7d,80-ff:8000-ffff mask=0x8000\n";
    if (chip.empty()) return text;
    const bool st = chip.compare(0, 2, "st") == 0;
    text += std::string("  necdsp model=") + (st ? "uPD96050" : "uPD7725") + " frequency=11000000\n"
        "    rom id=program name=" + chip + ".program.rom size=" + (st ? "0xc000" : "0x1800") + "\n"
        "    rom id=data name=" + chip + ".data.rom size=" + (st ? "0x1000" : "0x800") + "\n"
        "    map id=io address=68:0000-0fff select=0x1000\n";
    return text;
}
int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    try {
        require(argc == 2, "Output directory required");
        fs::create_directories(argv[1]);
        systemDirectory = (fs::absolute(argv[1]) / "snes-chips-XXXXXX").string();
        require(mkdtemp(systemDirectory.data()) != nullptr, "Cannot isolate system directory");
        std::vector<uint8_t> rom(32768, 0xea);
        // Increment a 24-bit ST0010 RAM counter and mirror it to WRAM.
        std::vector<uint8_t> program{0x78, 0xd8, 0x18}; // SEI; CLD; CLC
        for (uint8_t byte = 0; byte < 3; ++byte) {
            const uint8_t instructions[] = {0xaf, byte, 0, 0x68, 0x69, uint8_t(byte ? 0 : 1),
                0x8f, byte, 0, 0x68, 0x8f, byte, 0, 0x7e};
            program.insert(program.end(), std::begin(instructions), std::end(instructions));
        }
        program.push_back(0x80); program.push_back(static_cast<uint8_t>(2 - (program.size() + 1)));
        std::copy(program.begin(), program.end(), rom.begin());
        rom[0x7ffc] = 0; rom[0x7ffd] = 0x80;
        bsnes_mercury_retro_set_environment(environment);
        bsnes_mercury_retro_set_video_refresh([](const void*, unsigned, unsigned, size_t) {});
        bsnes_mercury_retro_set_audio_sample_batch([](const int16_t*, size_t frames) { return frames; });
        bsnes_mercury_retro_set_input_poll([] {});
        bsnes_mercury_retro_set_input_state([](unsigned, unsigned, unsigned, unsigned) -> int16_t { return 0; });
        bsnes_mercury_retro_init();
        auto load = [&](const std::string& chip) {
            const std::string markup = manifest(chip);
            const retro_game_info info{nullptr, rom.data(), rom.size(), markup.c_str()};
            errors.clear(); return bsnes_mercury_retro_load_game(&info);
        };
        require(load(""), "Normal cartridge failed");
        const size_t plainSize = bsnes_mercury_retro_serialize_size();
        require(plainSize > 0, "Normal cartridge states disabled");
        bsnes_mercury_retro_unload_game();
        for (const char* chip : {"dsp1", "dsp2", "dsp3", "dsp4"}) {
            require(load(chip), "Known DSP HLE load failed");
            require(errors.empty(), "Known DSP HLE unexpectedly requested firmware");
            require(bsnes_mercury_retro_serialize_size() == 0, "Incomplete DSP state offered");
            uint8_t byte = 0;
            require(!bsnes_mercury_retro_serialize(&byte, 1), "Incomplete DSP state saved");
            require(!bsnes_mercury_retro_unserialize(&byte, 1), "Incomplete DSP state restored");
            bsnes_mercury_retro_unload_game();
        }
        require(!load("st011"), "ST0011 incorrectly accepted ST0010 HLE");
        require(errors.find("st011.program.rom") != std::string::npos, "Missing ST0011 firmware not reported");
        bsnes_mercury_retro_unload_game();
        require(load("st010"), "ST0010 did not recover after missing ST0011 firmware");
        require(errors.empty(), "ST0010 HLE unexpectedly requested firmware");
        require(bsnes_mercury_retro_serialize_size() == plainSize + 4096, "ST0010 RAM absent from state");
        run(2);
        auto memory = wram(); require(memory[0] || memory[1] || memory[2], "ST0010 synthetic counter did not execute");
        std::vector<uint8_t> saved(bsnes_mercury_retro_serialize_size());
        require(bsnes_mercury_retro_serialize(saved.data(), saved.size()), "ST0010 state save failed");
        run(3); const auto expected = wram();
        require(bsnes_mercury_retro_unserialize(saved.data(), saved.size()), "ST0010 state load failed");
        run(3); require(wram() == expected, "ST0010 state replay diverged");
        bsnes_mercury_retro_reset();
        run(2); require(wram() == memory, "ST0010 reset retained prior chip RAM");
        bsnes_mercury_retro_unload_game();
        require(load(""), "Normal cartridge failed after chip session");
        require(bsnes_mercury_retro_serialize_size() == plainSize, "Chip state leaked into normal cartridge");
        bsnes_mercury_retro_unload_game(); bsnes_mercury_retro_deinit();
        fs::remove(systemDirectory);
        std::puts("PASS: SNES HLE dispatch, missing firmware recovery, DSP states blocked, ST0010 state/reset/replay");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n%s", error.what(), errors.c_str()); return 1;
    }
}
