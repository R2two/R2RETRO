// Production bridge + shared SDL host + pinned core, original RDP fixture only.
#include "emulator.h"
#include "gpu_session.h"
#include "log.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace r2n64;
static void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    try {
        require(SDL_Init(SDL_INIT_VIDEO) == 0, SDL_GetError());
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
        window = SDL_CreateWindow("GPU core contract", 0, 0, 320, 240, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
        require(window, SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
        require(renderer, SDL_GetError());
        std::filesystem::create_directories(argv[2]);
        std::filesystem::create_directories(std::filesystem::path(argv[2]) / "logs");
        Log log; require(log.open(argv[2]), "Cannot open isolated log");
        {
            GpuSession host(window, renderer);
            Emulator core; // Destroy core before its borrowed host.
            EmulationConfig config;
            config.graphics = N64Graphics::Gles2; config.hardware = &host;
            config.cpuMode = CpuMode::CachedInterpreter;
            std::string error;
            std::vector<uint32_t> previous;
            for (unsigned session = 0; session < 3; ++session) {
                require(core.load(argv[1], argv[2], log, error, config), error);
                if (session == 1) require(core.reset(error), error);
                for (unsigned frame = 0; frame < 40; ++frame) {
                    require(core.run(GamepadInput{}, error), error);
                    const auto& output = core.frame();
                    if (!output.hardware) continue;
                    require(output.pixels.empty(), "GPU copied a CPU framebuffer");
                    require(SDL_SetRenderDrawColor(renderer,0,0,0,255) == 0 && SDL_RenderClear(renderer) == 0, SDL_GetError());
                    require(host.draw(output.width,output.height,output.bottomLeftOrigin,SDL_Rect{0,0,320,240},error),error);
                    // Make SDL change program/buffer/blend state between core frames.
                    require(SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND)==0,SDL_GetError());
                    require(SDL_SetRenderDrawColor(renderer,255,255,255,160)==0,SDL_GetError());
                    const SDL_Rect marker{0,0,2,2};
                    require(SDL_RenderFillRect(renderer,&marker)==0,SDL_GetError());
                    require(SDL_RenderFlush(renderer)==0,SDL_GetError());
                }
                require(core.frame().hardware,"No hardware frame from actual core");
                std::vector<uint32_t> pixels(320*240);
                require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGB888,pixels.data(),320*4)==0,SDL_GetError());
                unsigned red=0,green=0,blue=0;
                for (const auto pixel:pixels) {
                    const unsigned r=(pixel>>16)&255,g=(pixel>>8)&255,b=pixel&255;
                    red += r>160 && g<80 && b<80;
                    green += g>160 && r<80 && b<80;
                    blue += b>160 && r<80 && g<80;
                }
                require(red>18000 && green>18000 && blue>18000,"GPU output lost RGB bands during SDL composition");
                if (session) require(pixels==previous,"Repeated GPU session/reset changed pixels");
                previous=std::move(pixels);
                core.unload(); host.reset();
                require(SDL_SetRenderDrawColor(renderer,23,41,67,255)==0 && SDL_RenderClear(renderer)==0,SDL_GetError());
                uint32_t pixel{}; const SDL_Rect point{10,10,1,1};
                require(SDL_RenderReadPixels(renderer,&point,SDL_PIXELFORMAT_RGB888,&pixel,4)==0 &&
                        (pixel&0xffffff)==0x172943,"Returning to SDL after GPU teardown failed");
            }
            config.graphics=N64Graphics::Software; config.hardware=nullptr;
            require(core.load(argv[1],argv[2],log,error,config),error);
            for (unsigned i=0;i<20;++i) require(core.run(GamepadInput{},error),error);
            require(!core.frame().hardware && !core.frame().pixels.empty(),"GPU-to-software session failed");
            core.unload();
        }
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
        std::cout << "PASS: production GPU bridge/host/core, RGB composition, reset/reload, SDL return and software transition.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit(); return 1;
    }
}
