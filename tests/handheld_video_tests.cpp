#include "video.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Linker wrappers observe real SDL operations without exposing renderer internals
// or replacing the renderer. The production calls still execute normally.
static unsigned textureCreates = 0, filterChanges = 0, surfaceTextureCreates = 0, overlayLoads = 0;
static unsigned overlayTextureCreates = 0, overlayUploads = 0, overlayReadbacks = 0;
static bool syntheticOverlays = true, failOverlayLoad = false, wrongOverlayDimensions = false;
static bool failOverlayUpload = false, failOverlayReadback = false, blankOverlayReadback = false;
static Uint8 syntheticOverlayAlpha = 255;
static bool failPngWrite = false;
static std::string lastOverlayPath;
static Uint32 lastStreamingFormat = SDL_PIXELFORMAT_UNKNOWN;
static unsigned memoryImageLoads = 0;
static unsigned gameUploads = 0;
static bool failGameUpload = false;
constexpr uint32_t overlayColor = 0x284871;
extern "C" int __real_IMG_SavePNG_RW(SDL_Surface*, SDL_RWops*, int);
extern "C" int __wrap_IMG_SavePNG_RW(SDL_Surface* surface, SDL_RWops* writer, int freeWriter) {
    if (!failPngWrite) return __real_IMG_SavePNG_RW(surface,writer,freeWriter);
    if (freeWriter) SDL_RWclose(writer);
    SDL_SetError("Synthetic PNG write failure");
    return -1;
}
extern "C" SDL_Texture* __real_SDL_CreateTexture(SDL_Renderer*, Uint32, int, int, int);
extern "C" SDL_Texture* __wrap_SDL_CreateTexture(SDL_Renderer* renderer, Uint32 format, int access, int w, int h) {
    ++textureCreates;
    if (access == SDL_TEXTUREACCESS_STREAMING) lastStreamingFormat = format;
    if (access == SDL_TEXTUREACCESS_STATIC && format == SDL_PIXELFORMAT_RGB888 && w == 1920 && h == 1080)
        ++overlayTextureCreates;
    return __real_SDL_CreateTexture(renderer, format, access, w, h);
}
extern "C" int __real_SDL_UpdateTexture(SDL_Texture*, const SDL_Rect*, const void*, int);
extern "C" int __wrap_SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect, const void* pixels, int pitch) {
    Uint32 format = 0;
    int access = 0, width = 0, height = 0;
    SDL_QueryTexture(texture,&format,&access,&width,&height);
    if (access == SDL_TEXTUREACCESS_STREAMING) {
        ++gameUploads;
        if (failGameUpload) return SDL_SetError("Synthetic game upload failure");
    }
    if (access == SDL_TEXTUREACCESS_STATIC && format == SDL_PIXELFORMAT_RGB888 && width == 1920 && height == 1080) {
        ++overlayUploads;
        if (failOverlayUpload) return SDL_SetError("Synthetic overlay upload failure");
    }
    return __real_SDL_UpdateTexture(texture,rect,pixels,pitch);
}
extern "C" int __real_SDL_RenderReadPixels(SDL_Renderer*, const SDL_Rect*, Uint32, void*, int);
extern "C" int __wrap_SDL_RenderReadPixels(SDL_Renderer* renderer, const SDL_Rect* rect, Uint32 format, void* pixels, int pitch) {
    if (rect && rect->w == 1 && rect->h == 1) {
        ++overlayReadbacks;
        if (failOverlayReadback) return SDL_SetError("Synthetic readback unavailable");
    }
    const int result = __real_SDL_RenderReadPixels(renderer,rect,format,pixels,pitch);
    if (result == 0 && rect && rect->w == 1 && rect->h == 1 && blankOverlayReadback)
        *static_cast<uint32_t*>(pixels) = 0xff000000u;
    return result;
}
extern "C" SDL_Surface* __real_IMG_Load_RW(SDL_RWops*, int);
extern "C" SDL_Surface* __wrap_IMG_Load_RW(SDL_RWops* source, int freeSource) {
    ++memoryImageLoads;
    return __real_IMG_Load_RW(source, freeSource);
}
extern "C" int __real_SDL_SetTextureScaleMode(SDL_Texture*, SDL_ScaleMode);
extern "C" int __wrap_SDL_SetTextureScaleMode(SDL_Texture* texture, SDL_ScaleMode mode) {
    ++filterChanges;
    return __real_SDL_SetTextureScaleMode(texture, mode);
}
extern "C" SDL_Texture* __real_SDL_CreateTextureFromSurface(SDL_Renderer*, SDL_Surface*);
extern "C" SDL_Texture* __wrap_SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface) {
    ++surfaceTextureCreates;
    return __real_SDL_CreateTextureFromSurface(renderer, surface);
}
extern "C" SDL_Surface* __real_IMG_Load(const char*);
extern "C" SDL_Surface* __wrap_IMG_Load(const char* path) {
    if (std::string(path).find("/overlays/") == std::string::npos) return __real_IMG_Load(path);
    ++overlayLoads;
    lastOverlayPath = path;
    if (!syntheticOverlays) return __real_IMG_Load(path);
    if (failOverlayLoad) { SDL_SetError("Synthetic missing artwork"); return nullptr; }
    auto* image = SDL_CreateRGBSurfaceWithFormat(0, wrongOverlayDimensions ? 32 : 1920,
                                                wrongOverlayDimensions ? 32 : 1080, 32, SDL_PIXELFORMAT_ARGB8888);
    if (image) SDL_FillRect(image, nullptr, (uint32_t(syntheticOverlayAlpha) << 24) | overlayColor);
    return image;
}

using namespace r2n64;
namespace fs = std::filesystem;
namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message + ": " + SDL_GetError());
}
std::vector<uint32_t> imagePixels(const fs::path& path) {
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
    Surface source(IMG_Load(path.c_str()), SDL_FreeSurface);
    require(bool(source), "Cannot load captured PNG");
    Surface image(SDL_ConvertSurfaceFormat(source.get(), SDL_PIXELFORMAT_ARGB8888, 0), SDL_FreeSurface);
    require(bool(image) && image->w == 1920 && image->h == 1080, "Unexpected output canvas");
    require(SDL_LockSurface(image.get()) == 0, "Cannot inspect capture");
    std::vector<uint32_t> pixels(1920 * 1080);
    for (int y = 0; y < 1080; ++y) {
        const auto* row = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(image->pixels) + y * image->pitch);
        for (int x = 0; x < 1920; ++x) pixels[y * 1920 + x] = row[x] & 0xffffff;
    }
    SDL_UnlockSurface(image.get());
    return pixels;
}
std::vector<uint32_t> capture(Video& video, const fs::path& path) {
    require(video.snapshot(path.string()), "Cannot capture renderer output");
    return imagePixels(path);
}
void verifyOpaque(const fs::path& path, SDL_Rect game) {
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;
    Surface source(IMG_Load(path.c_str()), SDL_FreeSurface);
    require(bool(source), "Cannot load opacity capture");
    Surface image(SDL_ConvertSurfaceFormat(source.get(), SDL_PIXELFORMAT_ARGB8888, 0), SDL_FreeSurface);
    require(bool(image) && SDL_LockSurface(image.get()) == 0, "Cannot inspect captured opacity");
    for (int y = game.y; y < game.y + game.h; ++y) {
        const auto* row = reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(image->pixels) + y * image->pitch);
        for (int x = game.x; x < game.x + game.w; ++x)
            require((row[x] >> 24) == 255, "An opaque core pixel became transparent in the composed frame");
    }
    SDL_UnlockSurface(image.get());
}
void verifyBounds(const std::vector<uint32_t>& pixels, SDL_Rect expected) {
    int minX = 1920, minY = 1080, maxX = -1, maxY = -1;
    size_t colored = 0;
    for (int y = 0; y < 1080; ++y) for (int x = 0; x < 1920; ++x) {
        if (!pixels[y * 1920 + x]) continue;
        ++colored;
        minX = std::min(minX, x); minY = std::min(minY, y);
        maxX = std::max(maxX, x); maxY = std::max(maxY, y);
    }
    require(minX == expected.x && minY == expected.y && maxX == expected.x + expected.w - 1 &&
            maxY == expected.y + expected.h - 1 && colored == size_t(expected.w) * expected.h,
            "Image bounds or black borders differ from expected aspect/scale");
}
bool inside(int x, int y, SDL_Rect rectangle) {
    return x >= rectangle.x && x < rectangle.x + rectangle.w &&
           y >= rectangle.y && y < rectangle.y + rectangle.h;
}
void verifyOverlay(const std::vector<uint32_t>& pixels, SDL_Rect aperture, SDL_Rect game) {
    require(game.x >= aperture.x && game.y >= aperture.y && game.x + game.w <= aperture.x + aperture.w &&
            game.y + game.h <= aperture.y + aperture.h, "Game rectangle escaped screen aperture");
    for (int y = 0; y < 1080; ++y) for (int x = 0; x < 1920; ++x) {
        const auto expected = inside(x,y,game) ? 0xffffffu : inside(x,y,aperture) ? 0u : overlayColor;
        require(pixels[y * 1920 + x] == expected, "Overlay artwork, game aperture or matte pixels differ");
    }
}
}

int main(int argc, char** argv) {
    try {
        bool accelerated = false, artwork = false;
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--accelerated") accelerated = true;
            else if (std::string(argv[i]) == "--artwork") artwork = true;
            else require(false, "Usage: handheld_video_tests [--accelerated] [--artwork]");
        }
        const auto directory = fs::path("previews") / (accelerated ? "handheld-video-gles2" : "handheld-video-software");
        fs::create_directories(directory);
        // An inherited SDL preference must not change our explicit default.
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        Video video;
        std::string error;
        require(video.initialize(R2N64_ASSETS, error, accelerated ? VideoBackend::Accelerated : VideoBackend::Software), error);
        // Optional visual QA uses the user-supplied images unchanged. Normal
        // CTest covers renderer behavior with wholly original synthetic artwork.
        if (artwork) {
            syntheticOverlays = false;
            require(video.preloadOverlayAssets(error),error);
            video.setAssetPath(""); // Original PNG/JPEG must decode from preserved bytes.
            for (auto system : {SystemType::GameBoy, SystemType::GameBoyColor, SystemType::GameBoyAdvance,
                                SystemType::SuperNintendo}) {
                const bool snes = system == SystemType::SuperNintendo;
                const unsigned width = snes ? 256 : system == SystemType::GameBoyAdvance ? 240 : 160;
                const unsigned height = snes ? 224 : system == SystemType::GameBoyAdvance ? 160 : 144;
                std::vector<uint32_t> pattern(width * height);
                const uint32_t colors[] = {0xffe18a,0x82cda8,0x699dcc,0x495575};
                for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x)
                    pattern[y * width + x] = (x % 16 == 0 || y % 16 == 0) ? 0x252a40 : colors[(x / 32 + y / 32) % 4];
                require(video.setHandheldOverlay(system,true,error), error);
                require(video.gameFrame(pattern.data(),width,height,error,!snes,false,!snes,system), error);
                const auto pixels = capture(video,directory / (std::string(systemId(system)) + "-artwork.png"));
                if (snes) {
                    const auto original = imagePixels(fs::path(R2N64_ASSETS) / "overlays/snes.jpg");
                    for (int y = 0; y < 1080; ++y) for (int x = 0; x < 1920; ++x) {
                        const auto pixel = pixels[y * 1920 + x];
                        if (x < 240 || x >= 1680)
                            require(pixel == original[y * 1920 + x], "SNES original side panel changed");
                        else if (!inside(x,y,{272,24,1376,1032}))
                            require(pixel == 0, "SNES original screen left a white halo outside the game");
                        else require(pixel != 0, "SNES 4:3 game aperture contains an unexpected gap");
                    }
                }
                require(video.present(error),error);
            }
            std::puts("PASS: original GB/GBC/GBA/SNES artwork loaded; SNES side panels unchanged and no white screen halo; diagnostic previews captured (no game ROM).");
            return 0;
        }
        const unsigned initialCreates = textureCreates;
        const auto frame = [&](unsigned width, unsigned height, bool integerScaling, bool nativeAspect,
                               SDL_Rect bounds, const char* name) {
            std::vector<uint32_t> source(size_t(width) * height, 0xffffff);
            require(video.gameFrame(source.data(), width, height, error, integerScaling, false, nativeAspect), error);
            verifyBounds(capture(video, directory / name), bounds);
            require(video.present(error), error);
        };
        frame(160,144,true,true,{400,36,1120,1008},"gb-integer.png");
        require(lastStreamingFormat == SDL_PIXELFORMAT_RGB888, "Core XRGB frame was uploaded as alpha-bearing artwork");
        require(textureCreates == initialCreates + 1, "First frame did not allocate exactly one streaming texture");
        const unsigned gbFilters = filterChanges;
        frame(160,144,false,true,{360,0,1200,1080},"gb-fit.png");
        require(textureCreates == initialCreates + 1 && filterChanges == gbFilters, "Scaling recreated texture or repeated unchanged filter");
        frame(240,160,true,true,{240,60,1440,960},"gba-integer.png");
        require(textureCreates == initialCreates + 2, "Changing core resolution must allocate one new texture");
        frame(240,160,false,true,{150,0,1620,1080},"gba-fit.png");
        require(textureCreates == initialCreates + 2, "GBA fit recreated texture");
        // Omitted arguments must preserve cropped N64 VI's display aspect.
        std::vector<uint32_t> croppedN64(320 * 224, 0xffffff);
        require(video.gameFrame(croppedN64.data(), 320, 224, error), error);
        verifyBounds(capture(video, directory / "n64-default.png"), {240,0,1440,1080});
        // NES/SNES use the same 4:3 television presentation by default, with
        // square pixels available explicitly through their integer-scale option.
        frame(256,240,false,false,{240,0,1440,1080},"nes-default-4by3.png");
        frame(256,240,true,false,{448,60,1024,960},"nes-square-integer.png");
        frame(256,224,false,false,{240,0,1440,1080},"snes-default-4by3.png");
        frame(512,448,true,false,{448,92,1024,896},"snes-hires-square-integer.png");
        frame(2048,2048,true,true,{420,0,1080,1080},"oversized-fit.png");

        std::vector<uint32_t> stripes(160 * 144);
        for (unsigned y = 0; y < 144; ++y) for (unsigned x = 0; x < 160; ++x)
            stripes[y * 160 + x] = x % 2 ? 0x2020ff : 0xff2020;
        require(video.gameFrame(stripes.data(), 160,144,error,true,false,true), error);
        const auto nearest = capture(video, directory / "nearest.png");
        verifyBounds(nearest, {400,36,1120,1008});
        require(std::all_of(nearest.begin(), nearest.end(), [](uint32_t pixel) {
                    return !pixel || pixel == 0x2020ff || pixel == 0xff2020;
                }), "Nearest filtering interpolated source colors");
        const unsigned stableCreates = textureCreates, nearestFilters = filterChanges;
        require(video.gameFrame(stripes.data(), 160,144,error,true,true,true), error);
        const auto linear = capture(video, directory / "bilinear.png");
        verifyBounds(linear, {400,36,1120,1008});
        const auto mixed = std::count_if(linear.begin(), linear.end(), [](uint32_t pixel) {
            return pixel && pixel != 0x2020ff && pixel != 0xff2020;
        });
        require(mixed > 1000 && linear != nearest, "Bilinear selection did not change sampled pixels");
        require(textureCreates == stableCreates && filterChanges == nearestFilters + 1, "Filter change recreated texture or did not apply once");
        for (unsigned repeat = 0; repeat < 3; ++repeat)
            require(video.gameFrame(stripes.data(), 160,144,error,true,true,true), error);
        require(textureCreates == stableCreates && filterChanges == nearestFilters + 1, "Repeated frames reset unchanged texture/filter");
        require(video.gameFrame(stripes.data(), 160,144,error,true,false,true), error);
        require(capture(video, directory / "nearest-restored.png") == nearest, "Restoring nearest did not restore exact pixels");
        auto unusedBits = stripes;
        for (size_t i = 0; i < unusedBits.size(); ++i) unusedBits[i] |= uint32_t((i * 37) & 255) << 24;
        require(video.gameFrame(unusedBits.data(),160,144,error,true,false,true),error);
        require(capture(video,directory / "xrgb-unused-bits.png") == nearest,
                "Unused XRGB bits affected opacity or RGB output");
        verifyOpaque(directory / "xrgb-unused-bits.png",{400,36,1120,1008});
        require(textureCreates == stableCreates && filterChanges == nearestFilters + 2, "Nearest restoration recreated texture");
        require(!video.gameFrame(nullptr,160,144,error) && !video.gameFrame(stripes.data(),0,144,error) &&
                !video.gameFrame(stripes.data(),2049,144,error), "Invalid frames accepted");
        require(textureCreates == stableCreates, "Invalid frame allocated a texture");
        video.releaseGameFrame();
        require(video.gameFrame(stripes.data(),160,144,error,true,true,true), error);
        require(textureCreates == stableCreates + 1 && filterChanges == nearestFilters + 3, "Recreated texture inherited stale filter cache");
        require(video.present(error), error);

        // Paused/duplicate frames recompose without retransferring pixels. The
        // same caller buffer can change; it is the explicit flag, not pointer
        // equality, that determines whether a new image must be uploaded.
        {
            video.releaseGameFrame();
            auto original = stripes;
            const auto beforeUploads = gameUploads;
            require(video.gameFrame(original.data(),160,144,error,true,false,true,SystemType::GameBoy,false),error);
            require(gameUploads == beforeUploads + 1, "First duplicate request must initialize the texture");
            require(capture(video,directory / "reuse-initial.png") == nearest, "Initial reusable texture is not the source image");
            std::fill(original.begin(),original.end(),0x20ff20);
            for (unsigned repeat = 0; repeat < 4; ++repeat)
                require(video.gameFrame(original.data(),160,144,error,true,false,true,SystemType::GameBoy,false),error);
            require(gameUploads == beforeUploads + 1, "Repeated image uploaded again");
            require(capture(video,directory / "reuse-paused.png") == nearest, "Duplicate image changed the cached texture");
            require(video.gameFrame(original.data(),160,144,error,true,true,true,SystemType::GameBoy,false),error);
            require(gameUploads == beforeUploads + 1 && capture(video,directory / "reuse-filter.png") == linear,
                    "Filter change must affect cached pixels without uploading");
            failGameUpload = true;
            require(!video.gameFrame(original.data(),160,144,error,true,false,true,SystemType::GameBoy,true),
                    "Failed new image upload reported success");
            failGameUpload = false;
            const auto afterFailure = gameUploads;
            require(video.gameFrame(original.data(),160,144,error,true,false,true,SystemType::GameBoy,false),error);
            require(gameUploads == afterFailure + 1, "Failed upload incorrectly marked texture initialized");
            const auto green = capture(video,directory / "reuse-new-pixels.png");
            require(green != nearest && green[540 * 1920 + 960] == 0x20ff20, "New pixels were not transferred");
            std::vector<uint32_t> resized(240 * 160,0xffffff);
            require(video.gameFrame(resized.data(),240,160,error,true,false,true,SystemType::GameBoyAdvance,false),error);
            require(gameUploads == afterFailure + 2, "Resize reused obsolete texture pixels");
            verifyBounds(capture(video,directory / "reuse-resized.png"),{240,60,1440,960});
            video.releaseGameFrame();
            require(video.gameFrame(original.data(),160,144,error,true,false,true,SystemType::GameBoy,false),error);
            require(gameUploads == afterFailure + 3, "New session did not initialize its texture");
        }

        const unsigned initialOverlayLoads = overlayLoads, initialOverlayTextures = overlayTextureCreates;
        failOverlayLoad = true;
        require(!video.setHandheldOverlay(SystemType::GameBoy,true,error) && !error.empty() && !video.handheldOverlayActive(),
                "Missing artwork did not fall back cleanly");
        failOverlayLoad = false;
        frame(160,144,true,true,{400,36,1120,1008},"missing-overlay.png");
        wrongOverlayDimensions = true;
        require(!video.setHandheldOverlay(SystemType::GameBoy,true,error) && !video.handheldOverlayActive(),
                "Unexpected artwork dimensions accepted");
        wrongOverlayDimensions = false;
        failOverlayUpload = true;
        require(!video.setHandheldOverlay(SystemType::GameBoy,true,error) && !video.handheldOverlayActive() &&
                error.find("Synthetic overlay upload failure") != std::string::npos,
                "GPU pixel upload failure was mistaken for a usable overlay");
        failOverlayUpload = false;
        require(video.setHandheldOverlay(SystemType::GameBoy,true,error) && video.handheldOverlayActive(),error);
        require(!video.handheldOverlayDrawn() && video.handheldOverlayDetail().find("pendiente") != std::string::npos,
                "Loading an overlay was misreported as a verified composition");
        require(overlayLoads == initialOverlayLoads + 4 && overlayTextureCreates == initialOverlayTextures + 2,
                "Invalid artwork uploaded a texture or successful retry was not cached");
        const auto overlayFrame = [&](unsigned width, unsigned height, bool integerScaling, SDL_Rect aperture,
                                      SDL_Rect bounds, const char* name, SystemType system = SystemType::Unknown) {
            std::vector<uint32_t> source(size_t(width) * height,0xffffff);
            require(video.gameFrame(source.data(),width,height,error,integerScaling,false,
                                    system != SystemType::SuperNintendo,system),error);
            verifyOverlay(capture(video,directory / name),aperture,bounds);
            require(video.present(error),error);
        };
        const unsigned beforeFramesLoads = overlayLoads, beforeFramesTextures = overlayTextureCreates;
        overlayFrame(160,144,true,{552,166,816,748},{560,180,800,720},"gb-overlay-integer.png");
        require(video.handheldOverlayDrawn() && video.handheldOverlayDetail().find("4/4 muestras visibles") != std::string::npos,
                "Composed frame pixels were not verified against source artwork");
        const auto firstReadbacks = overlayReadbacks, firstUploads = overlayUploads;
        overlayFrame(160,144,false,{552,166,816,748},{552,173,816,734},"gb-overlay-fit.png");
        require(overlayReadbacks == firstReadbacks && overlayUploads == firstUploads,
                "Ordinary frame repeated GPU readback or uploaded artwork pixels");
        require(overlayLoads == beforeFramesLoads && overlayTextureCreates == beforeFramesTextures,
                "Active game frames decoded or recreated the overlay texture");
        require(video.setHandheldOverlay(SystemType::Unknown,false,error) && !video.handheldOverlayActive(),error);
        frame(160,144,true,true,{400,36,1120,1008},"overlay-disabled.png");
        require(video.setHandheldOverlay(SystemType::GameBoy,true,error),error);
        require(overlayLoads == beforeFramesLoads && overlayTextureCreates == beforeFramesTextures,
                "Toggling a cached overlay decoded or recreated its texture");
        // A failed system switch must remove the preceding system's artwork.
        failOverlayLoad = true;
        require(!video.setHandheldOverlay(SystemType::GameBoyColor,true,error) && !video.handheldOverlayActive(),
                "Failed switch retained the preceding handheld overlay");
        failOverlayLoad = false;
        require(video.setHandheldOverlay(SystemType::GameBoyColor,true,error),error);
        overlayFrame(160,144,true,{552,166,816,748},{560,180,800,720},"gbc-overlay-integer.png");
        syntheticOverlayAlpha = 0; // artwork is opaque even if its unused alpha is zero
        require(video.setHandheldOverlay(SystemType::GameBoyAdvance,true,error),error);
        syntheticOverlayAlpha = 255;
        // A retained library clip or viewport must not clip away the GBA frame.
        const SDL_Rect staleViewport{310,106,1300,868};
        require(SDL_RenderSetViewport(video.renderer(),&staleViewport) == 0,"Cannot set inherited viewport");
        video.clip(0,0,240,160);
        overlayFrame(240,160,true,{310,106,1300,868},{360,140,1200,800},"gba-overlay-integer.png",SystemType::GameBoyAdvance);
        require(video.handheldOverlayDrawn() && video.handheldOverlayDetail().find("4/4 muestras visibles") != std::string::npos,
                "Explicit GBA system did not compose/verify its artwork after restoring canvas state");
        overlayFrame(240,160,false,{310,106,1300,868},{310,107,1300,866},"gba-overlay-fit.png",SystemType::GameBoyAdvance);
        require(video.setHandheldOverlay(SystemType::GameBoyAdvance,true,error),error);
        failOverlayReadback = true;
        overlayFrame(240,160,true,{310,106,1300,868},{360,140,1200,800},"gba-readback-unavailable.png",SystemType::GameBoyAdvance);
        failOverlayReadback = false;
        require(video.handheldOverlayActive() && video.handheldOverlayDrawn() &&
                video.handheldOverlayDetail().find("lectura no disponible") != std::string::npos,
                "Unavailable readback was confused with failed upload/draw or reported as verified");
        const auto unavailableReads = overlayReadbacks;
        overlayFrame(240,160,true,{310,106,1300,868},{360,140,1200,800},"gba-readback-not-retried.png",SystemType::GameBoyAdvance);
        require(overlayReadbacks == unavailableReads,"Unavailable readback retried on every frame");
        require(video.setHandheldOverlay(SystemType::GameBoyAdvance,true,error),error);
        blankOverlayReadback = true;
        overlayFrame(240,160,true,{310,106,1300,868},{360,140,1200,800},"gba-mismatched-readback.png",SystemType::GameBoyAdvance);
        blankOverlayReadback = false;
        require(video.handheldOverlayActive() && video.handheldOverlayDrawn() &&
                video.handheldOverlayDetail().find("0/4 muestras visibles") != std::string::npos,
                "Accepted draw calls with blank output were reported as verified artwork");
        failOverlayLoad = true;
        require(!video.setHandheldOverlay(SystemType::SuperNintendo,true,error) && !video.handheldOverlayActive(),
                "Missing SNES JPEG retained the preceding handheld overlay");
        failOverlayLoad = false;
        require(video.setHandheldOverlay(SystemType::SuperNintendo,true,error),error);
        overlayFrame(256,224,false,{240,0,1440,1080},{272,24,1376,1032},"snes-overlay-4by3.png",SystemType::SuperNintendo);
        overlayFrame(512,448,false,{240,0,1440,1080},{272,24,1376,1032},"snes-overlay-hires-4by3.png",SystemType::SuperNintendo);
        overlayFrame(512,448,true,{240,0,1440,1080},{448,92,1024,896},"snes-overlay-hires-integer.png",SystemType::SuperNintendo);
        require(overlayTextureCreates == initialOverlayTextures + 5, "Each system must cache exactly one successful artwork texture");
        const unsigned cachedLoads = overlayLoads, cachedTextures = overlayTextureCreates;
        for (auto system : {SystemType::GameBoy,SystemType::GameBoyColor,SystemType::GameBoyAdvance,SystemType::SuperNintendo})
            require(video.setHandheldOverlay(system,true,error),error);
        for (unsigned repeat = 0; repeat < 3; ++repeat)
            overlayFrame(256,224,false,{240,0,1440,1080},{272,24,1376,1032},"snes-overlay-cached.png",SystemType::SuperNintendo);
        require(overlayLoads == cachedLoads && overlayTextureCreates == cachedTextures, "Cached system switch reloaded artwork");
        require(video.gameFrame(croppedN64.data(),320,224,error),error);
        require(!video.handheldOverlayDrawn(),"An N64 frame inherited the preceding composition status");
        verifyBounds(capture(video,directory / "n64-after-overlay.png"),{240,0,1440,1080});
        require(video.gameFrame(croppedN64.data(),320,224,error,false,false,false,SystemType::Nintendo64),error);
        verifyBounds(capture(video,directory / "n64-explicit-after-snes-overlay.png"),{240,0,1440,1080});
        require(video.gameFrame(stripes.data(),160,144,error,true,false,true,SystemType::GameBoy),error);
        verifyBounds(capture(video,directory / "gb-after-snes-overlay.png"),{400,36,1120,1008});
        require(video.setHandheldOverlay(SystemType::GameBoy,true,error),error);
        std::vector<uint32_t> snesFrame(256 * 224,0xffffff);
        require(video.gameFrame(snesFrame.data(),256,224,error,false,false,false,SystemType::SuperNintendo),error);
        verifyBounds(capture(video,directory / "snes-after-gb-overlay.png"),{240,0,1440,1080});
        require(!video.setHandheldOverlay(SystemType::Nintendo64,true,error) && !video.handheldOverlayActive(),
                "An N64 game accepted a handheld overlay");
        require(video.setHandheldOverlay(SystemType::Unknown,false,error),error);
        // Production captures must use the configured system directory, retain
        // earlier images, and fail cleanly for unavailable/redirected storage.
        const auto screenshotRoot = directory / "capture-data";
        fs::create_directories(screenshotRoot);
        const auto bytes = [](const fs::path& path) {
            std::ifstream input(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(input), {});
        };
        std::string first, second;
        require(video.gameFrame(stripes.data(),160,144,error,true,false,true),error);
        require(video.saveScreenshot(screenshotRoot.string(),SystemType::GameBoy,first,error),error);
        const auto firstBytes = bytes(first);
        require(firstBytes.size() > 8 && firstBytes.substr(1,3) == "PNG", "Capture did not encode PNG");
        require(fs::path(first).parent_path() == screenshotRoot / "screenshots/gb", "Capture ignored system/configured root");
        require(fs::path(first).filename().string().find("R2RETRO-") == 0, "New capture retained the old application name");
        require(video.gameFrame(croppedN64.data(),320,224,error),error);
        require(video.saveScreenshot(screenshotRoot.string(),SystemType::GameBoy,second,error),error);
        require(first != second && bytes(first) == firstBytes && bytes(second) != firstBytes,
                "Second capture overwrote an earlier image or saved a stale frame");
        const auto entries = [](const fs::path& path) {
            return std::distance(fs::directory_iterator(path),fs::directory_iterator{});
        };
        const auto beforeFailure = entries(fs::path(first).parent_path());
        failPngWrite = true;
        require(!video.saveScreenshot(screenshotRoot.string(),SystemType::GameBoy,second,error) && second.empty() && !error.empty(),
                "PNG write failure reported success");
        failPngWrite = false;
        require(entries(fs::path(first).parent_path()) == beforeFailure, "PNG write failure left an incomplete file");
        require(!video.saveScreenshot(screenshotRoot.string(),SystemType::Unknown,second,error) && second.empty() && !error.empty(),
                "Unknown system did not fail cleanly");
        require(!video.saveScreenshot(first,SystemType::GameBoy,second,error) && second.empty(), "File accepted as capture data root");
        const auto linkedRoot = screenshotRoot / "redirected";
        fs::create_directories(linkedRoot);
        if (!fs::is_symlink(linkedRoot / "screenshots")) fs::create_directory_symlink(fs::absolute(screenshotRoot / "screenshots"),linkedRoot / "screenshots");
        require(!video.saveScreenshot(linkedRoot.string(),SystemType::GameBoy,second,error) && second.empty(),
                "Capture followed a redirected screenshots directory");
        require(bytes(first) == firstBytes, "Failed capture altered a prior file");
        // Late-loaded artwork must use the post-sandbox asset root. Updating
        // the path invalidates its cache, while fonts/background remain ready.
        const unsigned beforeRebase = overlayLoads;
        const std::string rebased = std::string(R2N64_ASSETS) + "/rebased-assets";
        video.setAssetPath(rebased);
        require(!video.handheldOverlayActive() && video.backgroundReady(), "Asset rebase reset the UI or retained an old overlay");
        require(video.setHandheldOverlay(SystemType::GameBoy,true,error),error);
        require(lastOverlayPath == rebased + "/overlays/gb.png" && overlayLoads == beforeRebase + 1,
                "Overlay did not use the resolved asset root");
        video.setAssetPath(rebased);
        require(video.setHandheldOverlay(SystemType::GameBoy,true,error) && overlayLoads == beforeRebase + 1,
                "Unchanged asset root needlessly invalidated the overlay cache");
        video.setAssetPath("");
        require(!video.setHandheldOverlay(SystemType::GameBoy,true,error) && !error.empty() &&
                !video.handheldOverlayActive() && overlayLoads == beforeRebase + 1,
                "Unresolved asset root attempted file I/O or retained an old overlay");
        require(video.gameFrame(stripes.data(),160,144,error,true,false,true,SystemType::GameBoy),error);
        require(video.backgroundReady(), "Missing late assets discarded the initial UI");
        // Preserve actual compressed bytes before losing their path, then
        // decode lazily through SDL_image. This reproduces the relevant asset
        // lifetime across a namespace transition without depending on PS4 PFS.
        const auto preloadRoot = fs::absolute(directory / "preload-assets");
        fs::create_directories(preloadRoot / "overlays");
        auto* artworkSurface = SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_ARGB8888);
        require(artworkSurface != nullptr,"Cannot allocate original test artwork");
        SDL_FillRect(artworkSurface,nullptr,0xff000000u | overlayColor);
        for (const auto* name : {"gb.png","gbc.png","gba.png","snes.jpg"})
            require(IMG_SavePNG(artworkSurface,(preloadRoot / "overlays" / name).c_str()) == 0,
                    "Cannot encode fixture artwork");
        SDL_FreeSurface(artworkSurface);
        video.setAssetPath(preloadRoot.string());
        const unsigned texturesBeforePreload = overlayTextureCreates, decodesBeforePreload = memoryImageLoads;
        require(video.preloadOverlayAssets(error),error);
        require(overlayTextureCreates == texturesBeforePreload && memoryImageLoads == decodesBeforePreload,
                "Preload decoded artwork or allocated GPU textures before first use");
        for (const auto* name : {"gb.png","gbc.png","gba.png","snes.jpg"})
            require(fs::remove(preloadRoot / "overlays" / name), "Cannot retire fixture asset path");
        video.setAssetPath("");
        const unsigned fileLoadsBeforeMemory = overlayLoads;
        for (auto system : {SystemType::GameBoy,SystemType::GameBoyColor,SystemType::GameBoyAdvance,SystemType::SuperNintendo}) {
            require(video.setHandheldOverlay(system,true,error),error);
            const bool snes = system == SystemType::SuperNintendo, gba = system == SystemType::GameBoyAdvance;
            overlayFrame(gba ? 240 : snes ? 256 : 160, gba ? 160 : snes ? 224 : 144, !snes,
                snes ? SDL_Rect{240,0,1440,1080} : gba ? SDL_Rect{310,106,1300,868} : SDL_Rect{552,166,816,748},
                snes ? SDL_Rect{272,24,1376,1032} : gba ? SDL_Rect{360,140,1200,800} : SDL_Rect{560,180,800,720},
                (std::string(systemId(system)) + "-preloaded.png").c_str(),system);
        }
        require(overlayLoads == fileLoadsBeforeMemory && memoryImageLoads == decodesBeforePreload + 4 &&
                overlayTextureCreates == texturesBeforePreload + 4, "Preloaded artwork did not decode exactly once per system");
        video.setAssetPath(preloadRoot.string() + "/no-longer-mounted");
        require(video.handheldOverlayActive(), "A new alias invalidated the preloaded texture");
        require(video.setHandheldOverlay(SystemType::SuperNintendo,false,error),error);
        require(video.setHandheldOverlay(SystemType::SuperNintendo,true,error),error);
        require(overlayLoads == fileLoadsBeforeMemory && memoryImageLoads == decodesBeforePreload + 4 &&
                overlayTextureCreates == texturesBeforePreload + 4, "Toggling preloaded artwork reread or decoded its image");
        std::printf("PASS: GB/GBA native fit and integer bounds, default N64 aspect, nearest/bilinear pixels, texture/filter reuse (%s).\n",
                    accelerated ? "GLES2" : "software");
        std::puts("PASS: GB/GBC/GBA/SNES overlays, safe apertures, cached toggles, no per-frame loads, missing/invalid artwork fallback, no stale system artwork.");
        return 0;
    } catch (const std::exception& exception) {
        std::fprintf(stderr,"FAIL: %s\n",exception.what()); return 1;
    }
}
