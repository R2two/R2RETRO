#include "audio.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace r2n64 {
namespace {
constexpr size_t FrameBytes = sizeof(int16_t) * 2;
constexpr size_t ChunkFrames = 2048;
}
Audio::~Audio() { shutdown(); }

void Audio::fail(const char* operation) {
    error_ = std::string(operation) + ": " + SDL_GetError();
    failed_ = true;
    if (device_) SDL_PauseAudioDevice(device_, 1);
    clear();
}
bool Audio::initialize(std::string& error) {
    if (ready()) return true;
    if (attempted_) { error = error_; return false; }
    attempted_ = true;
    // On PS4 SDL video must have loaded its services before audio is requested.
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        error_ = "Inicializa SDL video antes del audio";
        error = error_;
        return false;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fail("SDL audio"); error = error_; return false;
    }
    initialized_ = true;
    SDL_AudioSpec wanted{}, obtained{};
    wanted.freq = OutputRate;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = 1024;
    // A null callback selects SDL's queued-audio API; SDL owns the audio thread.
    device_ = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
    if (!device_) {
        fail("Abrir audio 48 kHz"); error = error_;
        SDL_QuitSubSystem(SDL_INIT_AUDIO); initialized_ = false;
        return false;
    }
    if (obtained.freq != OutputRate || obtained.format != AUDIO_S16SYS || obtained.channels != 2) {
        SDL_SetError("El dispositivo no admite PCM estereo de 16 bits a 48 kHz");
        fail("Formato de audio"); error = error_;
        SDL_CloseAudioDevice(device_); device_ = 0;
        SDL_QuitSubSystem(SDL_INIT_AUDIO); initialized_ = false;
        return false;
    }
    failed_ = false;
    error_.clear(); error.clear();
    return true;
}
bool Audio::start(double sourceRate, std::string& error) {
    if (!std::isfinite(sourceRate) || sourceRate < 8000 || sourceRate > 192000) {
        error = "Frecuencia de audio del nucleo no valida";
        return false;
    }
    if (!initialize(error)) return false;
    const int rate = int(std::lround(sourceRate));
    auto* stream = SDL_NewAudioStream(AUDIO_S16SYS, 2, rate, AUDIO_S16SYS, 2, OutputRate);
    if (!stream) { fail("Convertir audio"); error = error_; return false; }
    SDL_PauseAudioDevice(device_, 1);
    clear();
    if (stream_) SDL_FreeAudioStream(stream_);
    stream_ = stream;
    sourceRate_ = rate;
    SDL_PauseAudioDevice(device_, 0);
    error.clear();
    return true;
}
void Audio::push(const int16_t* samples, size_t frames) {
    if (!ready() || !stream_ || !samples || !frames) return;
    // A stalled frontend must not accumulate unbounded sound or conversion work.
    // Keep at most the newest 250 ms from an unusually large core callback.
    const size_t maximumInput = size_t(sourceRate_) / 4;
    if (frames > maximumInput) {
        samples += (frames - maximumInput) * 2;
        frames = maximumInput;
        clear();
    }
    std::array<int16_t, ChunkFrames * 2> converted{};
    while (frames) {
        const size_t chunk = std::min(frames, ChunkFrames);
        if (SDL_AudioStreamPut(stream_, samples, int(chunk * FrameBytes)) < 0) {
            fail("Entrada de audio"); return;
        }
        samples += chunk * 2;
        frames -= chunk;
        int available;
        while ((available = SDL_AudioStreamAvailable(stream_)) > 0) {
            const int bytes = SDL_AudioStreamGet(stream_, converted.data(),
                std::min(available, int(sizeof(converted))));
            if (bytes < 0) { fail("Conversion de audio"); return; }
            if (bytes == 0) break;
            if (SDL_GetQueuedAudioSize(device_) + unsigned(bytes) > MaxQueuedFrames * FrameBytes)
                SDL_ClearQueuedAudio(device_);
            if (SDL_QueueAudio(device_, converted.data(), unsigned(bytes)) < 0) {
                fail("Cola de audio"); return;
            }
        }
        if (available < 0) { fail("Estado de audio"); return; }
    }
}
void Audio::clear() {
    if (stream_) SDL_AudioStreamClear(stream_);
    if (device_) SDL_ClearQueuedAudio(device_);
}
void Audio::pause(bool paused) {
    if (device_) SDL_PauseAudioDevice(device_, paused || failed_ ? 1 : 0);
}
size_t Audio::queuedFrames() const {
    return device_ ? SDL_GetQueuedAudioSize(device_) / FrameBytes : 0;
}
void Audio::shutdown() {
    if (device_) SDL_PauseAudioDevice(device_, 1);
    if (stream_) SDL_FreeAudioStream(stream_);
    stream_ = nullptr;
    if (device_) SDL_CloseAudioDevice(device_);
    device_ = 0;
    if (initialized_) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    initialized_ = attempted_ = failed_ = false;
    sourceRate_ = 0;
    error_.clear();
}
}
