// Opt-in local ROM probe. No ROM is copied, redistributed, or added to CTest.
// All generated saves/logs/images belong to a fresh isolated output directory.
#include "emulator.h"
#include "log.h"
#include <SDL.h>
#include <SDL_image.h>
#include <libretro.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace r2n64;
using Clock = std::chrono::steady_clock;
namespace {
constexpr uint64_t HashBasis = UINT64_C(14695981039346656037);
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
unsigned number(const char* text, unsigned minimum, unsigned maximum) {
    const std::string input(text);
    require(!input.empty() && input.find_first_not_of("0123456789") == std::string::npos, "Invalid integer argument");
    const unsigned long value = std::stoul(input);
    require(value >= minimum && value <= maximum, "Integer argument outside supported range");
    return static_cast<unsigned>(value);
}
std::string quoted(const std::string& text) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
        else out << c;
    }
    out << '"';
    return out.str();
}
void hashByte(uint64_t& hash, unsigned char value) {
    hash = (hash ^ value) * UINT64_C(1099511628211);
}
void hashWord(uint64_t& hash, uint32_t word) {
    for (unsigned shift = 0; shift < 32; shift += 8) hashByte(hash, static_cast<unsigned char>(word >> shift));
}
std::string hex(uint64_t value) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}
double milliseconds(Clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}
GamepadInput scriptedInput(unsigned vi, bool scripted) {
    GamepadInput input{};
    input.connected = true;
    if (!scripted) return input;
    // Fixed generic trace, counted in emulated VIs, never wall-clock seconds.
    // Wait for the startup/title transition before pressing Start. An earlier
    // pulse is legitimately ignored by games that are still booting.
    if (vi >= 600 && vi < 612) input.buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
    if (vi >= 840 && (vi - 840) % 120 < 12) input.buttons |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
    // Exercise movement and camera only after the introductory sequence.
    if (vi >= 4800) {
        input.analogY = -24000;
        input.analogX = ((vi - 4800) / 240) % 2 ? -16000 : 16000;
        if ((vi - 4800) % 360 < 120) input.rightX = 24000;
    }
    return input;
}
void png(const CoreFrame& frame, const fs::path& path) {
    require(frame.width && frame.height && !frame.pixels.empty(), "Cannot capture a missing frame");
    // RGB888 has no alpha channel: the core's X byte is commonly zero.
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uint32_t*>(frame.pixels.data()), static_cast<int>(frame.width), static_cast<int>(frame.height),
        32, static_cast<int>(frame.width * 4), SDL_PIXELFORMAT_RGB888);
    require(surface != nullptr, std::string("Cannot create capture surface: ") + SDL_GetError());
    const int status = IMG_SavePNG(surface, path.string().c_str());
    const std::string error = status == 0 ? "" : IMG_GetError();
    SDL_FreeSurface(surface);
    require(status == 0, "Cannot save PNG: " + error);
}
struct Metrics {
    std::vector<double> coreTimes;
    uint64_t audioSamples = 0, nonzeroSamples = 0, clippedSamples = 0, audioHash = HashBasis, videoHash = HashBasis;
    uint64_t lastFrameHash = HashBasis;
    unsigned audioPeak = 0, width = 0, height = 0;
    size_t nonblackPixels = 0;
    unsigned nonblackFrames = 0;
    long double audioSquares = 0;
    void add(double duration, const CoreFrame& frame, const std::vector<int16_t>& pcm) {
        coreTimes.push_back(duration);
        width = frame.width;
        height = frame.height;
        lastFrameHash = HashBasis;
        nonblackPixels = 0;
        hashWord(videoHash, width);
        hashWord(videoHash, height);
        for (const auto pixel : frame.pixels) {
            const uint32_t rgb = pixel & 0x00FFFFFF;
            nonblackPixels += rgb != 0;
            hashWord(lastFrameHash, rgb);
            hashWord(videoHash, rgb);
        }
        nonblackFrames += nonblackPixels != 0;
        audioSamples += pcm.size();
        for (const auto sample : pcm) {
            const unsigned magnitude = static_cast<unsigned>(std::abs(int(sample)));
            nonzeroSamples += magnitude != 0;
            clippedSamples += magnitude >= 32767;
            audioPeak = std::max(audioPeak, magnitude);
            audioSquares += static_cast<long double>(sample) * sample;
            const auto bits = static_cast<uint16_t>(sample);
            hashByte(audioHash, static_cast<unsigned char>(bits));
            hashByte(audioHash, static_cast<unsigned char>(bits >> 8));
        }
    }
    std::string json(unsigned session, unsigned first, unsigned last, const Emulator& core,
                     uint64_t hleStart, double wallMs) const {
        require(!coreTimes.empty(), "Cannot report an empty phase");
        auto sorted = coreTimes;
        std::sort(sorted.begin(), sorted.end());
        const double total = std::accumulate(sorted.begin(), sorted.end(), 0.0);
        const size_t p95 = static_cast<size_t>(std::ceil(sorted.size() * 0.95)) - 1;
        const auto profile = core.coreProfile();
        std::ostringstream out;
        out << std::fixed << std::setprecision(6)
            << "{\"session\":" << session << ",\"first_vi\":" << first << ",\"last_vi\":" << last
            << ",\"vi_count\":" << coreTimes.size() << ",\"nominal_vi_hz\":" << core.fps()
            << ",\"system\":" << quoted(systemId(core.system())) << ",\"core_name\":" << quoted(core.coreName())
            << ",\"actual_cpu\":" << quoted(core.cpuName()) << ",\"jit_active\":" << (core.usingRecompiler() ? "true" : "false")
            << ",\"core_ms_mean\":" << total / sorted.size() << ",\"core_ms_p95\":" << sorted[p95]
            << ",\"core_ms_max\":" << sorted.back() << ",\"core_ms_total\":" << total << ",\"wall_ms\":" << wallMs
            << ",\"audio_hle_tasks\":" << core.audioHleTasks() - hleStart
            << ",\"audio_hle_tasks_total\":" << core.audioHleTasks()
            << ",\"video_width\":" << width << ",\"video_height\":" << height
            << ",\"last_nonblack_pixels\":" << nonblackPixels << ",\"nonblack_frames\":" << nonblackFrames
            << ",\"nonblack_video_observed\":" << (nonblackFrames ? "true" : "false")
            << ",\"last_frame_fnv64\":" << quoted(hex(lastFrameHash))
            << ",\"video_sequence_fnv64\":" << quoted(hex(videoHash))
            << ",\"audio_rate_hz\":" << core.sampleRate() << ",\"audio_samples\":" << audioSamples
            << ",\"audio_stereo_frames\":" << audioSamples / 2 << ",\"audio_nonzero_samples\":" << nonzeroSamples
            << ",\"nonzero_audio_observed\":" << (nonzeroSamples ? "true" : "false")
            << ",\"audio_peak\":" << audioPeak
            << ",\"audio_clipped_samples\":" << clippedSamples
            << ",\"audio_rms\":" << (audioSamples ? std::sqrt(static_cast<double>(audioSquares / audioSamples)) : 0.0)
            << ",\"audio_fnv64\":" << quoted(hex(audioHash))
            << ",\"profile_cumulative\":{\"run_us\":" << profile.run_us << ",\"run_calls\":" << profile.run_calls
            << ",\"rsp_us\":" << profile.rsp_us << ",\"rdp_us\":" << profile.rdp_us
            << ",\"scanout_us\":" << profile.scanout_us << ",\"audio_hle_us\":" << profile.audio_hle_us
            << ",\"timer_failures\":" << profile.timer_failures << ",\"dropped_scopes\":" << profile.dropped_scopes << "}}";
        return out.str();
    }
};
struct AudioTail {
    unsigned rate = 0;
    std::vector<int16_t> samples;
    size_t count = 0, cursor = 0;
    void add(const std::vector<int16_t>& pcm, double sampleRate) {
        const auto nextRate = static_cast<unsigned>(std::lround(sampleRate));
        if (nextRate != rate) {
            require(nextRate >= 8000 && nextRate <= 192000, "Invalid WAV sample rate");
            rate = nextRate;
            samples.assign(static_cast<size_t>(rate) * 2 * 10, 0); // Last ten seconds, stereo.
            count = cursor = 0;
        }
        for (const auto sample : pcm) {
            samples[cursor] = sample;
            cursor = (cursor + 1) % samples.size();
            count = std::min(count + 1, samples.size());
        }
    }
    void save(const fs::path& path) const {
        std::ofstream file(path, std::ios::binary);
        const auto little = [&file](uint32_t value, unsigned bytes) {
            for (unsigned n = 0; n < bytes; ++n) file.put(static_cast<char>(value >> (n * 8)));
        };
        file.write("RIFF", 4); little(36 + static_cast<uint32_t>(count * 2), 4);
        file.write("WAVEfmt ", 8); little(16, 4); little(1, 2); little(2, 2);
        little(rate, 4); little(rate * 4, 4); little(4, 2); little(16, 2);
        file.write("data", 4); little(static_cast<uint32_t>(count * 2), 4);
        const size_t first = count == samples.size() ? cursor : 0;
        for (size_t n = 0; n < count; ++n) little(static_cast<uint16_t>(samples[(first + n) % samples.size()]), 2);
        file.flush();
        require(file.good(), "Cannot save PCM WAV capture");
    }
};
}

int main(int argc, char** argv) {
    fs::path run;
    try {
        require(argc >= 7 && argc <= 11,
            "Usage: rom_probe ROM output-dir cached|auto lle|hle 1|4 frames [intro|scripted] [sessions:1|2] [shared|fresh] [off|profile]");
        const fs::path rom = fs::absolute(argv[1]);
        const fs::path output = fs::absolute(argv[2]);
        const std::string cpu = argv[3], audio = argv[4], scenario = argc >= 8 ? argv[7] : "intro";
        require(cpu == "cached" || cpu == "auto", "CPU must be cached or auto");
        require(audio == "lle" || audio == "hle", "Audio must be lle or hle");
        require(scenario == "intro" || scenario == "scripted", "Scenario must be intro or scripted");
        const unsigned workers = number(argv[5], 1, 4), frames = number(argv[6], 1, 36000);
        const unsigned sessions = argc >= 9 ? number(argv[8], 1, 2) : 1;
        const std::string dataMode = argc >= 10 ? argv[9] : "shared";
        const std::string measurement = argc == 11 ? argv[10] : "off";
        require(measurement == "off" || measurement == "profile", "Measurement must be off or profile");
        require(dataMode == "shared" || dataMode == "fresh", "Session data mode must be shared or fresh");
        require(workers == 1 || workers == 4, "Workers must be 1 or 4");
        fs::create_directories(output);
        std::string pattern = (output / ("run-" + std::to_string(::getpid()) + "-XXXXXX")).string();
        std::vector<char> directory(pattern.begin(), pattern.end());
        directory.push_back(0);
        require(::mkdtemp(directory.data()) != nullptr, "Cannot create a fresh probe directory");
        run = directory.data();
        const auto data = run / "data";
        fs::create_directories(data / "logs");
        require(SDL_Init(0) == 0, std::string("SDL initialization failed: ") + SDL_GetError());
        std::cout << "{\"run_directory\":" << quoted(run.string()) << ",\"rom_read_in_place\":true}" << std::endl;
        Log log;
        require(log.open(data.string()), "Cannot open probe log");
        std::ofstream phases(run / "phases.jsonl");
        require(phases.good(), "Cannot create phase metrics");
        std::vector<std::string> summaries, captures, wavCaptures, stateChecks;
        Emulator core;
        const EmulationConfig config{workers, cpu == "auto" ? CpuMode::Automatic : CpuMode::CachedInterpreter, audio == "hle", measurement == "profile"};
        std::string error;
        for (unsigned session = 1; session <= sessions; ++session) {
            const auto sessionData = dataMode == "fresh" ? run / ("data-session-" + std::to_string(session)) : data;
            const auto loadedAt = Clock::now();
            require(core.load(rom.string(), sessionData.string(), log, error, config), "ROM load failed: " + error);
            const double loadMs = milliseconds(Clock::now() - loadedAt);
            const auto sessionStart = Clock::now();
            auto phaseStart = sessionStart;
            unsigned first = 1;
            uint64_t hleStart = core.audioHleTasks();
            Metrics phase, total;
            AudioTail audioTail;
            for (unsigned vi = 1; vi <= frames; ++vi) {
                const auto begin = Clock::now();
                require(core.run(scriptedInput(vi, scenario == "scripted"), error), "Emulation failed: " + error);
                const double coreMs = milliseconds(Clock::now() - begin);
                phase.add(coreMs, core.frame(), core.audio());
                total.add(coreMs, core.frame(), core.audio());
                audioTail.add(core.audio(), core.sampleRate());
                if ((vi == 120 || vi % 300 == 0 || vi == frames) && !core.frame().pixels.empty()) {
                    const std::string file = "session-" + std::to_string(session) + "-vi-" + std::to_string(vi) + ".png";
                    png(core.frame(), run / file);
                    captures.push_back(file);
                }
                if (vi % 120 == 0 || vi == frames) {
                    const auto result = phase.json(session, first, vi, core, hleStart, milliseconds(Clock::now() - phaseStart));
                    phases << result << '\n';
                    phases.flush();
                    require(phases.good(), "Cannot flush phase metrics");
                    std::cout << result << std::endl;
                    first = vi + 1;
                    hleStart = core.audioHleTasks();
                    phase = {};
                    phaseStart = Clock::now();
                }
            }
            summaries.push_back(total.json(session, 1, frames, core, 0, milliseconds(Clock::now() - sessionStart)));
            const std::string wav = "session-" + std::to_string(session) + "-audio-last-10s.wav";
            audioTail.save(run / wav);
            wavCaptures.push_back(wav);
            if (core.system() != SystemType::Nintendo64) {
                require(core.supportsSaveStates(), "Portable cartridge has no state support");
                require(core.saveState(error), "Portable state save failed: " + error);
                const auto replay = [&]() {
                    uint64_t digest = HashBasis;
                    for (unsigned step = 1; step <= 60; ++step) {
                        require(core.run(scriptedInput(frames + step, scenario == "scripted"), error), error);
                        for (const auto pixel : core.frame().pixels) hashWord(digest, pixel & 0xFFFFFF);
                    }
                    return digest;
                };
                const auto expectedReplay = replay();
                require(core.loadState(error), "Portable state load failed: " + error);
                require(replay() == expectedReplay, "Portable state replay changed video");
                stateChecks.push_back("{\"session\":" + std::to_string(session) +
                    ",\"replay_frames\":60,\"video_replay_fnv64\":" + quoted(hex(expectedReplay)) + ",\"passed\":true}");
            }
            std::cout << "{\"session_finished\":" << session << ",\"load_ms\":" << loadMs << '}' << std::endl;
            core.unload();
        }
        std::ofstream report(run / "report.json");
        report << "{\n  \"rom_read_in_place\": true,\n  \"rom_name\": " << quoted(rom.filename().string())
               << ",\n  \"requested_cpu\": " << quoted(cpu) << ",\n  \"requested_audio\": " << quoted(audio)
               << ",\n  \"workers\": " << workers << ",\n  \"scenario\": " << quoted(scenario)
               << ",\n  \"session_data_mode\": " << quoted(dataMode)
               << ",\n  \"component_profiling\": " << (config.profileCore ? "true" : "false")
               << ",\n  \"input_trace\": " << quoted(scenario == "intro" ? "neutral" : "Start VI 600..611; A VI 840..851 and every 120 thereafter; analog movement/camera from VI 4800")
               << ",\n  \"ps4_measured\": false,\n  \"core_timing_excludes_capture_and_hash\": true,\n  \"sessions\": [\n";
        for (size_t i = 0; i < summaries.size(); ++i) report << (i ? ",\n" : "") << "    " << summaries[i];
        report << "\n  ],\n  \"captures\": [";
        for (size_t i = 0; i < captures.size(); ++i) report << (i ? ", " : "") << quoted(captures[i]);
        report << "],\n  \"audio_captures_last_10_seconds\": [";
        for (size_t i = 0; i < wavCaptures.size(); ++i) report << (i ? ", " : "") << quoted(wavCaptures[i]);
        report << "],\n  \"portable_state_checks\": [";
        for (size_t i = 0; i < stateChecks.size(); ++i) report << (i ? ", " : "") << stateChecks[i];
        report << "]\n}\n";
        report.flush();
        require(report.good(), "Cannot save the final probe report");
        SDL_Quit();
        std::cout << "COMPLETE: local ROM probe finished; inspect image/audio metrics in " << (run / "report.json") << std::endl;
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: " << exception.what() << '\n';
        if (!run.empty()) {
            std::ofstream failure(run / "failure.json");
            failure << "{\"error\":" << quoted(exception.what()) << ",\"partial_metrics\":\"phases.jsonl\"}\n";
        }
        SDL_Quit();
        return 1;
    }
}
