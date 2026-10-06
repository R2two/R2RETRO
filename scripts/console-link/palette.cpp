// Original malformed/valid palette regression against the actual static core.
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>
extern "C" {
void fceumm_retro_set_environment(bool (*)(unsigned, void*));
void fceumm_retro_init();
void fceumm_retro_deinit();
void fceumm_FCEUI_SetBaseDirectory(const char*);
void fceumm_FCEU_LoadGamePalette();
extern uint8_t fceumm_palette_game_available;
int64_t __real_fceumm_filestream_read(void*, void*, int64_t);
}
static bool shortRead = false;
extern "C" int64_t __wrap_fceumm_filestream_read(void* stream, void* bytes, int64_t size) {
    if (shortRead) return 0;
    return __real_fceumm_filestream_read(stream, bytes, size);
}
static bool environment(unsigned, void*) { return false; }
int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    try {
        if (argc != 2) throw std::runtime_error("Output directory required");
        fs::create_directories(argv[1]);
        std::string pattern = (fs::absolute(argv[1]) / "palette-XXXXXX").string();
        if (!mkdtemp(pattern.data())) throw std::runtime_error("Cannot create isolated data");
        const fs::path path = fs::path(pattern) / "nes.pal";
        fceumm_retro_set_environment(environment);
        fceumm_retro_init();
        fceumm_FCEUI_SetBaseDirectory(pattern.c_str());
        for (size_t size : {192, 1536, 0, 191, 193, 1535, 1537, 3072, 65536}) {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            const std::vector<char> bytes(size, '\x40');
            output.write(bytes.data(), bytes.size());
            output.close();
            if (!output) throw std::runtime_error("Palette write failed");
            fceumm_FCEU_LoadGamePalette();
            if (bool(fceumm_palette_game_available) != (size == 192 || size == 1536))
                throw std::runtime_error("Palette length accepted/rejected incorrectly: " + std::to_string(size));
        }
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        const std::vector<char> bytes(192, '\x60');
        output.write(bytes.data(), bytes.size()); output.close();
        shortRead = true;
        fceumm_FCEU_LoadGamePalette();
        if (fceumm_palette_game_available) throw std::runtime_error("Short palette read accepted");
        shortRead = false;
        fceumm_FCEU_LoadGamePalette();
        if (!fceumm_palette_game_available) throw std::runtime_error("Valid palette did not recover after short read");
        fs::remove(path);
        fceumm_FCEU_LoadGamePalette();
        if (fceumm_palette_game_available) throw std::runtime_error("Missing palette retained stale data");
        fs::remove(pattern);
        fceumm_retro_deinit();
        std::puts("PASS: NES palette bounds, 64/512 colors, missing/short files and recovery");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
