#pragma once
#include <string>

struct SDL_Window;
struct SDL_Renderer;

namespace r2n64 {
// Manual capability check only: this does not enable an N64 GPU renderer.
struct GpuProbeResult {
    bool supported = false;
    bool passed = false;
    bool contextCreated = false;
    bool shaderCompiler = false;
    bool vertexCompiled = false;
    bool fragmentCompiled = false;
    bool programLinked = false;
    bool framebufferComplete = false;
    bool pixelMatched = false;
    bool contextRestored = false;
    std::string status;
    std::string vendor;
    std::string renderer;
    std::string version;
    std::string shadingLanguage;
    std::string log;
};

// Call on the SDL render thread while no emulation session is running.
// The probe uses an unshared temporary GLES2 context and never presents it.
GpuProbeResult runGpuProbe(SDL_Window* window, SDL_Renderer* renderer);
}
