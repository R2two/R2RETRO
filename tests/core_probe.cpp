// Isolated desktop integration test for the real upstream libretro core.
// This is not a PS4 frontend and is never included in its PKG.
#include <libretro.h>
#include <SDL.h>
#include <SDL_image.h>
#include <dlfcn.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>

namespace {
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
std::string systemDirectory;
std::map<std::string, std::string> options;
std::vector<uint32_t> pixels;
unsigned width{}, height{}, videoFrames{}, duplicateFrames{}, inputPolls{}, hardwareRequests{};
unsigned inputReads{}, risingEdges{};
size_t audioFrames{}, nonzeroAudioFrames{}, unequalStereoFrames{};
int audioPeak{}, audioSign{};
uint16_t joypadMask{};
int16_t analogX{}, analogY{};
bool invalidVideo{};

void coreLog(retro_log_level level, const char* format, ...) {
    std::fprintf(stderr, "[core:%u] ", static_cast<unsigned>(level));
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
}

bool acceptOptions(const retro_core_options_v2* opts) {
    if (!opts || !opts->definitions) return false;
    for (auto def = opts->definitions; def->key; ++def) {
        const char* value = def->default_value ? def->default_value : def->values[0].value;
        if (value) options.emplace(def->key, value);
    }
    return true;
}

bool environment(unsigned cmd, void* data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char**>(data) = systemDirectory.c_str(); return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback*>(data)->log = coreLog; return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *static_cast<unsigned*>(data) = 2; return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
        return acceptOptions(static_cast<retro_core_options_v2*>(data));
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
        return acceptOptions(static_cast<retro_core_options_v2_intl*>(data)->us);
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto* variable = static_cast<retro_variable*>(data);
        auto it = options.find(variable->key);
        variable->value = it == options.end() ? nullptr : it->second.c_str();
        return variable->value != nullptr;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool*>(data) = false; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return *static_cast<retro_pixel_format*>(data) == RETRO_PIXEL_FORMAT_XRGB8888;
    case RETRO_ENVIRONMENT_SET_HW_RENDER:
        ++hardwareRequests; return false;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool*>(data) = true; return true;
    case RETRO_ENVIRONMENT_GET_JIT_CAPABLE:
    case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
        *static_cast<bool*>(data) = false; return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        *static_cast<int*>(data) = 3; return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
        *static_cast<unsigned*>(data) = RETRO_LANGUAGE_ENGLISH; return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        return true;
    default: return false;
    }
}

void video(const void* data, unsigned w, unsigned h, size_t pitch) {
    if (!data) { ++duplicateFrames; return; }
    if (data == RETRO_HW_FRAME_BUFFER_VALID || w == 0 || h == 0 ||
        w > 2048 || h > 2048 || pitch < w * 4 || pitch > 2048 * 4) {
        invalidVideo = true;
        return;
    }
    width = w; height = h;
    pixels.resize(static_cast<size_t>(w) * h);
    for (unsigned y = 0; y < h; ++y)
        std::memcpy(pixels.data() + y * w, static_cast<const char*>(data) + y * pitch, w * 4);
    ++videoFrames;
}
void poll() { ++inputPolls; }
int16_t input(unsigned port, unsigned device, unsigned index, unsigned id) {
    ++inputReads;
    if (port != 0) return 0;
    if (device == RETRO_DEVICE_JOYPAD) {
        if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return static_cast<int16_t>(joypadMask);
        if (id < 16) return (joypadMask >> id) & 1;
    }
    if (device == RETRO_DEVICE_ANALOG && index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
        return id == RETRO_DEVICE_ID_ANALOG_X ? analogX : analogY;
    return 0;
}
void audio(int16_t left, int16_t right) {
    ++audioFrames;
    nonzeroAudioFrames += left != 0 || right != 0;
    unequalStereoFrames += left != right;
    audioPeak = std::max({audioPeak, std::abs(static_cast<int>(left)), std::abs(static_cast<int>(right))});
    // Hysteresis ignores tiny resampling ripples at silence/zero crossings.
    if (left > 512) {
        risingEdges += audioSign < 0;
        audioSign = 1;
    } else if (left < -512) audioSign = -1;
}
size_t audioBatch(const int16_t* data, size_t frames) {
    for (size_t i = 0; i < frames; ++i) audio(data[i * 2], data[i * 2 + 1]);
    return frames;
}

template<typename T> T symbol(void* library, const char* name) {
    auto result = reinterpret_cast<T>(dlsym(library, name));
    require(result != nullptr, std::string("Missing libretro symbol: ") + name);
    return result;
}

struct Core {
    void* library{};
    bool initialized{}, loaded{};
#define API(name) decltype(&retro_##name) name{}
    API(set_environment); API(set_video_refresh); API(set_audio_sample);
    API(set_audio_sample_batch); API(set_input_poll); API(set_input_state);
    API(api_version); API(get_system_info); API(get_system_av_info); API(init); API(deinit);
    API(load_game); API(unload_game); API(run); API(get_memory_data);
    API(get_memory_size); API(serialize_size); API(serialize); API(unserialize);
#undef API
    explicit Core(const char* path) {
        library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!library) throw std::runtime_error(dlerror());
        try {
#define LOAD(name) name = symbol<decltype(name)>(library, "retro_" #name)
            LOAD(set_environment); LOAD(set_video_refresh); LOAD(set_audio_sample);
            LOAD(set_audio_sample_batch); LOAD(set_input_poll); LOAD(set_input_state);
            LOAD(api_version); LOAD(get_system_info); LOAD(get_system_av_info); LOAD(init); LOAD(deinit);
            LOAD(load_game); LOAD(unload_game); LOAD(run); LOAD(get_memory_data);
            LOAD(get_memory_size); LOAD(serialize_size); LOAD(serialize); LOAD(unserialize);
#undef LOAD
        } catch (...) { dlclose(library); throw; }
    }
    ~Core() {
        if (loaded) unload_game();
        if (initialized) deinit();
        if (library) dlclose(library);
    }
};

uint32_t ramWord(Core& core, size_t offset) {
    require(core.get_memory_size(RETRO_MEMORY_SYSTEM_RAM) >= offset + 4, "Missing RDRAM");
    auto ram = static_cast<const char*>(core.get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
    require(ram != nullptr, "Missing RDRAM pointer");
    // This pinned core exposes word-swapped RDRAM on little-endian x86-64.
    uint32_t result;
    std::memcpy(&result, ram + offset, sizeof(result));
    return result;
}

size_t whitePixels() {
    return std::count_if(pixels.begin(), pixels.end(), [](uint32_t pixel) {
        return ((pixel >> 16) & 255) > 200 && ((pixel >> 8) & 255) > 200 && (pixel & 255) > 200;
    });
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 4, "Usage: core_probe core.so diagnostic.z64 output-directory");
        const auto output = std::filesystem::absolute(argv[3]);
        std::filesystem::create_directories(output / "system");
        std::filesystem::remove(output / "result.json");
        std::filesystem::remove(output / "diagnostic.png");
        systemDirectory = (output / "system").string();
        options = {
            {"mupen64plus-rdp-plugin", "angrylion"},
            {"mupen64plus-rsp-plugin", "cxd4"},
            {"mupen64plus-cpucore", "cached_interpreter"},
            {"mupen64plus-angrylion-multithread", "1"},
            {"mupen64plus-angrylion-vioverlay", "Unfiltered"},
            {"mupen64plus-ThreadedRenderer", "False"},
        };
        Core core(argv[1]);
        require(core.api_version() == RETRO_API_VERSION, "Incompatible libretro API");
        core.set_environment(environment);
        core.set_video_refresh(video);
        core.set_audio_sample(audio);
        core.set_audio_sample_batch(audioBatch);
        core.set_input_poll(poll);
        core.set_input_state(input);
        retro_system_info info{};
        core.get_system_info(&info);
        require(info.library_name && std::string(info.library_name).find("Mupen64Plus") != std::string::npos,
                "Unexpected core identity");
        std::cout << "Real core: " << info.library_name << ' ' << info.library_version << std::endl;
        core.init(); core.initialized = true;
        std::ifstream source(argv[2], std::ios::binary);
        require(source.good(), "Cannot read diagnostic ROM");
        std::vector<char> rom((std::istreambuf_iterator<char>(source)), {});
        require(rom.size() == 4096, "Expected the original 4 KiB diagnostic ROM");
        retro_game_info game{argv[2], rom.data(), rom.size(), nullptr};
        require(core.load_game(&game), "Core rejected diagnostic ROM");
        core.loaded = true;
        for (unsigned i = 0; i < 60; ++i) core.run();
        require(hardwareRequests == 0, "Software profile requested a GPU context");
        require(!invalidVideo && videoFrames > 0, "No valid software video output");
        require(ramWord(core, 0x400) == 0x52324E36, "MIPS program did not write its RAM signature");
        unsigned red{}, green{}, blue{};
        for (auto pixel : pixels) {
            const auto r = (pixel >> 16) & 255, g = (pixel >> 8) & 255, b = pixel & 255;
            red += r > 160 && g < 80 && b < 80;
            green += g > 160 && r < 80 && b < 80;
            blue += b > 160 && r < 80 && g < 80;
        }
        require(red > 1000 && green > 1000 && blue > 1000, "Missing RGB diagnostic bands");

        retro_system_av_info av{};
        core.get_system_av_info(&av);
        require(av.timing.sample_rate > 0 && audioFrames > 20000 && nonzeroAudioFrames > 20000,
                "Diagnostic AI DMA did not produce sustained PCM audio");
        require(audioPeak > 1000 && audioPeak < 4000 && unequalStereoFrames == 0,
                "Incorrect diagnostic stereo PCM amplitude/channels");
        const auto toneFrequency = risingEdges * av.timing.sample_rate / audioFrames;
        require(toneFrequency > 480 && toneFrequency < 520, "Incorrect diagnostic tone frequency");

        require(ramWord(core, 0x408) == 0, "Neutral controller was not neutral through SI/PIF");
        require(whitePixels() < 20, "Diagnostic input indicator was active while neutral");
        joypadMask = 1 << RETRO_DEVICE_ID_JOYPAD_B;  // N64 A
        for (unsigned i = 0; i < 2; ++i) core.run();
        require(ramWord(core, 0x408) == 0x80000000, "N64 A press did not reach MIPS through SI/PIF");
        require(whitePixels() > 100, "N64 A press did not update the visible input indicator");
        joypadMask = (1 << RETRO_DEVICE_ID_JOYPAD_Y) | (1 << RETRO_DEVICE_ID_JOYPAD_L2) |
                     (1 << RETRO_DEVICE_ID_JOYPAD_START) | (1 << RETRO_DEVICE_ID_JOYPAD_UP);
        for (unsigned i = 0; i < 2; ++i) core.run();
        require(ramWord(core, 0x408) == 0x78000000, "N64 B/Z/Start/Up combination was mapped incorrectly");
        joypadMask = 0;
        analogX = 20000; analogY = 20000;
        for (unsigned i = 0; i < 2; ++i) core.run();
        const auto analogResponse = ramWord(core, 0x408);
        const auto n64x = static_cast<int8_t>(analogResponse >> 8);
        const auto n64y = static_cast<int8_t>(analogResponse);
        require((analogResponse >> 16) == 0 && n64x > 0 && n64x <= 80 && n64y < 0 && n64y >= -80,
                "Analog stick did not reach MIPS with the correct axes");
        analogX = 0; analogY = 0;
        for (unsigned i = 0; i < 2; ++i) core.run();
        require(ramWord(core, 0x408) == 0, "Controller release was not received through SI/PIF");
        require(whitePixels() < 20, "Diagnostic input indicator did not clear after release");
        require(inputReads > 0, "Core did not request controller input");

        const auto stateSize = core.serialize_size();
        require(stateSize > 0 && stateSize < 64 * 1024 * 1024, "Unexpected state size");
        std::vector<char> state(stateSize);
        require(core.serialize(state.data(), state.size()), "State serialization failed");
        const auto savedCounter = ramWord(core, 0x404);
        for (unsigned i = 0; i < 5; ++i) core.run();
        require(ramWord(core, 0x404) != savedCounter, "MIPS counter did not advance");
        require(core.unserialize(state.data(), state.size()), "State restoration failed");
        require(ramWord(core, 0x404) == savedCounter, "Restored state has wrong RAM counter");
        core.run();
        require(ramWord(core, 0x404) != savedCounter, "Core did not resume after state restoration");

        auto* frame = SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4,
                                               0x00FF0000, 0x0000FF00, 0x000000FF, 0);
        require(frame != nullptr, "Cannot create diagnostic capture surface");
        const auto capturePath = (output / "diagnostic.png").string();
        const bool captured = IMG_SavePNG(frame, capturePath.c_str()) == 0;
        SDL_FreeSurface(frame);
        require(captured, "Cannot write diagnostic capture");
        std::ofstream report(output / "result.json");
        report << "{\n  \"platform\": \"desktop-linux\",\n  \"software_video_frames\": " << videoFrames
               << ",\n  \"width\": " << width << ",\n  \"height\": " << height
               << ",\n  \"audio_frames\": " << audioFrames << ",\n  \"input_polls\": " << inputPolls
               << ",\n  \"nonzero_audio_frames\": " << nonzeroAudioFrames
               << ",\n  \"audio_peak\": " << audioPeak
               << ",\n  \"audio_sample_rate\": " << av.timing.sample_rate
               << ",\n  \"audio_tone_hz\": " << toneFrequency
               << ",\n  \"input_reads\": " << inputReads
               << ",\n  \"hardware_context_requests\": " << hardwareRequests
               << ",\n  \"mips_signature_verified\": true,\n  \"rgb_bands_verified\": true,\n"
               << "  \"ai_audio_verified\": true,\n  \"si_pif_controller_verified\": true,\n"
               << "  \"state_round_trip_verified\": true,\n  \"ps4_hardware_tested\": false\n}\n";
        report.close();
        require(report.good(), "Cannot write diagnostic report");
        std::cout << "PASS: MIPS execution, RGB software video (" << width << 'x' << height
                  << "), AI PCM audio (" << toneFrequency << " Hz), SI/PIF buttons/stick, state round trip and resume. "
                  << videoFrames << " video frames.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
