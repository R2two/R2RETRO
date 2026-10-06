// Desktop-only real GLES2/libretro lifecycle test. No commercial ROMs.
// Reuse the existing dynamic core loader and ordinary libretro callbacks.
#define main software_core_probe_entry
#include "core_probe.cpp"
#undef main
#include <GLES2/gl2.h>

namespace {
retro_hw_render_callback gpuCallbacks{};
GLuint hostFramebuffer{}, hostTexture{}, hostDepth{};
bool refuseHardware{};
unsigned gpuFrames{};
std::vector<uint8_t> gpuPixels;
uintptr_t framebuffer() { return hostFramebuffer; }
retro_proc_address_t procedure(const char* name) {
    return reinterpret_cast<retro_proc_address_t>(SDL_GL_GetProcAddress(name));
}
bool gpuEnvironment(unsigned cmd, void* data) {
    if (cmd != RETRO_ENVIRONMENT_SET_HW_RENDER) return environment(cmd, data);
    ++hardwareRequests;
    if (refuseHardware) return false;
    auto& cb = *static_cast<retro_hw_render_callback*>(data);
    require(cb.context_type == RETRO_HW_CONTEXT_OPENGLES2, "Core did not request GLES2");
    require(cb.depth && !cb.stencil && cb.bottom_left_origin && cb.cache_context,
            "Unexpected GLES2 context requirements");
    require(cb.context_reset && cb.context_destroy, "Missing context lifecycle callbacks");
    cb.get_current_framebuffer = framebuffer;
    cb.get_proc_address = procedure;
    gpuCallbacks = cb;
    return true;
}
void gpuVideo(const void* data, unsigned w, unsigned h, size_t pitch) {
    if (data != RETRO_HW_FRAME_BUFFER_VALID) { video(data, w, h, pitch); return; }
    require(w && w <= 640 && h && h <= 480, "GPU video exceeds the host target");
    require(glGetError() == GL_NO_ERROR, "Core issued an invalid GL operation before video callback");
    width = w; height = h;
    gpuPixels.resize(static_cast<size_t>(w) * h * 4);
    GLint previous{};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    glBindFramebuffer(GL_FRAMEBUFFER, hostFramebuffer);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Core damaged host FBO");
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, gpuPixels.data());
    require(glGetError() == GL_NO_ERROR, "GPU readback failed");
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
    ++gpuFrames;
}
void requireBands() {
    unsigned red{}, green{}, blue{};
    for (size_t i = 0; i + 3 < gpuPixels.size(); i += 4) {
        red += gpuPixels[i] > 160 && gpuPixels[i + 1] < 80 && gpuPixels[i + 2] < 80;
        green += gpuPixels[i + 1] > 160 && gpuPixels[i] < 80 && gpuPixels[i + 2] < 80;
        blue += gpuPixels[i + 2] > 160 && gpuPixels[i] < 80 && gpuPixels[i + 1] < 80;
    }
    std::cout << "GPU bands: " << red << '/' << green << '/' << blue << '\n';
    require(red > 1000 && green > 1000 && blue > 1000, "Actual GPU output lacks diagnostic RGB bands");
}
void configure(Core& core, bool gpu) {
    options = {
        {"mupen64plus-rdp-plugin", gpu ? "gliden64" : "angrylion"},
        {"mupen64plus-rsp-plugin", "cxd4"},
        {"mupen64plus-cpucore", "cached_interpreter"},
        {"mupen64plus-angrylion-multithread", "1"},
        {"mupen64plus-angrylion-vioverlay", "Unfiltered"},
        {"mupen64plus-ThreadedRenderer", "False"},
        {"mupen64plus-aspect", "4:3"},
        {"mupen64plus-43screensize", "320x240"},
        {"mupen64plus-EnableNativeResFactor", "1"},
    };
    core.set_environment(gpuEnvironment);
    core.set_video_refresh(gpuVideo);
    core.set_audio_sample(audio);
    core.set_audio_sample_batch(audioBatch);
    core.set_input_poll(poll);
    core.set_input_state(input);
    core.init(); core.initialized = true;
}
void finish(Core& core, bool resetWasCalled) {
    core.unload_game(); core.loaded = false;
    if (resetWasCalled) gpuCallbacks.context_destroy();
    core.deinit(); core.initialized = false;
    gpuCallbacks = {};
}
}
int main(int argc, char** argv) {
    SDL_Window* window{};
    SDL_GLContext context{};
    try {
        require(argc == 4, "Usage: core_gpu_probe core.so rdp-workload.z64 output-directory");
        std::filesystem::create_directories(std::filesystem::absolute(argv[3]) / "system");
        systemDirectory = (std::filesystem::absolute(argv[3]) / "system").string();
        require(SDL_Init(SDL_INIT_VIDEO) == 0, SDL_GetError());
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        window = SDL_CreateWindow("R2RETRO core GPU test", 0, 0, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        require(window != nullptr, SDL_GetError());
        context = SDL_GL_CreateContext(window);
        require(context != nullptr, SDL_GetError());
        std::cout << "GLES: " << glGetString(GL_VERSION) << "; " << glGetString(GL_RENDERER) << '\n';
        glGenTextures(1, &hostTexture);
        glBindTexture(GL_TEXTURE_2D, hostTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenRenderbuffers(1, &hostDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, hostDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 640, 480);
        glGenFramebuffers(1, &hostFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, hostFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hostTexture, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, hostDepth);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Host FBO incomplete");
        require(glGetError() == GL_NO_ERROR, "Host FBO setup GL error");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        std::ifstream source(argv[2], std::ios::binary);
        std::vector<char> rom((std::istreambuf_iterator<char>(source)), {});
        require(rom.size() == 4096, "Expected original 4 KiB RDP diagnostic");
        retro_game_info game{argv[2], rom.data(), rom.size(), nullptr};
        {
            Core core(argv[1]);
            // Refusal must not open a ROM or prevent a fresh software session.
            configure(core, true);
            refuseHardware = true;
            require(!core.load_game(&game), "Refused context was accepted");
            core.deinit(); core.initialized = false;
            refuseHardware = false;
            configure(core, false);
            require(core.load_game(&game), "Software fallback load failed"); core.loaded = true;
            for (unsigned i = 0; i < 12; ++i) core.run();
            require(!invalidVideo && videoFrames > 0 && ramWord(core, 0x400) == 0x52324450,
                    "Software fallback did not execute");
            finish(core, false);
            // The frontend may abort preparation after negotiation, before reset.
            configure(core, true);
            require(core.load_game(&game), "Aborted GPU preparation load failed"); core.loaded = true;
            finish(core, false);
            std::vector<uint8_t> previous;
            for (unsigned session = 0; session < 3; ++session) {
                configure(core, true);
                require(core.load_game(&game), "GPU load failed"); core.loaded = true;
                gpuCallbacks.context_reset();
                const auto before = gpuFrames;
                for (unsigned i = 0; i < 30; ++i) core.run();
                require(gpuFrames > before && ramWord(core, 0x400) == 0x52324450,
                        "GLideN64 failed to execute/produce hardware callbacks");
                requireBands();
                if (session) require(gpuPixels == previous, "Repeated GPU session changed original fixture pixels");
                previous = gpuPixels;
                require(glGetError() == GL_NO_ERROR, "Core left a GL error");
                finish(core, true);
                require(glGetError() == GL_NO_ERROR, "GPU teardown left a GL error");
            }
            configure(core, false);
            require(core.load_game(&game), "GPU-to-software load failed"); core.loaded = true;
            const auto before = videoFrames;
            for (unsigned i = 0; i < 12; ++i) core.run();
            require(videoFrames > before && !invalidVideo, "GPU-to-software video transition failed");
            finish(core, false);
        }
        glDeleteFramebuffers(1, &hostFramebuffer);
        glDeleteRenderbuffers(1, &hostDepth);
        glDeleteTextures(1, &hostTexture);
        SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
        std::cout << "PASS: actual GLES2/CXD4 RDP drawing, repeated sessions, aborted preparation and software fallback.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        if (context) SDL_GL_DeleteContext(context);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
}
