#pragma once
#include "scene.h"
#include "red_state.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pokemon3d {
struct Camera {
    float yaw = 35.f, pitch = 50.f, zoom = 1.f;
    bool follow = false;
    float targetX = 0.f, targetZ = 0.f;
    bool cutaway = true;
};
class Renderer {
public:
    static constexpr int Width = 1280, Height = 960;
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool initialize(bool visible, std::string& error, bool synchronize = true);
    bool upload(const Scene& scene, std::string& error);
    bool uploadSprites(const SpriteAtlas& atlas, std::string& error);
    bool draw(const Camera& camera, std::string& error);
    // Small dynamic marker buffer; the static map VBO stays unchanged.
    bool drawActors(const std::vector<Actor>& actors, std::string& error);
    // Original emulator image: complete fallback or a fixed inset in 3D.
    bool drawGameFrame(const std::uint32_t* pixels, unsigned width, unsigned height,
                       bool inset, std::string& error);
    // Complete original game frame beside the 3D world while menus are active.
    bool drawGameOverlay(const std::uint32_t* pixels, unsigned width, unsigned height,
                         std::string& error);
    // Explicit capture only; the interactive loop never reads GPU pixels.
    bool readPixels(std::vector<std::uint8_t>& rgba, std::string& error);
    bool savePng(const std::string& path, std::string& error);
    // Indicates the chosen presentation without altering the emulated game.
    void setPresentationEnabled(bool enabled);
    void present();
    unsigned depthBits() const;
    const std::string& deviceInfo() const;
private:
    bool drawImage(const std::uint32_t* pixels, unsigned width, unsigned height,
                   int mode, std::string& error);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
