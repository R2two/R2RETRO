#include "audio.h"
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        r2n64::Audio audio;
        std::string error;
        require(!audio.initialize(error), "audio must require video initialization first");
        require(!error.empty(), "initialization failure must explain its cause");
        require(SDL_Init(SDL_INIT_VIDEO) == 0, SDL_GetError());
        require(!audio.initialize(error), "a failed initialization must not retry implicitly");
        audio.shutdown();
        require(audio.initialize(error), error.c_str());
        require(!audio.start(std::numeric_limits<double>::quiet_NaN(), error), "reject an invalid core sample rate");
        require(audio.start(44100, error), error.c_str());
        audio.pause(true);
        std::vector<int16_t> samples(4410 * 2, 900);
        audio.push(samples.data(), 4410);
        const auto converted = audio.queuedFrames();
        std::printf("4410 source frames at 44.1 kHz -> %zu queued frames at 48 kHz\n", converted);
        // SDL's streaming resampler retains a short filter tail until more input.
        require(converted > 3000 && converted < 4801, "resampler must produce samples after its initial filter delay");
        audio.push(samples.data(), 4410);
        const auto nextBlock = audio.queuedFrames() - converted;
        require(nextBlock >= 4799 && nextBlock <= 4801, "44.1 kHz must resample each subsequent 100 ms to 4800 frames at 48 kHz");
        for (unsigned i = 0; i < 200; ++i) {
            audio.push(samples.data(), 4410);
            require(audio.queuedFrames() <= r2n64::Audio::MaxQueuedFrames, "queue must remain bounded when playback stalls");
        }
        audio.clear();
        require(audio.queuedFrames() == 0, "clear must discard queued sound");
        require(audio.start(48000, error), error.c_str());
        audio.pause(true);
        audio.push(samples.data(), 4410);
        require(audio.queuedFrames() == 4410, "equal sample rates must preserve frame count");
        require(audio.start(22050, error), error.c_str());
        audio.pause(true);
        require(audio.queuedFrames() == 0, "starting a new game must discard old sound");
        audio.push(samples.data(), 4410);
        require(audio.queuedFrames() > 7000, "sample rate changes must recreate the converter");
        audio.shutdown();
        require(!audio.ready() && audio.queuedFrames() == 0, "shutdown must close audio");
        audio.shutdown();
        SDL_Quit();
        std::puts("Audio: resampling, bounded queue, pause/clear and shutdown passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Audio test: %s\n", e.what());
        SDL_Quit();
        return 1;
    }
}
