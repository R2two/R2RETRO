#include "app.h"
#include "startup.h"
#include <cstdio>
#include <exception>
#include <cstdlib>
#include <string>
int main(int argc, char** argv) {
    r2n64::startupBegin();
    bool smoke = false;
    bool emulationSmoke = false, accelerated = false, gpuSmoke = false, n64Gpu = false;
    std::string screenshot, romSmoke, librarySmoke;
    bool libraryOffline = false, n64GraphicsHle = true, n64Auto = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--smoke") smoke = true;
        else if (arg == "--emulation-smoke") emulationSmoke = true;
        else if (arg == "--gpu-smoke") gpuSmoke = true;
        else if (arg == "--n64-gpu") { n64Gpu = true; accelerated = true; }
        else if (arg == "--n64-auto") { n64Auto = true; accelerated = true; }
        else if (arg == "--n64-gpu-lle") { n64Gpu = true; accelerated = true; n64GraphicsHle = false; }
        else if (arg == "--accelerated") accelerated = true;
#ifndef R2N64_PS4
        else if (arg == "--rom-smoke" && i + 1 < argc) { romSmoke = argv[++i]; emulationSmoke = true; }
        else if ((arg == "--library-smoke" || arg == "--library-cache-smoke") && i + 1 < argc) {
            librarySmoke = argv[++i]; libraryOffline = arg == "--library-cache-smoke";
        }
#endif
        else if (arg == "--screenshot" && i + 1 < argc) screenshot = argv[++i];
        else { std::fprintf(stderr, "Usage: r2n64 [--smoke|--emulation-smoke|--gpu-smoke] [--accelerated] [--n64-auto|--n64-gpu|--n64-gpu-lle] [--screenshot output.png]; desktop also supports --rom-smoke path with isolated R2N64_DATA\n"); return r2n64::finishApplication(2); }
    }
    int result = 1;
    if ((!romSmoke.empty() || !librarySmoke.empty()) && (!std::getenv("R2N64_DATA") || !*std::getenv("R2N64_DATA"))) {
        std::fprintf(stderr, "ROM/library smoke requires an isolated R2N64_DATA directory\n");
        return r2n64::finishApplication(2);
    }
    try { r2n64::App app; result = app.run(smoke, screenshot, emulationSmoke, accelerated, gpuSmoke, romSmoke, librarySmoke, libraryOffline, n64Gpu, n64GraphicsHle, n64Auto); }
    catch (const std::exception& e) { result = r2n64::startupFailure("Unhandled exception", e.what()); }
    return r2n64::finishApplication(result);
}
