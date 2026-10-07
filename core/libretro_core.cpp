#include "core/libretro_core.h"
#include "n64_profiles.h"
#include "frontend/system_detector.h"
#include "log.h"
#include "rom.h"
#include "storage.h"
#include <libretro.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <unistd.h>
#ifdef R2N64_PS4
#include <orbis/libkernel.h>
#endif

namespace {
double hardwareClockMs() {
#ifdef R2N64_PS4
    // PacBrew CLOCK_MONOTONIC maps to a different libkernel clock ID.
    // Use the same elapsed-time clock as the PS4 core profiler.
    return static_cast<double>(sceKernelGetProcessTime()) / 1000.0;
#else
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}
}

// R2N64 core patch API: uses the real aligned code cache, not a guessed OS
// capability. Probe only after SDL/GoldHEN startup and retro_init.
extern "C" int retro_r2n64_dynarec_available(void);
extern "C" int retro_r2n64_dynarec_active(void);
extern "C" void retro_r2n64_set_audio_hle(int enabled);
extern "C" uint64_t retro_r2n64_audio_hle_tasks(void);
extern "C" uint64_t retro_r2n64_graphics_hle_tasks(void);
extern "C" uint64_t retro_r2n64_graphics_lle_tasks(void);

#ifdef R2N64_HAS_SAMEBOY
extern "C" {
extern decltype(retro_init) sameboy_retro_init;
extern decltype(retro_deinit) sameboy_retro_deinit;
extern decltype(retro_api_version) sameboy_retro_api_version;
extern decltype(retro_get_system_info) sameboy_retro_get_system_info;
extern decltype(retro_get_system_av_info) sameboy_retro_get_system_av_info;
extern decltype(retro_set_environment) sameboy_retro_set_environment;
extern decltype(retro_set_video_refresh) sameboy_retro_set_video_refresh;
extern decltype(retro_set_audio_sample) sameboy_retro_set_audio_sample;
extern decltype(retro_set_audio_sample_batch) sameboy_retro_set_audio_sample_batch;
extern decltype(retro_set_input_poll) sameboy_retro_set_input_poll;
extern decltype(retro_set_input_state) sameboy_retro_set_input_state;
extern decltype(retro_set_controller_port_device) sameboy_retro_set_controller_port_device;
extern decltype(retro_reset) sameboy_retro_reset;
extern decltype(retro_run) sameboy_retro_run;
extern decltype(retro_load_game) sameboy_retro_load_game;
extern decltype(retro_unload_game) sameboy_retro_unload_game;
extern decltype(retro_serialize_size) sameboy_retro_serialize_size;
extern decltype(retro_serialize) sameboy_retro_serialize;
extern decltype(retro_unserialize) sameboy_retro_unserialize;
extern decltype(retro_get_memory_data) sameboy_retro_get_memory_data;
extern decltype(retro_get_memory_size) sameboy_retro_get_memory_size;
}
#endif
#ifdef R2N64_HAS_MGBA
extern "C" {
extern decltype(retro_init) mgba_retro_init;
extern decltype(retro_deinit) mgba_retro_deinit;
extern decltype(retro_api_version) mgba_retro_api_version;
extern decltype(retro_get_system_info) mgba_retro_get_system_info;
extern decltype(retro_get_system_av_info) mgba_retro_get_system_av_info;
extern decltype(retro_set_environment) mgba_retro_set_environment;
extern decltype(retro_set_video_refresh) mgba_retro_set_video_refresh;
extern decltype(retro_set_audio_sample) mgba_retro_set_audio_sample;
extern decltype(retro_set_audio_sample_batch) mgba_retro_set_audio_sample_batch;
extern decltype(retro_set_input_poll) mgba_retro_set_input_poll;
extern decltype(retro_set_input_state) mgba_retro_set_input_state;
extern decltype(retro_set_controller_port_device) mgba_retro_set_controller_port_device;
extern decltype(retro_reset) mgba_retro_reset;
extern decltype(retro_run) mgba_retro_run;
extern decltype(retro_load_game) mgba_retro_load_game;
extern decltype(retro_unload_game) mgba_retro_unload_game;
extern decltype(retro_serialize_size) mgba_retro_serialize_size;
extern decltype(retro_serialize) mgba_retro_serialize;
extern decltype(retro_unserialize) mgba_retro_unserialize;
extern decltype(retro_get_memory_data) mgba_retro_get_memory_data;
extern decltype(retro_get_memory_size) mgba_retro_get_memory_size;
}
#endif

#define R2N64_DECLARE_CORE(prefix) \
    extern "C" { \
    extern decltype(retro_init) prefix##retro_init; \
    extern decltype(retro_deinit) prefix##retro_deinit; \
    extern decltype(retro_api_version) prefix##retro_api_version; \
    extern decltype(retro_get_system_info) prefix##retro_get_system_info; \
    extern decltype(retro_get_system_av_info) prefix##retro_get_system_av_info; \
    extern decltype(retro_set_environment) prefix##retro_set_environment; \
    extern decltype(retro_set_video_refresh) prefix##retro_set_video_refresh; \
    extern decltype(retro_set_audio_sample) prefix##retro_set_audio_sample; \
    extern decltype(retro_set_audio_sample_batch) prefix##retro_set_audio_sample_batch; \
    extern decltype(retro_set_input_poll) prefix##retro_set_input_poll; \
    extern decltype(retro_set_input_state) prefix##retro_set_input_state; \
    extern decltype(retro_set_controller_port_device) prefix##retro_set_controller_port_device; \
    extern decltype(retro_reset) prefix##retro_reset; \
    extern decltype(retro_run) prefix##retro_run; \
    extern decltype(retro_load_game) prefix##retro_load_game; \
    extern decltype(retro_unload_game) prefix##retro_unload_game; \
    extern decltype(retro_serialize_size) prefix##retro_serialize_size; \
    extern decltype(retro_serialize) prefix##retro_serialize; \
    extern decltype(retro_unserialize) prefix##retro_unserialize; \
    extern decltype(retro_get_memory_data) prefix##retro_get_memory_data; \
    extern decltype(retro_get_memory_size) prefix##retro_get_memory_size; \
    }
#ifdef R2N64_HAS_FCEUMM
R2N64_DECLARE_CORE(fceumm_)
#endif
#ifdef R2N64_HAS_BSNES_MERCURY
R2N64_DECLARE_CORE(bsnes_mercury_)
#endif
#undef R2N64_DECLARE_CORE

namespace r2n64 {
namespace {
constexpr size_t MaxStateBytes = 64 * 1024 * 1024;
constexpr size_t StateHeaderSize = 192;
constexpr size_t MaxSaveBytes = 16 * 1024 * 1024;
constexpr size_t MaxAudioSamples = 44100 * 2; // Bounded even for a misbehaving core.
constexpr std::array<const char*, GameBoyPaletteCount> GameBoyPalettes{{"greyscale", "lime", "olive", "teal", "r2retro_color"}};

bool readExact(int fd, void* destination, size_t size) {
    auto* bytes = static_cast<unsigned char*>(destination);
    while (size) {
        const auto n = ::read(fd, bytes, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        bytes += n;
        size -= static_cast<size_t>(n);
    }
    return true;
}
bool writeExact(int fd, const void* source, size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(source);
    while (size) {
        const auto n = ::write(fd, bytes, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        bytes += n;
        size -= static_cast<size_t>(n);
    }
    return true;
}
bool realDirectory(const std::string& path) {
    if (::mkdir(path.c_str(), 0700) < 0 && errno != EEXIST) return false;
    struct stat st{};
    return ::lstat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
bool managedConfigFile(const std::string& path) {
    // The pinned core refreshes its bundled INI using fopen("w"). Refuse an
    // existing link/device before letting that legacy path write into it.
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (fd < 0) return false;
    struct stat st{};
    const bool regular = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_nlink == 1;
    ::close(fd);
    return regular;
}
std::string cartridgeFingerprint(const std::vector<unsigned char>& rom, RomOrder order) {
    // Include every byte, in canonical .z64 order. Header CRCs alone collide
    // for homebrew and modified dumps; swapped copies should share their save.
    uint64_t hash = UINT64_C(14695981039346656037);
    const size_t xorMask = order == RomOrder::ByteSwapped ? 1 : order == RomOrder::LittleEndian ? 3 : 0;
    for (size_t i = 0; i < rom.size(); ++i) {
        hash ^= rom[i ^ xorMask];
        hash *= UINT64_C(1099511628211);
    }
    char text[17];
    std::snprintf(text, sizeof(text), "%016llX", static_cast<unsigned long long>(hash));
    return text;
}
bool atomicWrite(const std::string& path, const void* bytes, size_t size, std::string& error) {
    struct stat st{};
    const int existing = ::lstat(path.c_str(), &st);
    if ((existing == 0 && !S_ISREG(st.st_mode)) || (existing < 0 && errno != ENOENT)) {
        error = "No se reemplazo un archivo no regular: " + path; return false;
    }
    const auto temporary = path + ".tmp";
    if (::lstat(temporary.c_str(), &st) == 0) {
        if (!S_ISREG(st.st_mode) || st.st_nlink != 1 || ::unlink(temporary.c_str()) != 0) {
            error = "Temporal no regular o no eliminable: " + temporary; return false;
        }
    } else if (errno != ENOENT) { error = "No se pudo inspeccionar el temporal."; return false; }
    const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (fd < 0) { error = "No se pudo crear el temporal: " + temporary; return false; }
    bool ok = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && writeExact(fd, bytes, size) && ::fsync(fd) == 0;
    if (::close(fd) != 0) ok = false;
    if (ok) ok = ::rename(temporary.c_str(), path.c_str()) == 0;
    if (!ok) { ::unlink(temporary.c_str()); error = "No se pudo completar el guardado; se conserva el anterior."; }
    return ok;
}
uint64_t hashBytes(const unsigned char* bytes, size_t size) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= UINT64_C(1099511628211); }
    return hash;
}
void put64(unsigned char* target, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) target[i] = static_cast<unsigned char>(value >> (i * 8));
}
uint64_t get64(const unsigned char* source) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= static_cast<uint64_t>(source[i]) << (i * 8);
    return value;
}

int routineMgbaDmaChannel(const char* text, size_t length) {
    // Pinned mGBA src/gba/dma.c / GBARetroLog format. Match the entire INFO
    // record, not the category or prefix: changed formats and diagnostics
    // (including an explanatory suffix) must remain visible.
    constexpr char pattern[] = "GBA DMA: Starting DMA # 0xXXXXXXXX -> 0xXXXXXXXX (XXXX:XXXX)";
    if (length != sizeof(pattern) - 1) return -1;
    int channel = -1;
    for (size_t i = 0; i < sizeof(pattern) - 1; ++i) {
        if (pattern[i] == 'X') {
            if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'A' && text[i] <= 'F'))) return -1;
        } else if (pattern[i] == '#') {
            if (text[i] < '0' || text[i] > '3') return -1;
            channel = text[i] - '0';
        } else if (text[i] != pattern[i]) return -1;
    }
    return channel;
}

}

struct LibretroCore::State {
    static State* active;
    static bool lostHardwareContext;
    State(const CoreDescriptor& descriptor, const CoreApi& functions) : descriptor(&descriptor), api(&functions) {}
    const CoreDescriptor* descriptor;
    const CoreApi* api;
    SystemType system = SystemType::Unknown;
    EmulationConfig config{};
    const char* appliedProfile = "Manual";
    HardwareTiming boundaryTiming{};
    retro_pixel_format pixelFormat = RETRO_PIXEL_FORMAT_0RGB1555;
    std::string romPath, dataRoot, identity, stateBase, coreVersion;
    struct Save { unsigned id; std::string path, legacy; bool writable = true; };
    std::vector<Save> saves;
    // Retain content for cores that keep a view until unload_game.
    std::vector<unsigned char> rom;
    bool n64() const { return system == SystemType::Nintendo64; }
    bool initialized = false, loaded = false;
    bool acceptedPixelFormat = false, saveWritable = true;
    bool jitCapable = false, ran = false;
    bool optionsUpdated = false;
    retro_hw_render_callback hardwareCallback{};
    bool hardwareNegotiated = false, hardwareReady = false;
    bool gpu() const { return n64() && config.graphics == N64Graphics::Gles2; }
    static uintptr_t framebuffer() { return active && active->config.hardware ? active->config.hardware->framebuffer() : 0; }
    static retro_proc_address_t procedure(const char* name) {
        return active && active->config.hardware ? active->config.hardware->procedure(name) : nullptr;
    }
    Log* log = nullptr;
    std::string systemDirectory, saveDirectory, savePath, failure, lastCoreError;
    std::map<std::string, std::string> options;
    CoreFrame frame;
    uint64_t videoSerial = 0; // Survives reset/state load within this core object.
    std::vector<int16_t> audio;
    size_t audioSamplesReceived = 0;
    std::array<uint64_t, 4> routineDmaMessages{};
    GamepadInput input{};
    double fps = 60.0, rate = 44100.0;
    // Consume every core callback during a portable speed-up, keeping the APU
    // clock and queues intact, but do not copy PCM the frontend will discard.
    bool fastForwarding() const { return !n64() && input.connected && input.fastForward; }

    void fail(const char* message) {
        if (failure.empty()) failure = message;
    }
    static void coreLog(retro_log_level level, const char* format, ...) {
        if (!active || !active->log || !format) return;
        char text[2048];
        va_list args;
        va_start(args, format);
        std::vsnprintf(text, sizeof(text), format, args);
        va_end(args);
        size_t length = std::strlen(text);
        while (length && (text[length - 1] == '\n' || text[length - 1] == '\r')) text[--length] = 0;
        // Debug output can include every emulated event; keep the console log useful.
        if (level == RETRO_LOG_DEBUG) return;
        if (level == RETRO_LOG_INFO && active->system == SystemType::GameBoyAdvance &&
            std::strcmp(active->descriptor->id, "mgba") == 0) {
            const int channel = routineMgbaDmaChannel(text, length);
            if (channel >= 0) {
                auto& count = active->routineDmaMessages[static_cast<size_t>(channel)];
                if (count != UINT64_MAX) ++count;
                return; // No allocation, timestamp, mutex or file I/O for routine DMA.
            }
        }
        try {
            if (level >= RETRO_LOG_ERROR) active->lastCoreError = text;
            active->log->write(level >= RETRO_LOG_ERROR ? "ERROR" : level == RETRO_LOG_WARN ? "WARN" : "CORE", text);
        }
        catch (...) { /* Never unwind through the C core or a libco stack. */ }
    }
    void finishLogSession() {
        if (log && std::any_of(routineDmaMessages.begin(), routineDmaMessages.end(),
                             [](uint64_t count) { return count != 0; })) {
            char text[256];
            std::snprintf(text, sizeof(text),
                "mGBA: routine DMA start messages aggregated; channel 0=%llu, 1=%llu, 2=%llu, 3=%llu",
                static_cast<unsigned long long>(routineDmaMessages[0]),
                static_cast<unsigned long long>(routineDmaMessages[1]),
                static_cast<unsigned long long>(routineDmaMessages[2]),
                static_cast<unsigned long long>(routineDmaMessages[3]));
            try { log->write("CORE", text); }
            catch (...) { /* Unload must remain safe even while reporting diagnostics. */ }
        }
        routineDmaMessages.fill(0);
    }
    bool acceptOptions(const retro_core_options_v2* definitions) {
        if (!definitions || !definitions->definitions) return false;
        for (const auto* def = definitions->definitions; def->key; ++def) {
            const char* value = def->default_value ? def->default_value : def->values[0].value;
            if (value) options.emplace(def->key, value);
        }
        return true;
    }
    void timing(const retro_system_av_info& info) {
        if (std::isfinite(info.timing.fps) && info.timing.fps >= 20 && info.timing.fps <= 120)
            fps = info.timing.fps;
        else fail("El nucleo solicito una frecuencia de video no compatible.");
        if (std::isfinite(info.timing.sample_rate) && info.timing.sample_rate >= 8000 && info.timing.sample_rate <= 192000)
            rate = info.timing.sample_rate;
        else fail("El nucleo solicito una frecuencia de audio no compatible.");
    }
    static bool rumble(unsigned, retro_rumble_effect, uint16_t) { return false; }
    static bool environment(unsigned command, void* data) {
        if (!active) return false;
        auto& s = *active;
        try {
            if (!data && command != RETRO_ENVIRONMENT_GET_INPUT_BITMASKS &&
                command != RETRO_ENVIRONMENT_SHUTDOWN) return false;
            switch (command) {
            case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
                if (!data) return false;
                *static_cast<const char**>(data) = s.systemDirectory.c_str(); return true;
            case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
                if (!data) return false;
                *static_cast<const char**>(data) = s.saveDirectory.c_str(); return true;
            case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
                static_cast<retro_log_callback*>(data)->log = coreLog; return true;
            case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
                static_cast<retro_rumble_interface*>(data)->set_rumble_state = rumble; return true;
            case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
                *static_cast<unsigned*>(data) = 2; return true;
            case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
                return s.acceptOptions(static_cast<const retro_core_options_v2*>(data));
            case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
                return data && s.acceptOptions(static_cast<const retro_core_options_v2_intl*>(data)->us);
            case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
                const auto* definitions = static_cast<const retro_core_option_definition*>(data);
                for (const auto* d = definitions; d->key; ++d) {
                    const char* value = d->default_value ? d->default_value : d->values[0].value;
                    if (value) s.options.emplace(d->key, value);
                }
                return true;
            }
            case RETRO_ENVIRONMENT_SET_VARIABLES:
                for (const auto* v = static_cast<const retro_variable*>(data); v->key; ++v) {
                    if (!v->value) continue;
                    const char* first = std::strchr(v->value, ';');
                    if (!first) continue;
                    while (*++first == ' ') {}
                    const char* end = std::strchr(first, '|');
                    s.options.emplace(v->key, end ? std::string(first, end) : std::string(first));
                }
                return true;
            case RETRO_ENVIRONMENT_GET_VARIABLE: {
                auto* variable = static_cast<retro_variable*>(data);
                if (!variable || !variable->key) return false;
                const auto value = s.options.find(variable->key);
                variable->value = value == s.options.end() ? nullptr : value->second.c_str();
                return variable->value != nullptr;
            }
            case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
                *static_cast<bool*>(data) = s.optionsUpdated;
                s.optionsUpdated = false;
                return true;
            case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
                s.pixelFormat = *static_cast<const retro_pixel_format*>(data);
                s.acceptedPixelFormat = s.pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888 ||
                    s.pixelFormat == RETRO_PIXEL_FORMAT_RGB565 || s.pixelFormat == RETRO_PIXEL_FORMAT_0RGB1555;
                if (!s.acceptedPixelFormat) s.fail("El nucleo solicito un formato de video no compatible.");
                return s.acceptedPixelFormat;
            case RETRO_ENVIRONMENT_SET_HW_RENDER: {
                auto* callback = static_cast<retro_hw_render_callback*>(data);
                if (!s.gpu() || !s.config.hardware || s.hardwareNegotiated ||
                    callback->context_type != RETRO_HW_CONTEXT_OPENGLES2 || !callback->context_reset) {
                    s.fail("No se puede proporcionar el contexto GLES2 solicitado por el nucleo.");
                    return false;
                }
                callback->get_current_framebuffer = framebuffer;
                callback->get_proc_address = procedure;
                s.hardwareCallback = *callback;
                s.hardwareNegotiated = true;
                return true;
            }
            case RETRO_ENVIRONMENT_GET_CAN_DUPE:
                *static_cast<bool*>(data) = true; return true;
            case RETRO_ENVIRONMENT_GET_JIT_CAPABLE:
                if (!data) return false;
                *static_cast<bool*>(data) = s.jitCapable; return true;
            case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
                *static_cast<bool*>(data) = s.fastForwarding(); return true;
            case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
                *static_cast<int*>(data) = s.fastForwarding() ? 1 : 3; return true;
            case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
                *static_cast<uint64_t*>(data) = (1ULL << RETRO_DEVICE_JOYPAD) | (1ULL << RETRO_DEVICE_ANALOG); return true;
            case RETRO_ENVIRONMENT_GET_LANGUAGE:
                *static_cast<unsigned*>(data) = RETRO_LANGUAGE_ENGLISH; return true;
            case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
                if (!data) return false;
                s.timing(*static_cast<const retro_system_av_info*>(data)); return s.failure.empty();
            case RETRO_ENVIRONMENT_SET_MESSAGE:
                if (data && static_cast<const retro_message*>(data)->msg)
                    coreLog(RETRO_LOG_INFO, "%s", static_cast<const retro_message*>(data)->msg);
                return true;
            case RETRO_ENVIRONMENT_SHUTDOWN:
                s.fail("El nucleo solicito cerrar la emulacion."); return true;
            case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
            case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
            case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
            case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
            case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
            case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
            case RETRO_ENVIRONMENT_SET_GEOMETRY:
            case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
                return true;
            default: return false;
            }
        } catch (...) {
            s.fail("No hay memoria suficiente para configurar el nucleo.");
            return false;
        }
    }
    static void video(const void* data, unsigned width, unsigned height, size_t pitch) {
        if (!active || !data) return; // NULL duplicates the last frame.
        auto& s = *active;
        if (data == RETRO_HW_FRAME_BUFFER_VALID) {
            if (!s.hardwareReady || width == 0 || height == 0 || width > 640 || height > 480) {
                s.fail("El nucleo entrego un cuadro GPU fuera del perfil compatible."); return;
            }
            s.frame.hardware = true;
            s.frame.bottomLeftOrigin = s.hardwareCallback.bottom_left_origin;
            s.frame.width = width; s.frame.height = height;
            if (!++s.videoSerial) ++s.videoSerial;
            s.frame.serial = s.videoSerial;
            return;
        }
        if (s.gpu() || !s.acceptedPixelFormat ||
            width == 0 || height == 0 || width > 2048 || height > 2048 ||
            pitch < static_cast<size_t>(width) * (s.pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2) ||
            pitch > 16384) {
            s.fail("El nucleo entrego un cuadro de video no valido."); return;
        }
        try {
            s.frame.pixels.resize(static_cast<size_t>(width) * height);
            for (unsigned y = 0; y < height; ++y) {
                auto* target = s.frame.pixels.data() + static_cast<size_t>(y) * width;
                const auto* source = static_cast<const unsigned char*>(data) + static_cast<size_t>(y) * pitch;
                if (s.pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888) std::memcpy(target, source, width * 4);
                else for (unsigned x = 0; x < width; ++x) {
                    uint16_t pixel;
                    std::memcpy(&pixel, source + x * 2, sizeof(pixel));
                    const unsigned r = (pixel >> (s.pixelFormat == RETRO_PIXEL_FORMAT_RGB565 ? 11 : 10)) & 31;
                    const unsigned g = (pixel >> 5) & (s.pixelFormat == RETRO_PIXEL_FORMAT_RGB565 ? 63 : 31);
                    const unsigned b = pixel & 31;
                    const unsigned green = s.pixelFormat == RETRO_PIXEL_FORMAT_RGB565 ? (g << 2) | (g >> 4) : (g << 3) | (g >> 2);
                    target[x] = (((r << 3) | (r >> 2)) << 16) | (green << 8) | (b << 3) | (b >> 2);
                }
            }
            s.frame.width = width;
            s.frame.height = height;
            if (!++s.videoSerial) ++s.videoSerial;
            s.frame.serial = s.videoSerial;
        } catch (...) { s.fail("No hay memoria suficiente para el cuadro de video."); }
    }
    static void poll() {} // Platform already took one coherent snapshot before retro_run.
    static int16_t inputState(unsigned port, unsigned device, unsigned index, unsigned id) {
        if (!active || port != 0 || !active->input.connected) return 0;
        const auto& pad = active->input;
        if (device == RETRO_DEVICE_JOYPAD) {
            uint16_t buttons = pad.buttons;
            if (!active->n64()) {
                if (active->system == SystemType::SuperNintendo) {
                    // SNES follows the physical diamond: Cross=B, Square=Y,
                    // Circle=A, Triangle=X. Other systems retain their A/B map.
                    if (pad.faceEast) buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
                    if (pad.faceNorth) buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_X;
                } else {
                // The platform's existing Cross/Square bits mean N64 A/B.
                // Portable libretro cores expect A at bit 8 and B at bit 0.
                buttons &= static_cast<uint16_t>(~((1u << RETRO_DEVICE_ID_JOYPAD_B) | (1u << RETRO_DEVICE_ID_JOYPAD_Y)));
                if (pad.buttons & (1u << RETRO_DEVICE_ID_JOYPAD_B)) buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
                if (pad.buttons & (1u << RETRO_DEVICE_ID_JOYPAD_Y)) buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
                }
                if (pad.start) buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
                if (pad.select) buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
            }
            if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return static_cast<int16_t>(buttons);
            return id < 16 && (buttons & (1u << id)) ? 1 : 0;
        }
        if (device == RETRO_DEVICE_ANALOG && id <= RETRO_DEVICE_ID_ANALOG_Y) {
            if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT)
                return id == RETRO_DEVICE_ID_ANALOG_X ? pad.analogX : pad.analogY;
            if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                return id == RETRO_DEVICE_ID_ANALOG_X ? pad.rightX : pad.rightY;
        }
        return 0;
    }
    static size_t audioBatch(const int16_t* data, size_t frames) {
        if (!active || !data || !frames) return frames;
        auto& s = *active;
        if (frames > MaxAudioSamples / 2 || s.audioSamplesReceived > MaxAudioSamples - frames * 2) {
            s.fail("El nucleo excedio el limite del buffer de audio."); return frames;
        }
        s.audioSamplesReceived += frames * 2;
        if (s.fastForwarding()) return frames;
        try { s.audio.insert(s.audio.end(), data, data + frames * 2); }
        catch (...) { s.fail("No hay memoria suficiente para el audio."); }
        return frames;
    }
    static void audioSample(int16_t left, int16_t right) {
        const int16_t samples[] = {left, right};
        audioBatch(samples, 1);
    }
    void restoreSaves() {
        for (auto& save : saves) {
            void* memory = api->get_memory_data(save.id);
            const size_t capacity = api->get_memory_size(save.id);
            if (!memory || !capacity || capacity > MaxSaveBytes) continue;
            std::string source = save.path;
            int fd = ::open(source.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
            // Migrate only when the new per-system file is absent. Never delete
            // or update the legacy copy, even after successful migration.
            if (fd < 0 && errno == ENOENT && !save.legacy.empty()) {
                source = save.legacy;
                fd = ::open(source.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
            }
            if (fd < 0) {
                if (errno != ENOENT) {
                    save.writable = false;
                    log->write("WARN", "No se puede abrir guardado; se conserva: " + source);
                }
                continue;
            }
            struct stat st{};
            bool valid = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0 &&
                         st.st_size <= static_cast<off_t>(capacity);
            const size_t size = valid ? static_cast<size_t>(st.st_size) : 0;
            if (size != capacity) {
                // mGBA initially exposes its 128 KiB autodetection buffer;
                // after gameplay its size can become EEPROM/SRAM/Flash size.
                valid = valid && system == SystemType::GameBoyAdvance && save.id == RETRO_MEMORY_SAVE_RAM &&
                    (size == 512 || size == 8192 || size == 32768 || size == 65536 || size == 131072);
            }
            std::vector<unsigned char> restored;
            try { if (valid) restored.resize(size); }
            catch (...) { ::close(fd); throw; }
            const bool read = valid && readExact(fd, restored.data(), size);
            ::close(fd);
            if (read) {
                std::memcpy(memory, restored.data(), size);
                log->write("INFO", "Guardado restaurado: " + source);
            } else {
                save.writable = false;
                log->write("WARN", "Guardado incompatible o incompleto; se conserva sin sobrescribir: " + source);
            }
        }
    }
    bool persistSaves(std::string* report = nullptr) {
        if (report) report->clear();
        if (!saveWritable) {
            if (report) *report = "Guardado protegido: la sesion no permite escribir SRAM/RTC.";
            return false;
        }
        bool written = false, failed = false;
        for (const auto& save : saves) {
            const void* memory = api->get_memory_data(save.id);
            const size_t size = api->get_memory_size(save.id);
            if (!size) continue;
            std::string error;
            if (!save.writable) error = "Guardado anterior protegido; no se sobrescribe: " + save.path;
            else if (!memory || size > MaxSaveBytes) error = "Memoria de guardado no valida: " + save.path;
            else if (atomicWrite(save.path, memory, size, error)) {
                written = true;
                log->write("INFO", "Guardado escrito: " + save.path);
                continue;
            }
            failed = true;
            log->write("ERROR", error);
            if (report && report->empty()) *report = error;
        }
        if (report && !written && !failed) *report = "Este cartucho no expone memoria SRAM/RTC para guardar.";
        return written && !failed;
    }
    std::array<unsigned char, StateHeaderSize> stateHeader(size_t size, uint64_t hash) const {
        std::array<unsigned char, StateHeaderSize> header{};
        std::memcpy(header.data(), "R2STATE", 7);
        header[8] = 1; // Frontend state-container version; bump on incompatible bridge changes.
        header[12] = static_cast<unsigned char>(system);
        put64(header.data() + 16, size);
        put64(header.data() + 24, hash);
        std::memcpy(header.data() + 32, descriptor->id, std::strlen(descriptor->id));
        std::memcpy(header.data() + 64, coreVersion.data(), coreVersion.size());
        std::memcpy(header.data() + 128, identity.data(), identity.size());
        return header;
    }
};
LibretroCore::State* LibretroCore::State::active = nullptr;
bool LibretroCore::State::lostHardwareContext = false;

const CoreApi* coreApiFor(SystemType system) {
    static const CoreApi n64{
        retro_init, retro_deinit, retro_api_version, retro_get_system_info, retro_get_system_av_info,
        retro_set_environment, retro_set_video_refresh, retro_set_audio_sample, retro_set_audio_sample_batch,
        retro_set_input_poll, retro_set_input_state, retro_set_controller_port_device, retro_reset, retro_run,
        retro_load_game, retro_unload_game, retro_serialize_size, retro_serialize, retro_unserialize,
        retro_get_memory_data, retro_get_memory_size, retro_r2n64_dynarec_available, retro_r2n64_dynarec_active,
        retro_r2n64_set_audio_hle, retro_r2n64_audio_hle_tasks, retro_r2n64_profile_set_enabled,
        retro_r2n64_profile_reset, retro_r2n64_profile_read,
        retro_r2n64_graphics_hle_tasks, retro_r2n64_graphics_lle_tasks
    };
#define R2N64_CORE_API(prefix) prefix##retro_init, prefix##retro_deinit, prefix##retro_api_version, \
    prefix##retro_get_system_info, prefix##retro_get_system_av_info, prefix##retro_set_environment, \
    prefix##retro_set_video_refresh, prefix##retro_set_audio_sample, prefix##retro_set_audio_sample_batch, \
    prefix##retro_set_input_poll, prefix##retro_set_input_state, prefix##retro_set_controller_port_device, \
    prefix##retro_reset, prefix##retro_run, prefix##retro_load_game, prefix##retro_unload_game, \
    prefix##retro_serialize_size, prefix##retro_serialize, prefix##retro_unserialize, \
    prefix##retro_get_memory_data, prefix##retro_get_memory_size
#ifdef R2N64_HAS_SAMEBOY
    static const CoreApi sameboy{R2N64_CORE_API(sameboy_)};
    if (system == SystemType::GameBoy || system == SystemType::GameBoyColor) return &sameboy;
#endif
#ifdef R2N64_HAS_MGBA
    static const CoreApi mgba{R2N64_CORE_API(mgba_)};
    if (system == SystemType::GameBoyAdvance) return &mgba;
#endif
#ifdef R2N64_HAS_FCEUMM
    static const CoreApi fceumm{R2N64_CORE_API(fceumm_)};
    if (system == SystemType::NintendoEntertainmentSystem) return &fceumm;
#endif
#ifdef R2N64_HAS_BSNES_MERCURY
    static const CoreApi bsnes{R2N64_CORE_API(bsnes_mercury_)};
    if (system == SystemType::SuperNintendo) return &bsnes;
#endif
#undef R2N64_CORE_API
    return system == SystemType::Nintendo64 ? &n64 : nullptr;
}

LibretroCore::LibretroCore(const CoreDescriptor& descriptor, const CoreApi& api) : state_(new State(descriptor, api)) {}
LibretroCore::~LibretroCore() { unload(); }

bool LibretroCore::load(const std::string& romPath, const std::string& dataRoot, Log& log,
                        std::string& error, EmulationConfig config) {
    unload();
    error.clear();
    auto& s = *state_;
    if (State::lostHardwareContext) { error = "Se perdio el contexto GPU. Cierra y vuelve a abrir R2RETRO."; return false; }
    if (State::active && State::active != &s) { error = "Ya existe una sesion de emulacion activa."; return false; }
    s.system = detectSystemFromExtension(romPath);
    if (!s.descriptor->supports(s.system)) { error = "El nucleo no admite el sistema seleccionado."; return false; }
    s.log = &log;
    s.failure.clear();
    s.lastCoreError.clear();
    s.pixelFormat = RETRO_PIXEL_FORMAT_0RGB1555; // Libretro default when SET_PIXEL_FORMAT is not called.
    s.acceptedPixelFormat = !s.n64();
    s.saveWritable = true;
    s.jitCapable = false;
    s.ran = false;
    s.optionsUpdated = false;
    s.audioSamplesReceived = 0;
    s.routineDmaMessages.fill(0);
    s.fps = 60;
    s.rate = 44100;
    s.config = config;
    s.appliedProfile = "Manual";
    s.boundaryTiming = {};
    if (s.n64() && config.graphics != N64Graphics::Software && config.graphics != N64Graphics::Gles2) {
        error = "El backend grafico no es valido."; return false;
    }
    if (s.gpu() && !config.hardware) { error = "El modo GPU necesita un contexto de video GLES2."; return false; }
    s.romPath = romPath;
    s.dataRoot = dataRoot;
    s.saves.clear();
    s.options.clear();
    if (s.system == SystemType::GameBoy && config.gbPalette >= GameBoyPaletteCount) {
        error = "La paleta de Game Boy no es valida."; return false;
    }
    if (s.system == SystemType::GameBoyAdvance && config.gbaFrameskip > 2) {
        error = "Salto de cuadros GBA fuera de rango (0 a 2)."; return false;
    }
    if (s.n64() && (config.workers != 1 && config.workers != 4)) {
        error = "El perfil grafico requiere 1 o 4 trabajadores."; return false;
    }
    if (s.n64() && config.cpuMode != CpuMode::Automatic && config.cpuMode != CpuMode::CachedInterpreter) {
        error = "El perfil de CPU no es valido."; return false;
    }
    if (dataRoot.empty() || dataRoot.size() > 1024 || romPath.empty() || romPath.size() > 2048) {
        error = "La ruta de ROM o datos es demasiado larga o esta vacia."; return false;
    }
    if (!prepareStorage(dataRoot, error)) return false;
    s.systemDirectory = dataRoot + "/system";
    s.saveDirectory = dataRoot + "/saves/" + systemId(s.system);
    const auto stateDirectory = dataRoot + "/states/" + systemId(s.system);
    if (!realDirectory(s.systemDirectory) || !realDirectory(s.saveDirectory) || !realDirectory(stateDirectory)) {
        error = "No se pudo preparar el directorio del nucleo."; return false;
    }
    if (s.n64() && (!realDirectory(s.systemDirectory + "/Mupen64plus") ||
        !managedConfigFile(s.systemDirectory + "/Mupen64plus/mupen64plus.ini"))) {
        error = "La configuracion del nucleo no es un archivo regular propio."; return false;
    }
    const int fd = ::open(romPath.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) { error = "No se puede abrir ROM: " + std::string(std::strerror(errno)); return false; }
    struct stat st{};
    const bool valid = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= 192 &&
                       st.st_size <= static_cast<off_t>(maximumRomFileSize(s.system));
    try { if (valid) s.rom.resize(static_cast<size_t>(st.st_size)); }
    catch (...) { ::close(fd); error = "No hay memoria suficiente para cargar la ROM."; return false; }
    const bool read = valid && readExact(fd, s.rom.data(), s.rom.size());
    ::close(fd);
    if (!read || !validateSystemHeader(s.system, s.rom.data(), s.rom.size(), s.rom.size(), error)) {
        if (error.empty()) error = "ROM truncada, no regular o demasiado grande.";
        s.rom.clear();
        return false;
    }
    // A copier's 512-byte prefix is metadata, not cartridge content. Normalize
    // it before hashing so .sfc/.smc copies share SRAM and state identity.
    if (s.system == SystemType::SuperNintendo && snesCopierHeaderSize(s.rom.size()))
        s.rom.erase(s.rom.begin(), s.rom.begin() + 512);
    Game game;
    if (s.n64()) {
        std::array<uint8_t, 64> header{};
        std::copy_n(s.rom.data(), header.size(), header.begin());
        if (!parseHeader(header, game, error)) return false;
        s.identity = game.id + "-" + cartridgeFingerprint(s.rom, game.order);
        s.appliedProfile = applyN64Profile(s.identity, s.rom.size(), config);
        s.config = config;
        log.write("INFO", std::string("N64 profile: ") + s.appliedProfile + "; identity=" + s.identity);
    } else {
        s.identity = std::string(systemId(s.system)) + "-" + cartridgeFingerprint(s.rom, RomOrder::BigEndian);
        game.title = systemName(s.system);
        game.id = s.identity;
    }
    // Reserve the existing bounded callback budget during load, avoiding vector
    // growth in the emulation path without changing PCM or callback limits.
    try { s.audio.reserve(MaxAudioSamples); }
    catch (...) { error = "No hay memoria suficiente para el buffer de audio."; return false; }
    const auto saveBase = s.saveDirectory + "/" + s.identity;
    s.saves.push_back({RETRO_MEMORY_SAVE_RAM, saveBase + ".srm",
                      s.n64() ? dataRoot + "/saves/" + s.identity + ".srm" : "", true});
    if (!s.n64()) s.saves.push_back({RETRO_MEMORY_RTC, saveBase + ".rtc", "", true});
    s.stateBase = stateDirectory + "/" + s.identity;
    if (s.n64()) s.options = {
        {"mupen64plus-rdp-plugin", s.gpu() ? "gliden64" : "angrylion"}, {"mupen64plus-rsp-plugin", s.gpu() && config.graphicsHle ? "hle" : "cxd4"},
        {"mupen64plus-cpucore", "cached_interpreter"}, {"mupen64plus-angrylion-multithread", std::to_string(config.workers)},
        {"mupen64plus-angrylion-sync", "Low"}, {"mupen64plus-angrylion-vioverlay", "Unfiltered"},
        {"mupen64plus-ThreadedRenderer", "False"}, {"mupen64plus-alt-map", "disabled"}, {"mupen64plus-pak1", "memory"}
    };
    else if (s.system == SystemType::GameBoyAdvance) s.options = {
        {"mgba_use_bios", "OFF"}, {"mgba_frameskip", std::to_string(config.gbaFrameskip)}
    };
    else if (s.system == SystemType::GameBoy || s.system == SystemType::GameBoyColor)
        s.options = {{"sameboy_model", "Auto"}, {"sameboy_border", "never"}};
    else if (s.system == SystemType::NintendoEntertainmentSystem) {
        // Keep original timing and sprite restrictions explicit rather than
        // inheriting a changed upstream default when the core is updated.
        s.options = {{"fceumm_region", "Auto"}, {"fceumm_nospritelimit", "disabled"},
                     {"fceumm_overclocking", "disabled"}, {"fceumm_game_genie", "disabled"}};
        // The isolated memory loader deliberately hides the on-disk name.
        // FCEUmm's iNES1 path normally guesses PAL from that name; honor the
        // unambiguous legacy PAL bit instead. NES2 owns its own timing field.
        const bool cleanPal = s.rom.size()>=16 && (s.rom[7]&0x0c)==0 && s.rom[9]==1 &&
            std::all_of(s.rom.begin()+10,s.rom.begin()+16,[](uint8_t b){return b==0;});
        if(cleanPal) s.options["fceumm_region"]="PAL";
        log.write("INFO","NES region: "+s.options["fceumm_region"]+
                  (cleanPal ? " (clean iNES PAL header)" : " (core detection)"));
    }
    else if (s.system == SystemType::SuperNintendo)
        s.options = {{"bsnes_violate_accuracy", "enabled"}, {"bsnes_chip_hle", "HLE"},
                     {"bsnes_superfx_overclock", "100%"}, {"bsnes_gamma_ramp", "disabled"},
                     {"bsnes_crop_overscan", "disabled"}, {"bsnes_region", "auto"}};
    if (s.system == SystemType::GameBoy) s.options["sameboy_mono_palette"] = GameBoyPalettes[config.gbPalette];
    if (s.gpu()) {
        s.options["mupen64plus-43screensize"] = "320x240";
        s.options["mupen64plus-EnableNativeResFactor"] = "0";
        s.options["mupen64plus-EnableShadersStorage"] = "False";
        s.options["mupen64plus-txHiresEnable"] = "False";
    }
    State::active = &s;
    if (s.api->api_version() != RETRO_API_VERSION) {
        State::active = nullptr; error = "Version libretro incompatible."; return false;
    }
    s.api->set_environment(State::environment);
    s.api->set_video_refresh(State::video);
    s.api->set_audio_sample(State::audioSample);
    s.api->set_audio_sample_batch(State::audioBatch);
    s.api->set_input_poll(State::poll);
    s.api->set_input_state(State::inputState);
    retro_system_info coreInfo{};
    s.api->get_system_info(&coreInfo);
    s.coreVersion = coreInfo.library_version ? coreInfo.library_version : "unknown";
    if (coreInfo.need_fullpath || s.coreVersion.size() >= 64 || s.identity.size() >= 64 || std::strlen(s.descriptor->id) >= 32) {
        State::active = nullptr; error = "Contrato de contenido o version de nucleo no compatible."; return false;
    }
    if (s.api->set_audio_hle) s.api->set_audio_hle(config.audioHle ? 1 : 0);
    s.api->init();
    s.initialized = true;
    if (s.api->profile_reset) s.api->profile_reset();
    if (s.api->profile_set_enabled) s.api->profile_set_enabled(config.profileCore ? 1 : 0);
    if (s.n64()) {
        log.write("INFO", config.profileCore ? "Core component profiling enabled (measurement overhead applies)" :
                  "Core component profiling disabled");
        if (s.failure.empty() && config.cpuMode == CpuMode::Automatic && s.api->dynarec_available) {
            s.jitCapable = s.api->dynarec_available() != 0;
            if (s.jitCapable) s.options["mupen64plus-cpucore"] = "dynamic_recompiler";
            else log.write("WARN", "CPU automatica: cache ejecutable no disponible; se usa interprete cacheado.");
        }
        log.write("INFO", "Inicializando Mupen64Plus-Next: " + s.options["mupen64plus-cpucore"] +
                  (s.gpu() ? (config.graphicsHle ? " / GLideN64 GLES2 320x240 / graphics HLE + CXD4 fallback" : " / GLideN64 GLES2 320x240 / CXD4") :
                  " / Angrylion " + std::to_string(config.workers) + " trabajadores, sync Low / CXD4") +
                  (config.audioHle ? " + audio HLE reconocido" : " LLE completo"));
        if (s.failure.empty() && !s.gpu() && !s.acceptedPixelFormat) s.fail("El nucleo no confirmo formato de video.");
    } else log.write("INFO", std::string("Inicializando ") + s.descriptor->name + " " + s.coreVersion);
    if (s.failure.empty()) {
        // All supported cores consume content memory. No adjacent content files
        // are opened implicitly (N64 transfer paks/disks, or cartridge sidecars).
        const retro_game_info info{nullptr, s.rom.data(), s.rom.size(), nullptr};
        s.loaded = s.api->load_game(&info);
        if (!s.loaded) s.fail(s.lastCoreError.empty() ? "El nucleo no pudo cargar la ROM." :
                             ("El nucleo no pudo cargar la ROM: " + s.lastCoreError).c_str());
    }
    if (s.loaded && s.failure.empty()) {
        s.api->set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
        for (unsigned port = 1; port < 4; ++port) s.api->set_controller_port_device(port, RETRO_DEVICE_NONE);
        retro_system_av_info info{};
        s.api->get_system_av_info(&info);
        s.timing(info);
        if (s.gpu() && s.failure.empty()) {
            std::string gpuError;
            if (!s.hardwareNegotiated) s.fail("El nucleo no negocio la salida GPU.");
            else if (!s.config.hardware->prepare(640, 480, s.hardwareCallback.depth,
                                                 s.hardwareCallback.stencil, gpuError) ||
                     !s.config.hardware->begin(gpuError)) s.fail(gpuError.c_str());
            else {
                s.hardwareReady = true;
                s.hardwareCallback.context_reset();
                if (!s.config.hardware->end(gpuError)) s.fail(gpuError.c_str());
            }
        }
        try { s.restoreSaves(); }
        catch (...) { s.fail("No hay memoria suficiente para restaurar el guardado."); }
    }
    if (!s.failure.empty()) {
        error = s.failure;
        s.saveWritable = false;
        unload();
        return false;
    }
    if (s.n64()) {
        // The pinned M64CMD_ROM_OPEN copies the cartridge; emu_step_load_data
        // frees its input before load_game returns. Other cores may borrow it.
        const auto bytes = s.rom.size();
        std::vector<unsigned char>().swap(s.rom);
        log.write("INFO", "N64: liberada copia de ROM del frontend (" + std::to_string(bytes) + " bytes).");
    }
    log.write("INFO", "ROM cargada: " + game.title + " [" + game.id + "]");
    return true;
}

bool LibretroCore::run(const GamepadInput& input, std::string& error) {
    auto& s = *state_;
    error.clear();
    if (!s.loaded || State::active != &s) { error = "No hay ROM cargada."; return false; }
    if (!s.failure.empty()) { error = s.failure; return false; }
    s.input = input;
    if (s.input.analogX == INT16_MIN) s.input.analogX = -32767;
    if (s.input.analogY == INT16_MIN) s.input.analogY = -32767;
    s.audio.clear();
    s.audioSamplesReceived = 0;
    const bool measure = s.hardwareReady && s.config.profileCore;
    const double start = measure ? hardwareClockMs() : 0.0;
    if (s.hardwareReady && !s.config.hardware->begin(error)) { s.fail(error.c_str()); return false; }
    if (measure) s.boundaryTiming.beginMs += hardwareClockMs() - start;
    s.api->run();
    const double finish = measure ? hardwareClockMs() : 0.0;
    if (s.hardwareReady && !s.config.hardware->end(error)) s.fail(error.c_str());
    if (measure) {
        s.boundaryTiming.endMs += hardwareClockMs() - finish;
        ++s.boundaryTiming.calls;
    }
    if (!s.ran) {
        s.ran = true;
        if (s.log) s.log->write("INFO", std::string("CPU activa: ") + cpuName());
    }
    if (!s.failure.empty()) { error = s.failure; return false; }
    return true;
}

void LibretroCore::unload() {
    auto& s = *state_;
    if (State::active == &s) {
        std::string gpuError;
        const bool entered = s.hardwareReady && s.config.hardware->begin(gpuError);
        if (s.hardwareReady && !entered) {
            // Core GL destructors cannot run without their context. Leave its
            // allocations to process teardown, and forbid reusing global core
            // state. A later load must not pretend this teardown succeeded.
            State::lostHardwareContext = true;
            s.loaded = s.initialized = false;
            s.fail("Se perdio el contexto GPU. Cierra y vuelve a abrir R2RETRO.");
            if (s.log) s.log->write("ERROR", s.failure);
        } else {
        if (s.loaded) {
            // N64 flushes controller paks at unload and retains its global save
            // buffer. Portable cores free memory there, so persist them first.
            if (s.n64()) s.api->unload_game();
            try { s.persistSaves(); }
            catch (...) { if (s.log) s.log->write("ERROR", "Error guardando datos de cartucho."); }
            if (!s.n64()) s.api->unload_game();
            s.loaded = false;
        }
        if (s.hardwareReady && s.hardwareCallback.context_destroy) s.hardwareCallback.context_destroy();
        if (s.initialized) { s.api->deinit(); s.initialized = false; }
        }
        if (entered) s.config.hardware->end(gpuError);
        if (!gpuError.empty() && s.log) s.log->write("ERROR", "GPU al cerrar: " + gpuError);
        if (s.api->profile_set_enabled) s.api->profile_set_enabled(0);
        State::active = nullptr;
    }
    // Include callbacks from unload/deinit, including a rejected load. Portable
    // reset stays within this session; a new load always starts fresh counters.
    s.finishLogSession();
    s.log = nullptr;
    s.audio.clear();
    s.frame = CoreFrame{};
    s.input = GamepadInput{};
    s.rom.clear();
    s.jitCapable = false;
    s.ran = false;
    s.hardwareCallback = {};
    s.hardwareNegotiated = s.hardwareReady = false;
}
bool LibretroCore::loaded() const { return state_->loaded; }
const CoreFrame& LibretroCore::frame() const { return state_->frame; }
const std::vector<int16_t>& LibretroCore::audio() const { return state_->audio; }
double LibretroCore::fps() const { return state_->fps; }
double LibretroCore::sampleRate() const { return state_->rate; }
SystemType LibretroCore::system() const { return state_->system; }
const char* LibretroCore::coreName() const { return state_->descriptor->name; }
bool LibretroCore::usingRecompiler() const {
    return state_->loaded && state_->ran && state_->failure.empty() && State::active == state_.get() &&
           state_->api->dynarec_active && state_->api->dynarec_active() != 0;
}
const char* LibretroCore::cpuName() const {
    if (!state_->loaded) return "Sin sesion";
    if (!state_->failure.empty()) return "CPU detenida";
    if (!state_->ran) return "Pendiente";
    if (state_->system == SystemType::NintendoEntertainmentSystem) return "Ricoh 2A03/2A07";
    if (state_->system == SystemType::SuperNintendo) return "Ricoh 5A22";
    if (!state_->n64()) return state_->system == SystemType::GameBoyAdvance ? "ARM7TDMI" : "SM83";
    return usingRecompiler() ? "Recompilador x64" : "Interprete cacheado";
}
uint64_t LibretroCore::audioHleTasks() const {
    return state_->loaded && State::active == state_.get() && state_->api->audio_hle_tasks ? state_->api->audio_hle_tasks() : 0;
}
R2N64CoreProfile LibretroCore::coreProfile() const {
    R2N64CoreProfile result{};
    if (state_->loaded && State::active == state_.get() && state_->api->profile_read) state_->api->profile_read(&result);
    return result;
}
uint64_t LibretroCore::graphicsHleTasks() const {
    return state_->loaded && state_->gpu() && state_->api->graphics_hle_tasks ? state_->api->graphics_hle_tasks() : 0;
}
uint64_t LibretroCore::graphicsLleTasks() const {
    return state_->loaded && state_->gpu() && state_->api->graphics_lle_tasks ? state_->api->graphics_lle_tasks() : 0;
}
HardwareTiming LibretroCore::hardwareTiming() const { return state_->boundaryTiming; }
EmulationConfig LibretroCore::effectiveConfig() const { return state_->config; }
const char* LibretroCore::profileName() const { return state_->appliedProfile; }
bool LibretroCore::supportsSaveStates() const {
    if (!state_->loaded || State::active != state_.get() || !state_->failure.empty() || !state_->ran) return false;
    // The hybrid N64 audio extension has auxiliary state outside upstream
    // serialization. Do not present incomplete snapshots as supported states.
    if (state_->n64() && (state_->config.audioHle || state_->gpu())) return false;
    const size_t size = state_->api->serialize_size();
    return size > 0 && size <= MaxStateBytes;
}
bool LibretroCore::saveState(std::string& error, unsigned slot) {
    error.clear();
    if (slot >= SaveStateSlotCount) { error = "El espacio de estado debe estar entre 1 y 5."; return false; }
    if (!supportsSaveStates()) { error = "Los estados no estan disponibles para esta sesion."; return false; }
    auto& s = *state_;
    try {
        const auto path = s.stateBase + ".slot" + std::to_string(slot) + ".state";
        const size_t size = s.api->serialize_size();
        if (!size || size > MaxStateBytes) { error = "Tamano de estado no valido."; return false; }
        std::vector<unsigned char> bytes(StateHeaderSize + size);
        if (!s.api->serialize(bytes.data() + StateHeaderSize, size)) { error = "El nucleo no pudo crear el estado."; return false; }
        const auto header = s.stateHeader(size, hashBytes(bytes.data() + StateHeaderSize, size));
        std::copy(header.begin(), header.end(), bytes.begin());
        if (!atomicWrite(path, bytes.data(), bytes.size(), error)) return false;
        s.log->write("INFO", "Estado guardado: " + path);
        return true;
    } catch (...) { error = "No hay memoria suficiente para guardar el estado."; return false; }
}
bool LibretroCore::loadState(std::string& error, unsigned slot) {
    error.clear();
    if (slot >= SaveStateSlotCount) { error = "El espacio de estado debe estar entre 1 y 5."; return false; }
    if (!supportsSaveStates()) { error = "Los estados no estan disponibles para esta sesion."; return false; }
    auto& s = *state_;
    const auto path = s.stateBase + ".slot" + std::to_string(slot) + ".state";
    const int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) { error = "No se pudo abrir el estado guardado."; return false; }
    std::vector<unsigned char> bytes;
    try {
        struct stat st{};
        const bool valid = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
            st.st_size > static_cast<off_t>(StateHeaderSize) && st.st_size <= static_cast<off_t>(StateHeaderSize + MaxStateBytes);
        if (valid) bytes.resize(static_cast<size_t>(st.st_size));
        const bool read = valid && readExact(fd, bytes.data(), bytes.size());
        ::close(fd);
        if (!read) { error = "Estado truncado o no regular."; return false; }
    } catch (...) { ::close(fd); error = "No hay memoria suficiente para leer el estado."; return false; }
    const size_t size = bytes.size() - StateHeaderSize;
    const size_t currentSize = s.api->serialize_size();
    const uint64_t hash = hashBytes(bytes.data() + StateHeaderSize, size);
    const auto expected = s.stateHeader(size, hash);
    // mGBA appends detected cartridge save data; its snapshot size can change
    // as EEPROM/SRAM autodetection resolves. The complete container remains
    // bounded, checksummed and tied to this ROM/core/version before core parsing.
    if (!currentSize || currentSize > MaxStateBytes || get64(bytes.data() + 16) != size ||
        (s.system != SystemType::GameBoyAdvance && size != currentSize) ||
        !std::equal(expected.begin(), expected.end(), bytes.begin())) {
        error = "Estado incompatible o corrupto (ROM, sistema, nucleo o version)."; return false;
    }
    try {
        // Restore only after validating the entire container. Retain a rollback
        // image because a core can reject an otherwise intact state payload.
        std::vector<unsigned char> rollback(currentSize);
        if (!s.api->serialize(rollback.data(), currentSize)) { error = "No se pudo proteger el estado actual."; return false; }
        if (!s.api->unserialize(bytes.data() + StateHeaderSize, size)) {
            if (!s.api->unserialize(rollback.data(), currentSize)) {
                s.saveWritable = false;
                s.fail("El nucleo no pudo restaurar su estado anterior; cierre la sesion.");
            }
            error = s.failure.empty() ? "El nucleo rechazo el estado; se restauro la sesion anterior." : s.failure;
            return false;
        }
        s.frame = CoreFrame{};
        s.audio.clear();
        s.input = GamepadInput{};
        if (s.api->profile_reset) s.api->profile_reset();
        s.log->write("INFO", "Estado restaurado: " + path);
        return true;
    } catch (...) { error = "No hay memoria suficiente para restaurar el estado."; return false; }
}
bool LibretroCore::setGameBoyPalette(unsigned palette, std::string& error) {
    error.clear();
    auto& s = *state_;
    if (!s.loaded || State::active != &s || !s.failure.empty()) {
        error = "No hay una sesion activa para cambiar la paleta."; return false;
    }
    if (s.system != SystemType::GameBoy) { error = "Las paletas monocromas solo se aplican a Game Boy."; return false; }
    if (palette >= GameBoyPaletteCount) { error = "La paleta de Game Boy no es valida."; return false; }
    if (palette == s.config.gbPalette) return true;
    try { s.options["sameboy_mono_palette"] = GameBoyPalettes[palette]; }
    catch (...) { error = "No hay memoria suficiente para cambiar la paleta."; return false; }
    s.config.gbPalette = palette;
    s.optionsUpdated = true;
    return true;
}
bool LibretroCore::setGbaFrameskip(unsigned frameskip, std::string& error) {
    error.clear();
    auto& s = *state_;
    if (!s.loaded || State::active != &s || !s.failure.empty() || s.system != SystemType::GameBoyAdvance) {
        error = "El salto de cuadros requiere una sesion GBA activa."; return false;
    }
    if (frameskip > 2) { error = "Salto de cuadros GBA fuera de rango (0 a 2)."; return false; }
    try { s.options["mgba_frameskip"] = std::to_string(frameskip); }
    catch (...) { error = "No hay memoria para cambiar el salto de cuadros."; return false; }
    s.config.gbaFrameskip = frameskip;
    s.optionsUpdated = true;
    return true;
}
bool LibretroCore::saveBattery(std::string& error) {
    error.clear();
    auto& s = *state_;
    if (!s.loaded || State::active != &s || !s.failure.empty() || s.n64()) {
        error = "No hay una sesion GB/GBC/GBA/NES/SNES activa para guardar SRAM/RTC."; return false;
    }
    try { return s.persistSaves(&error); }
    catch (...) { error = "No se pudo completar el guardado SRAM/RTC."; return false; }
}
bool LibretroCore::reset(std::string& error) {
    error.clear();
    auto& s = *state_;
    if (!s.loaded || State::active != &s || !s.failure.empty()) { error = "No hay una sesion activa que reiniciar."; return false; }
    if (s.n64()) {
        // Use the tested complete ROM lifecycle instead of in-coroutine JIT
        // hard reset. That path must not resume a half-initialized code cache.
        const auto path = s.romPath, root = s.dataRoot;
        Log* log = s.log;
        const auto config = s.config;
        return load(path, root, *log, error, config);
    }
    // mGBA's reset discards a temporary savedata mask created by loadState.
    // Preserve the active cartridge memory, rather than reviving its older
    // backing buffer. Keep this independent of writable filesystem storage.
    std::vector<std::pair<unsigned, std::vector<unsigned char>>> cartridgeMemory;
    try {
        for (const auto& save : s.saves) {
            const size_t size = s.api->get_memory_size(save.id);
            const auto* data = static_cast<const unsigned char*>(s.api->get_memory_data(save.id));
            if (!size) continue;
            if (!data || size > MaxSaveBytes) { error = "Memoria de cartucho no valida para reiniciar."; return false; }
            cartridgeMemory.emplace_back(save.id, std::vector<unsigned char>(data, data + size));
        }
    } catch (...) { error = "No hay memoria suficiente para proteger el guardado antes de reiniciar."; return false; }
    s.api->reset();
    for (const auto& memory : cartridgeMemory) {
        void* data = s.api->get_memory_data(memory.first);
        const size_t size = s.api->get_memory_size(memory.first);
        if (!data || size < memory.second.size()) {
            s.saveWritable = false;
            s.fail("El nucleo cambio la memoria del cartucho al reiniciar; cierre la sesion.");
            break;
        }
        std::memcpy(data, memory.second.data(), memory.second.size());
    }
    s.frame = CoreFrame{};
    s.audio.clear();
    s.input = GamepadInput{};
    if (!s.failure.empty()) { error = s.failure; return false; }
    return true;
}
}
