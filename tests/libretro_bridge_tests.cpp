// Exercise frontend contracts independently of any commercial cartridge.
#include "core/libretro_core.h"
#include "log.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <unistd.h>
namespace fs = std::filesystem;
using namespace r2n64;
namespace {
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
retro_environment_t env;
retro_video_refresh_t video;
retro_audio_sample_batch_t audio;
retro_input_state_t input;
retro_log_printf_t coreLogger;
retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
std::array<unsigned char, 16> sram{};
std::array<unsigned char, 4> rtc{};
std::array<unsigned char, 64> pixels{};
unsigned width = 2, height = 2;
size_t pitch = 16;
bool active = false, rejectLoad = false;
bool dmaDuringLoad = false, dmaDuringDeinit = false;
unsigned rejectStates = 0, unserializes = 0, runs = 0, resets = 0, deinits = 0;
unsigned optionUpdates = 0;
bool observedFastForward = false;
int observedAvEnable = 0;
std::string observedPalette;
uint32_t counter = 0;
uint16_t observedInput = 0;
const char* version = "test-1";
void emitDma(retro_log_level level, unsigned channel) {
    char message[128];
    std::snprintf(message, sizeof(message), "Starting DMA %u 0x08000248 -> 0x03003580 (8400:0200)", channel);
    coreLogger(level, "%s: %s\n", "GBA DMA", message);
}
void init() {
    env(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &format);
    retro_log_callback callback{};
    require(env(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &callback) && callback.log, "Core logger unavailable");
    coreLogger = callback.log;
}
void deinit() { ++deinits; if (dmaDuringDeinit) emitDma(RETRO_LOG_INFO, 1); }
unsigned apiVersion() { return RETRO_API_VERSION; }
void info(retro_system_info* value) { *value = {"Synthetic", version, "gb|gbc", false, false}; }
void av(retro_system_av_info* value) { *value = {{2, 2, 8, 8, 1}, {59.7275, 48000}}; }
void setEnv(retro_environment_t cb) { env = cb; }
void setVideo(retro_video_refresh_t cb) { video = cb; }
void setAudio(retro_audio_sample_t) {}
void setAudioBatch(retro_audio_sample_batch_t cb) { audio = cb; }
void setPoll(retro_input_poll_t) {}
void setInput(retro_input_state_t cb) { input = cb; }
void controller(unsigned, unsigned) {}
void resetCore() {
    counter = 0; ++resets;
    // Simulate reset reviving an older savedata backing buffer.
    sram.fill(0xDD); rtc.fill(0xCC);
}
void run() {
    require(env(RETRO_ENVIRONMENT_GET_FASTFORWARDING, &observedFastForward), "Missing fast-forward callback");
    require(env(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &observedAvEnable), "Missing AV-enable callback");
    bool updated = false;
    require(env(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated), "Missing variable update callback");
    if (updated) {
        ++optionUpdates;
        retro_variable variable{"sameboy_mono_palette", nullptr};
        require(env(RETRO_ENVIRONMENT_GET_VARIABLE, &variable) && variable.value, "Updated palette missing");
        observedPalette = variable.value;
    }
    ++runs;
    ++counter;
    sram[0] = static_cast<unsigned char>(counter);
    observedInput = static_cast<uint16_t>(input(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK));
    video(pixels.data(), width, height, pitch);
    const int16_t samples[] = {100, -200, 300, -400};
    audio(samples, 2);
}
bool load(const retro_game_info* game) {
    require(game && game->data && game->size == 32768 && !game->path, "Core did not receive in-memory content");
    if (dmaDuringLoad) emitDma(RETRO_LOG_INFO, 2);
    active = !rejectLoad;
    sram.fill(0);
    rtc.fill(0);
    counter = 0;
    retro_variable variable{"sameboy_mono_palette", nullptr};
    if (env(RETRO_ENVIRONMENT_GET_VARIABLE, &variable) && variable.value) observedPalette = variable.value;
    return active;
}
void unload() { active = false; sram.fill(0xEE); rtc.fill(0xEE); }
size_t serializeSize() { return 32; }
bool serialize(void* out, size_t size) {
    if (size != 32) return false;
    std::memset(out, 0, size);
    std::memcpy(out, &counter, sizeof(counter));
    return true;
}
bool unserialize(const void* bytes, size_t size) {
    ++unserializes;
    if (rejectStates) { --rejectStates; counter = 0xBAD; return false; }
    if (size != 32) return false;
    std::memcpy(&counter, bytes, sizeof(counter));
    return true;
}
void* memory(unsigned id) { return !active ? nullptr : id == RETRO_MEMORY_SAVE_RAM ? sram.data() : id == RETRO_MEMORY_RTC ? rtc.data() : nullptr; }
size_t memorySize(unsigned id) { return !active ? 0 : id == RETRO_MEMORY_SAVE_RAM ? sram.size() : id == RETRO_MEMORY_RTC ? rtc.size() : 0; }
const CoreApi api{init, deinit, apiVersion, info, av, setEnv, setVideo, setAudio, setAudioBatch,
                  setPoll, setInput, controller, resetCore, run, load, unload, serializeSize,
                  serialize, unserialize, memory, memorySize};
const CoreDescriptor descriptor{"synthetic", "Synthetic", {SystemType::GameBoy, SystemType::GameBoyColor}, "gb|gbc", true};
std::vector<unsigned char> read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(file)), {});
}
void write(const fs::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    require(bool(file), "Cannot write synthetic test file");
}
fs::path findExtension(const fs::path& dir, const char* extension) {
    for (const auto& entry : fs::directory_iterator(dir)) if (entry.path().extension() == extension) return entry.path();
    throw std::runtime_error("Missing expected output file");
}
void sourcePixels(retro_pixel_format selected) {
    format = selected;
    pixels.fill(0xAB);
    const uint32_t colors32[] = {0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFFFF};
    const uint16_t colors565[] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    const uint16_t colors1555[] = {0x7C00, 0x03E0, 0x001F, 0x7FFF};
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned bytes = selected == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2;
        const void* value = selected == RETRO_PIXEL_FORMAT_XRGB8888 ? static_cast<const void*>(&colors32[i]) :
            selected == RETRO_PIXEL_FORMAT_RGB565 ? static_cast<const void*>(&colors565[i]) : static_cast<const void*>(&colors1555[i]);
        std::memcpy(pixels.data() + (i / 2) * pitch + (i % 2) * bytes, value, bytes);
    }
}
std::string logText(const fs::path& root) {
    const auto bytes = read(root / "logs/r2n64.log");
    return std::string(bytes.begin(), bytes.end());
}
void testRoutineDmaLogs(const fs::path& root, const fs::path& gbPath, Log& log) {
    std::string error;
    std::vector<unsigned char> rom(32768);
    rom[0xB2] = 0x96;
    unsigned checksum = 0x19;
    for (size_t i = 0xA0; i <= 0xBC; ++i) checksum += rom[i];
    rom[0xBD] = static_cast<unsigned char>(-checksum);
    const auto gbaPath = root / "synthetic.gba";
    write(gbaPath, rom);
    const CoreDescriptor mgba{"mgba", "Synthetic mGBA", {SystemType::GameBoyAdvance, SystemType::Unknown}, "gba", true};
    LibretroCore core(mgba, api);
    require(core.load(gbaPath.string(), root.string(), log, error, {}), error);
    auto before = logText(root);
    for (unsigned i = 0; i < 10000; ++i) emitDma(RETRO_LOG_INFO, 3);
    require(logText(root) == before, "Routine DMA messages performed log I/O during execution");
    require(core.reset(error), error);
    emitDma(RETRO_LOG_INFO, 0);
    require(logText(root) == before, "Reset leaked routine DMA logs or flushed the active session");
    emitDma(RETRO_LOG_WARN, 3);
    emitDma(RETRO_LOG_ERROR, 3);
    const std::array<const char*, 6> keep{{
        "GBA DMA: Starting DMA 3 0x08000248 -> 0x03003580 (8400:0200) diagnostic suffix",
        "GBA DMA: Starting DMA 4 0x08000248 -> 0x03003580 (8400:0200)",
        "GBA DMA: Starting DMA 3 0x08000a48 -> 0x03003580 (8400:0200)",
        "GBA DMA: Starting DMA failed",
        "GBA Video: Starting DMA 3 0x08000248 -> 0x03003580 (8400:0200)",
        "GBA DMA: other informative diagnostic"
    }};
    for (const char* message : keep) coreLogger(RETRO_LOG_INFO, "%s\n", message);
    auto recorded = logText(root).substr(before.size());
    require(recorded.find("[WARN] GBA DMA: Starting DMA 3 0x08000248 -> 0x03003580 (8400:0200)\n") != std::string::npos &&
            recorded.find("[ERROR] GBA DMA: Starting DMA 3 0x08000248 -> 0x03003580 (8400:0200)\n") != std::string::npos,
            "Routine-message matching hid or changed warning/error content");
    for (const char* message : keep)
        require(recorded.find(std::string("[CORE] ") + message + "\n") != std::string::npos,
                "DMA aggregation hid an unrelated or unrecognized INFO record");
    dmaDuringDeinit = true;
    core.unload();
    dmaDuringDeinit = false;
    recorded = logText(root).substr(before.size());
    const std::string summary = "mGBA: routine DMA start messages aggregated; channel 0=1, 1=1, 2=0, 3=10000";
    require(recorded.find(summary) != std::string::npos && recorded.find(summary, recorded.find(summary) + 1) == std::string::npos,
            "Session DMA summary lost counts across reset/deinit or emitted twice");
    before = logText(root);
    core.unload();
    require(logText(root) == before, "Repeated unload repeated the DMA summary");
    require(core.load(gbaPath.string(), root.string(), log, error, {}), error);
    core.unload();
    require(logText(root).substr(before.size()).find("aggregated") == std::string::npos,
            "Clean new session inherited old DMA counters");
    before = logText(root);
    dmaDuringLoad = rejectLoad = true;
    require(!core.load(gbaPath.string(), root.string(), log, error, {}), "Synthetic rejected load succeeded");
    dmaDuringLoad = rejectLoad = false;
    require(logText(root).substr(before.size()).find("channel 0=0, 1=0, 2=1, 3=0") != std::string::npos,
            "Rejected load did not report its own DMA counts");
    before = logText(root);
    require(core.load(gbaPath.string(), root.string(), log, error, {}), error);
    core.unload();
    require(logText(root).substr(before.size()).find("aggregated") == std::string::npos,
            "Successful load inherited failed-load DMA counters");

    // The same diagnostic belongs to its issuing core, not its text alone.
    const CoreDescriptor sameboy{"sameboy", "Synthetic SameBoy", {SystemType::GameBoy, SystemType::GameBoyColor}, "gb|gbc", true};
    const CoreDescriptor n64{"mupen64plus_next", "Synthetic N64", {SystemType::Nintendo64, SystemType::Unknown}, "z64", true};
    const CoreDescriptor otherGba{"other_gba", "Synthetic other GBA", {SystemType::GameBoyAdvance, SystemType::Unknown}, "gba", true};
    rom.assign(32768, 0);
    rom[0] = 0x80; rom[1] = 0x37; rom[2] = 0x12; rom[3] = 0x40;
    const auto n64Path = root / "synthetic.z64";
    write(n64Path, rom);
    for (const auto& test : std::array<std::pair<const CoreDescriptor*, fs::path>, 3>{{
            {&sameboy, gbPath}, {&n64, n64Path}, {&otherGba, gbaPath}}}) {
        LibretroCore other(*test.first, api);
        require(other.load(test.second.string(), root.string(), log, error, {}), error);
        before = logText(root);
        emitDma(RETRO_LOG_INFO, 3);
        other.unload();
        recorded = logText(root).substr(before.size());
        require(recorded.find("[CORE] GBA DMA: Starting DMA 3 0x08000248 -> 0x03003580 (8400:0200)\n") != std::string::npos &&
                recorded.find("aggregated") == std::string::npos,
                "DMA aggregation affected SameBoy, N64 or an unrelated GBA core");
    }
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Usage: libretro_bridge_tests output-directory");
        const auto root = fs::absolute(argv[1]) / ("bridge-" + std::to_string(getpid()));
        fs::create_directories(root / "logs");
        Log log;
        require(log.open(root.string()), "Cannot open log");
        std::vector<unsigned char> rom(32768);
        rom[0x147] = 3; rom[0x149] = 2;
        for (unsigned i = 0x134; i <= 0x14C; ++i) rom[0x14D] = static_cast<unsigned char>(rom[0x14D] - rom[i] - 1);
        const auto path = root / "synthetic.gb";
        write(path, rom);
        LibretroCore core(descriptor, api), competitor(descriptor, api);
        GamepadInput pad{};
        pad.connected = true;
        std::string error;
        for (const auto pixelFormat : {RETRO_PIXEL_FORMAT_XRGB8888, RETRO_PIXEL_FORMAT_RGB565, RETRO_PIXEL_FORMAT_0RGB1555}) {
            sourcePixels(pixelFormat);
            require(core.load(path.string(), root.string(), log, error, {}), error);
            require(!competitor.load(path.string(), root.string(), log, error, {}), "Concurrent core session accepted");
            require(!core.supportsSaveStates(), "Unstarted core advertised save states");
            require(core.run(pad, error), error);
            const std::vector<uint32_t> expected{0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0xFFFFFFFF};
            require(core.frame().width == 2 && core.frame().height == 2 && core.frame().pixels == expected,
                    "Pixel conversion or row pitch changed colors");
            require(core.audio() == std::vector<int16_t>({100, -200, 300, -400}) && core.sampleRate() == 48000,
                    "Stereo PCM or native sample rate changed");
            require(!observedFastForward && observedAvEnable == 3, "Normal portable output flags are incorrect");
            pad.fastForward = true;
            const auto beforeSpeed = counter;
            require(core.run(pad, error) && counter == beforeSpeed + 1 && core.frame().pixels == expected,
                    "Portable fast-forward changed frame conversion or stopped cartridge execution");
            require(observedFastForward && observedAvEnable == 1 && core.audio().empty(),
                    "Portable fast-forward copied muted PCM or reported normal output flags");
            pad.connected = false;
            require(core.run(pad, error) && !observedFastForward && observedAvEnable == 3 && !core.audio().empty(),
                    "Disconnect retained fast-forward audio suppression");
            pad.connected = true; pad.fastForward = false;
            require(core.run(pad, error) && core.audio() == std::vector<int16_t>({100, -200, 300, -400}) &&
                    !observedFastForward && observedAvEnable == 3,
                    "Fast-forward release lost audio or retained muted samples");
            core.unload();
            require(core.frame().pixels.empty() && core.audio().empty(), "Unload retained stale output");
        }
        sourcePixels(RETRO_PIXEL_FORMAT_RGB565);
        require(core.load(path.string(), root.string(), log, error, {}), error);
        require(observedPalette == "greyscale", "Default palette differs from previous SameBoy default");
        const auto beforePalette = counter;
        require(core.setGameBoyPalette(1, error) && counter == beforePalette && resets == 0,
                "Palette setter reset or executed the cartridge");
        require(core.run(pad, error) && observedPalette == "lime" && optionUpdates == 1,
                "Native palette did not update on the next core run");
        require(core.run(pad, error) && optionUpdates == 1, "Variable update repeated after consumption");
        require(core.setGameBoyPalette(1, error) && core.run(pad, error) && optionUpdates == 1,
                "No-op palette change signaled another update");
        require(!core.setGameBoyPalette(GameBoyPaletteCount, error) &&
                !core.setGameBoyPalette(std::numeric_limits<unsigned>::max(), error), "Invalid palette accepted");
        pad.buttons = (1u << 0) | (1u << 1) | (1u << 4) | (1u << 10) | (1u << 11);
        pad.start = pad.select = true;
        require(core.run(pad, error), error);
        require(observedInput == ((1u << 8) | (1u << 0) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 10) | (1u << 11)),
                "Portable A/B/Start/Select/D-pad/shoulder mapping wrong");
        pad.connected = false;
        require(core.run(pad, error) && observedInput == 0, "Disconnected portable input retained buttons");
        pad.connected = true;
        require(core.saveState(error), error);
        const auto statePath = findExtension(root / "states/gb", ".state");
        const auto original = read(statePath);
        const auto saved = counter;
        const auto base = statePath.string().substr(0, statePath.string().rfind(".slot0.state"));
        std::array<uint32_t, SaveStateSlotCount> slotCounters{};
        slotCounters[0] = counter;
        for (unsigned slot = 1; slot < SaveStateSlotCount; ++slot) {
            require(core.run(pad, error), error);
            slotCounters[slot] = counter;
            require(core.saveState(error, slot), error);
            require(fs::exists(base + ".slot" + std::to_string(slot) + ".state"), "Slot filename is incorrect");
        }
        require(read(statePath) == original, "Saving another slot changed legacy slot zero");
        for (unsigned slot = 0; slot < SaveStateSlotCount; ++slot)
            require(core.loadState(error, slot) && counter == slotCounters[slot], "Slots did not restore distinct CPU snapshots");
        require(!core.saveState(error, SaveStateSlotCount) && !core.loadState(error, SaveStateSlotCount) &&
                !core.saveState(error, std::numeric_limits<unsigned>::max()) &&
                !core.loadState(error, std::numeric_limits<unsigned>::max()), "Out-of-range slot accepted");
        require(!fs::exists(base + ".slot5.state"), "Invalid slot created a file");
        const auto slot4Path = base + ".slot4.state";
        auto corruptSlot = read(slot4Path); corruptSlot.back() ^= 1;
        write(slot4Path, corruptSlot);
        const auto beforeCorruptSlot = counter;
        require(!core.loadState(error, 4) && counter == beforeCorruptSlot, "Corrupted selected slot mutated CPU");
        require(core.loadState(error) && counter == saved, "Corrupt slot affected valid slot zero");
        require(core.run(pad, error) && counter != saved, "Counter failed to advance");
        require(core.loadState(error) && counter == saved, "State did not restore fake CPU");
        require(core.frame().pixels.empty() && core.audio().empty(), "State restore retained stale output");
        for (const size_t offset : {0, 8, 12, 16, 24, 32, 64, 128, 192}) {
            auto invalid = original; invalid[offset] ^= 1;
            write(statePath, invalid);
            const auto before = unserializes;
            require(!core.loadState(error) && unserializes == before && counter == saved,
                    "Invalid state reached core or changed active CPU");
        }
        write(statePath, std::vector<unsigned char>(original.begin(), original.end() - 1));
        require(!core.loadState(error), "Truncated state accepted");
        write(statePath, original);
        require(core.run(pad, error), error);
        const auto before = counter;
        rejectStates = 1;
        require(!core.loadState(error) && counter == before, "Rejected core state did not roll back");
        const auto beforeResetRam = sram;
        const auto beforeResetRtc = rtc;
        require(core.reset(error) && resets == 1 && counter == 0, "Portable reset failed");
        require(sram == beforeResetRam && rtc == beforeResetRtc, "Reset discarded active cartridge memory");
        require(core.run(pad, error), error);
        sram[5] = 0x5A; rtc[0] = 0x7B;
        core.unload();
        require(read(findExtension(root / "saves/gb", ".srm"))[5] == 0x5A, "SRAM saved after core freed it");
        require(read(findExtension(root / "saves/gb", ".rtc"))[0] == 0x7B, "RTC lost on unload");
        require(core.load(path.string(), root.string(), log, error, {}) && sram[5] == 0x5A && rtc[0] == 0x7B,
                "Native SRAM/RTC did not restore");
        require(core.run(pad, error), error);
        rejectStates = 2;
        require(!core.loadState(error) && !core.run(pad, error), "Failed rollback did not stop unsafe session");
        core.unload();
        require(read(findExtension(root / "saves/gb", ".srm"))[5] == 0x5A, "Failed rollback overwrote native save");
        rejectLoad = true;
        const auto deinitBefore = deinits;
        require(!core.load(path.string(), root.string(), log, error, {}) && deinits == deinitBefore + 1, "Failed load leaked initialization");
        rejectLoad = false;
        version = "test-2";
        require(core.load(path.string(), root.string(), log, error, {}), error);
        require(core.run(pad, error) && !core.loadState(error), "State from another core version accepted");
        width = 2049;
        const auto beforeRuns = runs;
        require(!core.run(pad, error) && runs == beforeRuns + 1, "Invalid frame dimensions accepted");
        require(!core.run(pad, error) && runs == beforeRuns + 1, "Failed callback did not stop core execution");
        core.unload();
        width = 2;
        for (const bool muted : {false, true}) {
            require(core.load(path.string(), root.string(), log, error, {}), error);
            pad.fastForward = muted;
            require(core.run(pad, error), error);
            // Muted output must still enforce the accumulated per-run callback
            // limit. Every individual chunk fits; their sum exceeds the cap.
            std::vector<int16_t> samples(44100, 1);
            require(audio(samples.data(), 22050) == 22050, "Audio batch was not consumed");
            require(audio(samples.data(), 22050) == 22050, "Audio batch was not consumed");
            require(!core.run(pad, error), "Accumulated audio overflow was accepted");
            if (muted) require(core.audio().empty(), "Muted overflow allocated or retained PCM");
            core.unload();
        }
        testRoutineDmaLogs(root, path, log);
        std::cout << "PASS: dynamic pixel formats/pitch, portable input/audio, muted fast-forward/resume, callback bounds, lifecycle, SRAM/RTC, state validation/rollback/reset, exact mGBA INFO DMA aggregation and diagnostic preservation\n";
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
