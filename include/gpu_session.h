#pragma once
#include "core/hardware_video.h"
#include <memory>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Rect;

namespace r2n64 {
// Optional lightweight display masks for software-frame cores. Draw after SDL
// copies the game, before UI; no readback, texture conversion or core GL calls.
class DisplayShader final {
public:
    DisplayShader(SDL_Window* window, SDL_Renderer* renderer);
    ~DisplayShader();
    bool select(unsigned mode, std::string& error);
    bool draw(unsigned width, unsigned height, const SDL_Rect& destination, std::string& error);
    unsigned mode() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// One optional core session, on the SDL render thread. Borrows the window and
// renderer; reset/destroy this before destroying either SDL object. No second
// GL context is created. begin/end surround every core operation that uses GL.
class GpuSession final : public HardwareVideoHost {
public:
    GpuSession(SDL_Window* window, SDL_Renderer* renderer);
    ~GpuSession() override;
    GpuSession(const GpuSession&) = delete;
    GpuSession& operator=(const GpuSession&) = delete;
    bool prepare(unsigned width, unsigned height, bool depth, bool stencil,
                 std::string& error) override;
    bool begin(std::string& error) override;
    bool end(std::string& error) override;
    uintptr_t framebuffer() const override;
    Procedure procedure(const char* name) override;
    bool draw(unsigned width, unsigned height, bool bottomLeftOrigin,
              const SDL_Rect& destination, std::string& error);
    bool draw(unsigned width, unsigned height, bool bottomLeftOrigin, std::string& error);
    bool ready() const;
    std::string driverDescription() const;
    void reset();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
