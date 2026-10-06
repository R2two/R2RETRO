#include "renderer.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
float number(const std::string& value) {
    std::size_t used = 0;
    const float result = std::stof(value, &used);
    if (used != value.size() || !std::isfinite(result)) throw std::runtime_error("Invalid numeric argument");
    return result;
}
void usage() {
    std::cout << "pokemon3d --scene FILE --screenshot PNG [--yaw DEGREES] [--pitch DEGREES] [--zoom 0.25..4] [--interactive]\n"
        "Native research viewer, independent of the emulator and PS4 package.\n"
        "Interactive: Left/Right orbit; Up/Down elevation; mouse wheel zoom; Esc exits.\n";
}
}
int main(int argc, char** argv) {
    try {
        std::string scenePath, screenshot;
        pokemon3d::Camera camera;
        bool interactive = false;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") { usage(); return 0; }
            if (arg == "--interactive") { interactive = true; continue; }
            if ((arg == "--scene" || arg == "--screenshot" || arg == "--yaw" || arg == "--pitch" || arg == "--zoom") && i + 1 < argc) {
                const std::string value = argv[++i];
                if (arg == "--scene") scenePath = value;
                else if (arg == "--screenshot") screenshot = value;
                else if (arg == "--yaw") camera.yaw = number(value);
                else if (arg == "--zoom") camera.zoom = number(value);
                else camera.pitch = number(value);
            } else throw std::runtime_error("Unknown or incomplete argument: " + arg);
        }
        if (scenePath.empty() || (!interactive && screenshot.empty())) { usage(); return 2; }
        if (!interactive && !SDL_getenv("SDL_VIDEODRIVER")) SDL_setenv("SDL_VIDEODRIVER", "offscreen", 0);
        pokemon3d::Scene scene;
        std::string error;
        if (!pokemon3d::loadScene(scenePath, scene, error)) throw std::runtime_error(error);
        pokemon3d::Renderer renderer;
        if (!renderer.initialize(interactive, error) || !renderer.upload(scene, error) || !renderer.draw(camera, error))
            throw std::runtime_error(error);
        std::cout << "Vertices: " << scene.vertices.size() << "; depth: " << renderer.depthBits()
            << " bits; GLES: " << renderer.deviceInfo() << '\n';
        if (!screenshot.empty()) {
            if (!renderer.savePng(screenshot, error)) throw std::runtime_error(error);
            std::cout << "Capture: " << screenshot << '\n';
        }
        if (!interactive) return 0;
        renderer.present();
        bool running = true;
        Uint64 last = SDL_GetPerformanceCounter();
        const double frequency = double(SDL_GetPerformanceFrequency());
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) running = false;
                if (event.type == SDL_MOUSEWHEEL) camera.zoom = std::clamp(camera.zoom*std::pow(1.1f, float(event.wheel.y)), 0.25f, 4.f);
            }
            const Uint64 now = SDL_GetPerformanceCounter();
            const float step = float(std::min(0.1, double(now-last)/frequency))*70.f;
            last = now;
            const auto* keys = SDL_GetKeyboardState(nullptr);
            camera.yaw += step*(int(keys[SDL_SCANCODE_RIGHT])-int(keys[SDL_SCANCODE_LEFT]));
            camera.pitch = std::clamp(camera.pitch+step*(int(keys[SDL_SCANCODE_UP])-int(keys[SDL_SCANCODE_DOWN])), -85.f, 85.f);
            if (!running) break;
            if (!renderer.draw(camera, error)) throw std::runtime_error(error);
            renderer.present();
            SDL_Delay(1);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "pokemon3d: " << e.what() << '\n';
        return 1;
    }
}
