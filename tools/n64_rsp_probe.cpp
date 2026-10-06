// Opt-in desktop experiment, never registered in CTest or included in a PKG.
// Reuses the diagnostic loader, but reads the caller's ROM in place.
#define main original_core_probe_main
#include "../tests/core_probe.cpp"
#undef main
#include "core_profile.h"
#include <GLES2/gl2.h>
#include <chrono>

namespace {
retro_hw_render_callback callbacks{};
GLuint target{}, texture{}, depth{};
unsigned hardwareFrames{};
bool jitAvailable{};
uint64_t pcmHash = 14695981039346656037ull;
void measuredAudio(int16_t left, int16_t right) {
    for (const auto value : {left, right}) {
        const auto sample = static_cast<uint16_t>(value);
        pcmHash = (pcmHash ^ (sample & 255)) * 1099511628211ull;
        pcmHash = (pcmHash ^ (sample >> 8)) * 1099511628211ull;
    }
    audio(left, right);
}
size_t measuredBatch(const int16_t* data, size_t frames) {
    for (size_t i = 0; i < frames; ++i) measuredAudio(data[2*i], data[2*i+1]);
    return frames;
}
uintptr_t currentFramebuffer() { return target; }
retro_proc_address_t getProcedure(const char* name) {
    return reinterpret_cast<retro_proc_address_t>(SDL_GL_GetProcAddress(name));
}
bool probeEnvironment(unsigned cmd, void* data) {
    if (cmd == RETRO_ENVIRONMENT_GET_JIT_CAPABLE) {
        *static_cast<bool*>(data) = jitAvailable; return true;
    }
    if (cmd != RETRO_ENVIRONMENT_SET_HW_RENDER) return environment(cmd, data);
    auto& cb = *static_cast<retro_hw_render_callback*>(data);
    require(cb.context_type == RETRO_HW_CONTEXT_OPENGLES2 && !cb.stencil,
            "Expected GLES2 without stencil");
    require(cb.context_reset && cb.context_destroy, "Missing context callbacks");
    cb.get_current_framebuffer = currentFramebuffer;
    cb.get_proc_address = getProcedure;
    callbacks = cb;
    return true;
}
void probeVideo(const void* data, unsigned w, unsigned h, size_t) {
    if (!data) { ++duplicateFrames; return; }
    require(data == RETRO_HW_FRAME_BUFFER_VALID && w && w <= 640 && h && h <= 480,
            "Expected bounded hardware output");
    width = w; height = h; ++hardwareFrames;
    // No readback or presentation inside the measurement.
}
void capture(const std::filesystem::path& path) {
    require(width && height, "No image to capture");
    std::vector<unsigned char> raw(width * height * 4), upright(raw.size());
    GLint previous{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
    require(glGetError() == GL_NO_ERROR, "Capture failed");
    for (unsigned y = 0; y < height; ++y)
        std::memcpy(upright.data() + y * width * 4,
                    raw.data() + (height - 1 - y) * width * 4, width * 4);
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(upright.data(), width, height,
                                                            32, width * 4, SDL_PIXELFORMAT_RGBA32);
    require(surface != nullptr, SDL_GetError());
    const int result = IMG_SavePNG(surface, path.c_str());
    SDL_FreeSurface(surface);
    require(result == 0, IMG_GetError());
}
}

int main(int argc, char** argv) {
    SDL_Window* window{};
    SDL_GLContext context{};
    try {
        require(argc == 6, "Usage: n64-rsp-probe core.so ROM.z64 output cxd4|hle measured-VI");
        const std::string rsp = argv[4];
        require(rsp == "cxd4" || rsp == "hle", "Invalid RSP mode");
        const unsigned count = std::stoul(argv[5]);
        require(count >= 600 && count <= 18000, "Expected 600..18000 measured VI");
        const auto output = std::filesystem::absolute(argv[3]);
        systemDirectory = (output / "system").string();
        std::filesystem::create_directories(systemDirectory);
        std::ifstream source(argv[2], std::ios::binary);
        std::vector<char> rom((std::istreambuf_iterator<char>(source)), {});
        require(rom.size() >= 4096 && rom.size() <= 64 * 1024 * 1024 &&
                static_cast<unsigned char>(rom[0]) == 0x80 && rom[1] == 0x37,
                "Expected a big-endian .z64 ROM, at most 64 MiB");
        require(SDL_Init(SDL_INIT_VIDEO) == 0, SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        window = SDL_CreateWindow("RSP comparison", 0, 0, 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
        require(window != nullptr, SDL_GetError());
        context = SDL_GL_CreateContext(window);
        require(context != nullptr, SDL_GetError());
        std::cout << "GLES: " << glGetString(GL_VERSION) << "; " << glGetString(GL_RENDERER) << '\n';
        glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenRenderbuffers(1, &depth); glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 640, 480);
        glGenFramebuffers(1, &target); glBindFramebuffer(GL_FRAMEBUFFER, target);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Incomplete target");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        {
            Core core(argv[1]);
            jitAvailable = symbol<int(*)()>(core.library, "retro_r2n64_dynarec_available")() != 0;
            require(jitAvailable, "This comparison requires the checked x64 dynarec");
            auto profileEnable = symbol<void(*)(int)>(core.library, "retro_r2n64_profile_set_enabled");
            auto profileRead = symbol<void(*)(R2N64CoreProfile*)>(core.library, "retro_r2n64_profile_read");
            symbol<void(*)(int)>(core.library, "retro_r2n64_set_audio_hle")(1);
            options = {{"mupen64plus-rdp-plugin", "gliden64"}, {"mupen64plus-rsp-plugin", rsp},
                {"mupen64plus-cpucore", "dynamic_recompiler"}, {"mupen64plus-ThreadedRenderer", "False"},
                {"mupen64plus-alt-map", "disabled"}, {"mupen64plus-pak1", "memory"},
                {"mupen64plus-43screensize", "320x240"}, {"mupen64plus-EnableNativeResFactor", "0"},
                {"mupen64plus-EnableShadersStorage", "False"}, {"mupen64plus-txHiresEnable", "False"}};
            core.set_environment(probeEnvironment); core.set_video_refresh(probeVideo);
            core.set_audio_sample(measuredAudio); core.set_audio_sample_batch(measuredBatch);
            core.set_input_poll(poll); core.set_input_state(input);
            core.init(); core.initialized = true;
            const retro_game_info game{nullptr, rom.data(), rom.size(), nullptr};
            require(core.load_game(&game), "ROM rejected"); core.loaded = true;
            require(callbacks.context_reset != nullptr, "Core did not negotiate hardware");
            callbacks.context_reset();
            for (unsigned i = 0; i < 300; ++i) core.run();
            glFinish(); capture(output / "warmup.png");
            profileEnable(1);
            const auto start = std::chrono::steady_clock::now();
            for (unsigned i = 0; i < count; ++i) core.run();
            glFinish(); // Include completion of outstanding GPU work once at the boundary.
            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            R2N64CoreProfile profile{}; profileRead(&profile); profileEnable(0);
            require(profile.run_calls == count && !profile.timer_failures && !profile.dropped_scopes,
                    "Invalid profile sample");
            require(glGetError() == GL_NO_ERROR && hardwareFrames && nonzeroAudioFrames,
                    "Missing graphics/audio or GL error");
            capture(output / "final.png");
            std::ofstream report(output / "result.json");
            report << "{\n  \"rsp\": \"" << rsp << "\", \"vi\": " << count
                << ", \"warmup_vi\": 300, \"wall_ms_per_vi\": " << elapsed * 1000 / count
                << ", \"core_ms_per_vi\": " << double(profile.run_us) / count / 1000
                << ", \"rsp_including_gl_ms_per_vi\": " << double(profile.rsp_us) / count / 1000
                << ", \"audio_hle_ms_per_vi\": " << double(profile.audio_hle_us) / count / 1000
                << ", \"hardware_frames_total\": " << hardwareFrames
                << ", \"nonzero_audio_samples_total\": " << nonzeroAudioFrames
                << ", \"pcm_hash\": \"" << pcmHash << "\"";
            auto gfxHle = reinterpret_cast<uint64_t(*)()>(dlsym(core.library, "retro_r2n64_graphics_hle_tasks"));
            auto gfxLle = reinterpret_cast<uint64_t(*)()>(dlsym(core.library, "retro_r2n64_graphics_lle_tasks"));
            if (gfxHle && gfxLle) report << ", \"graphics_hle_tasks\": " << gfxHle()
                << ", \"graphics_lle_fallback\": " << gfxLle();
            report << "\n}\n";
            require(bool(report), "Cannot write report");
            core.unload_game(); core.loaded = false;
            callbacks.context_destroy(); core.deinit(); core.initialized = false;
            require(glGetError() == GL_NO_ERROR, "GL teardown error");
        }
        glDeleteFramebuffers(1, &target); glDeleteRenderbuffers(1, &depth); glDeleteTextures(1, &texture);
        SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        if (context) SDL_GL_DeleteContext(context);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit(); return 1;
    }
}
