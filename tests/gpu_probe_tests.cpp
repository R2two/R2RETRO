#include "gpu_probe.h"
#include <SDL2/SDL.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string(message) + ": " + SDL_GetError());
}
void drawAndCheck(SDL_Renderer* renderer, SDL_Texture* texture) {
    require(SDL_SetRenderDrawColor(renderer, 0, 210, 0, 255) == 0, "draw color");
    require(SDL_RenderClear(renderer) == 0, "clear after probe");
    SDL_Rect rectangle{16, 16, 32, 32};
    require(SDL_RenderCopy(renderer, texture, nullptr, &rectangle) == 0, "old texture after probe");
    std::array<uint32_t, 64 * 64> pixels{};
    require(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                                pixels.data(), 64 * 4) == 0, "SDL readback after probe");
    require((pixels[32 * 64 + 32] & 0xFFFFFF) == 0xC80000, "existing texture survived isolated context");
    require((pixels[2 * 64 + 2] & 0xFFFFFF) == 0x00D200, "SDL clear survived isolated context");
    SDL_RenderPresent(renderer);
}
}

int main(int argc, char** argv) {
    const bool software = argc > 1 && std::string(argv[1]) == "--software";
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    try {
        const auto unavailable = r2n64::runGpuProbe(nullptr, nullptr);
        require(!unavailable.passed && !unavailable.supported && unavailable.contextRestored,
                "null input must be handled without changing context");
        require(SDL_Init(SDL_INIT_VIDEO) == 0, "SDL video");
        window = SDL_CreateWindow("GPU capability test", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
        require(window != nullptr, "window");
        renderer = SDL_CreateRenderer(window, -1, software ? SDL_RENDERER_SOFTWARE :
                                      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        require(renderer != nullptr, "renderer");
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 2, 2);
        require(texture != nullptr, "texture");
        const std::array<uint32_t, 4> red{{0xFFC80000,0xFFC80000,0xFFC80000,0xFFC80000}};
        require(SDL_UpdateTexture(texture, nullptr, red.data(), 2 * 4) == 0, "texture contents");
        drawAndCheck(renderer, texture);
        // A caller's sharing preference must not make this probe's resources
        // shared with SDL, and the preference must survive the diagnostic.
        if (!software) require(SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1) == 0, "sharing preference");
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            const SDL_GLContext original = SDL_GL_GetCurrentContext();
            const int originalSwapInterval = original ? SDL_GL_GetSwapInterval() : 0;
            const auto result = r2n64::runGpuProbe(window, renderer);
            std::printf("Probe %u: %s\n%s", attempt, result.status.c_str(), result.log.c_str());
            require(result.contextRestored, "context must restore");
            require(SDL_GL_GetCurrentContext() == original, "original context identity must restore");
            require(!original || SDL_GL_GetSwapInterval() == originalSwapInterval, "swap interval must restore");
            if (!software) {
                int sharing = 0;
                require(SDL_GL_GetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, &sharing) == 0 && sharing == 1,
                        "context sharing preference must restore");
            }
            require(result.log.size() <= 8192, "bounded log");
            if (software) {
                require(!result.passed && !result.supported && !result.contextCreated, "software path must decline gracefully");
            } else {
                require(result.supported && result.passed && result.contextCreated && result.shaderCompiler &&
                        result.vertexCompiled && result.fragmentCompiled && result.programLinked &&
                        result.framebufferComplete && result.pixelMatched,
                        "Mesa GLES2 must compile original shaders and render expected framebuffer");
                require(!result.vendor.empty() && !result.version.empty() && !result.renderer.empty(), "driver identity");
            }
            drawAndCheck(renderer, texture);
        }
        SDL_DestroyTexture(texture); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        std::puts("PASS: optional GPU diagnostic and repeated SDL restoration.");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
}
