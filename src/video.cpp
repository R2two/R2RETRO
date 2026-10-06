#include "video.h"
#include "startup.h"
#include "asset_path.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace r2n64 {
namespace {
struct HandheldOverlay {
    SystemType system;
    const char* filename;
    SDL_Rect aperture;
    SDL_Rect matte;
};
constexpr HandheldOverlay handheldOverlays[] = {
    {SystemType::GameBoy, "gb.png", {552,166,816,748}, {552,166,816,748}},
    {SystemType::GameBoyColor, "gbc.png", {552,166,816,748}, {552,166,816,748}},
    {SystemType::GameBoyAdvance, "gba.png", {310,106,1300,868}, {310,106,1300,868}},
    // The JPEG's curved white screen and its halo extend past the safe game
    // aperture. Cover the central screen only; preserve both original panels.
    {SystemType::SuperNintendo, "snes.jpg", {260,24,1400,1032}, {240,0,1440,1080}}
};
// Well inside the opaque artwork, away from game apertures and screen edges.
constexpr SDL_Point overlaySamplePoints[] = {{96,96},{1824,96},{96,984},{1824,984}};
}
GpuProbeResult Video::probeGpu() { return runGpuProbe(window_, renderer_); }
Video::~Video() {
    releaseGameFrame();
    clearLibraryArtwork();
    for (auto& entry : texts_) SDL_DestroyTexture(entry.second.image);
    for (auto& entry : fonts_) TTF_CloseFont(entry.second);
    for (auto* texture : backgrounds_) if (texture) SDL_DestroyTexture(texture);
    for (auto* texture : overlays_) if (texture) SDL_DestroyTexture(texture);
    if (background_) SDL_FreeSurface(background_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    if (images_) IMG_Quit();
    if (ttf_) TTF_Quit();
    if (initialized_) SDL_Quit();
}
bool Video::initialize(const std::string& assets, std::string& error, VideoBackend backend) {
    assets_ = assets;
    uint32_t flags = SDL_INIT_VIDEO;
#ifndef R2N64_PS4
    flags |= SDL_INIT_GAMECONTROLLER;
#endif
    startupLog("SDL_Init begin");
    if (SDL_Init(flags) < 0) { error = SDL_GetError(); return false; }
    initialized_ = true;
    startupLog("SDL_Init OK; creating window");
    window_ = SDL_CreateWindow("R2RETRO | XMB", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, 0);
    if (!window_) { error = SDL_GetError(); return false; }
    startupLog("Window OK; creating renderer");
    if (backend == VideoBackend::Accelerated) {
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    } else {
        surface_ = SDL_GetWindowSurface(window_);
        if (!surface_) { error = SDL_GetError(); return false; }
        renderer_ = SDL_CreateSoftwareRenderer(surface_);
    }
    if (!renderer_) { error = SDL_GetError(); return false; }
    SDL_RendererInfo rendererInfo{};
    SDL_GetRendererInfo(renderer_, &rendererInfo);
    startupLog("Renderer OK", rendererInfo.name);
    if (SDL_RenderSetLogicalSize(renderer_, 1920, 1080) < 0) { error = SDL_GetError(); return false; }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    startupLog("TTF_Init begin");
    if (TTF_Init() < 0) { error = TTF_GetError(); return false; }
    ttf_ = true;
    startupLog("IMG_Init begin");
    const int imageFlags = IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
    images_ = true;
    if (!(imageFlags & IMG_INIT_JPG)) { error = IMG_GetError(); return false; }
    startupLog("Opening fonts");
    const auto font = assets + "/fonts/DejaVuSans.ttf";
    for (int size : {20, 24, 28, 34, 42, 48}) {
        fonts_[size] = TTF_OpenFont(font.c_str(), size);
        if (!fonts_[size]) { error = TTF_GetError(); return false; }
    }
    startupLog("Loading background.jpg");
    if (!loadBackground(assets + "/background.jpg"))
        warning_ = "No se pudo cargar el fondo: " + std::string(IMG_GetError());
    startupLog("Video assets ready", warning_.empty() ? "OK" : warning_.c_str());
    return true;
}
void Video::setAssetPath(const std::string& assets) {
    if (assets_ == assets) return;
    for (size_t i = 0; i < overlays_.size(); ++i) {
        // These bytes identify the bundled artwork loaded before GoldHEN.
        // Changing its filesystem alias must not invalidate a usable cache.
        if (!overlayBytes_[i].empty()) continue;
        auto*& texture = overlays_[i];
        if (texture) SDL_DestroyTexture(texture);
        texture = nullptr;
        if (overlayIndex_ == static_cast<int>(i)) {
            overlayIndex_ = -1; overlayDrawn_ = false; overlayCheckPending_ = false;
            overlayDetail_.clear();
        }
    }
    assets_ = assets;
}
bool Video::preloadOverlayAssets(std::string& error) {
    error.clear();
    for (size_t i = 0; i < overlayBytes_.size(); ++i) {
        if (!overlayBytes_[i].empty()) continue;
        const std::string relative = std::string("overlays/") + handheldOverlays[i].filename;
        if (!readAssetBytes(assets_, relative, 2 * 1024 * 1024, overlayBytes_[i], overlayReadErrors_[i])) {
            const auto detail = relative + ": " + overlayReadErrors_[i];
            startupLog("Overlay preload failed", detail.c_str());
            if (!error.empty()) error += "; ";
            error += detail;
        } else {
            const auto detail = relative + " (" + std::to_string(overlayBytes_[i].size()) + " bytes)";
            startupLog("Overlay preloaded", detail.c_str());
        }
    }
    return error.empty();
}
bool Video::loadBackground(const std::string& path) {
    auto* image = IMG_Load(path.c_str());
    if (!image) return false;
    background_ = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!background_) { SDL_FreeSurface(image); return false; }
    SDL_Rect crop{0, 0, image->w, image->h};
    if (image->w * 1080 > image->h * 1920) {
        crop.w = image->h * 1920 / 1080; crop.x = (image->w - crop.w) / 2;
    } else { crop.h = image->w * 1080 / 1920; crop.y = (image->h - crop.h) / 2; }
    const bool ok = SDL_BlitScaled(image, &crop, background_, nullptr) == 0;
    SDL_FreeSurface(image);
    if (!ok) { SDL_FreeSurface(background_); background_ = nullptr; }
    return ok;
}
SDL_Texture* Video::shadedBackground(int contrast) {
    contrast = std::clamp(contrast, 0, 2);
    auto*& cached = backgrounds_[contrast];
    if (cached || !background_) return cached;
    auto* layer = SDL_ConvertSurface(background_, background_->format, 0);
    if (!layer) return nullptr;
    if (SDL_LockSurface(layer) < 0) { SDL_FreeSurface(layer); return nullptr; }
    // Cache the vignette per preset; avoid decoding/scaling/shading each frame.
    const float base = contrast == 1 ? .46f : contrast == 2 ? .12f : .27f;
    for (int y = 0; y < layer->h; ++y) {
        auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(layer->pixels) + y * layer->pitch);
        const float edge = std::max(0.0f, (std::abs(y - 540.0f) - 285.0f) / 255.0f) * .21f;
        for (int x = 0; x < layer->w; ++x) {
            const float focus = .23f * std::max(0.0f, 1.0f - std::abs(x - 570.0f) / 1100.0f);
            const float keep = 1.0f - std::min(.83f, base + edge + focus);
            const auto p = row[x];
            const auto r = uint32_t(((p >> 16) & 255) * keep);
            const auto g = uint32_t(((p >> 8) & 255) * keep);
            const auto b = uint32_t((p & 255) * keep);
            row[x] = 0xff000000u | (r << 16) | (g << 8) | b;
        }
    }
    SDL_UnlockSurface(layer);
    cached = SDL_CreateTextureFromSurface(renderer_, layer);
    SDL_FreeSurface(layer);
    return cached;
}
void Video::clear(int contrast) {
    SDL_SetRenderDrawColor(renderer_, 12, 18, 30, 255);
    SDL_RenderClear(renderer_);
    if (auto* texture = shadedBackground(contrast)) SDL_RenderCopy(renderer_, texture, nullptr, nullptr);
}
void Video::rect(int x, int y, int w, int h, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    SDL_Rect r{x,y,w,h}; SDL_RenderFillRect(renderer_, &r);
}
void Video::line(int x1, int y1, int x2, int y2, SDL_Color c, int thickness) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
    for (int i = -thickness / 2; i < (thickness + 1) / 2; ++i) {
        if (std::abs(x2-x1) >= std::abs(y2-y1)) SDL_RenderDrawLine(renderer_, x1, y1+i, x2, y2+i);
        else SDL_RenderDrawLine(renderer_, x1+i, y1, x2+i, y2);
    }
}
void Video::path(std::initializer_list<SDL_Point> points, SDL_Color c, int thickness) {
    if (points.size() < 2) return;
    auto previous = points.begin();
    for (auto current = previous + 1; current != points.end(); ++current) {
        line(previous->x, previous->y, current->x, current->y, c, thickness); previous = current;
    }
}
void Video::circle(int x, int y, int radius, SDL_Color c, int thickness) {
    constexpr int steps = 40;
    for (int i = 0; i < steps; ++i) {
        const float a = float(i) * 6.2831853f / steps, b = float(i+1) * 6.2831853f / steps;
        line(x + int(std::cos(a)*radius), y + int(std::sin(a)*radius),
             x + int(std::cos(b)*radius), y + int(std::sin(b)*radius), c, thickness);
    }
}
Video::TextTexture* Video::cachedText(const std::string& value, int size) {
    const auto key = std::to_string(size) + ":" + value;
    if (auto it = texts_.find(key); it != texts_.end()) return &it->second;
    if (texts_.size() >= 160) {
        for (auto& entry : texts_) SDL_DestroyTexture(entry.second.image);
        texts_.clear();
    }
    const auto font = fonts_.find(size);
    if (font == fonts_.end()) return nullptr;
    auto* s = TTF_RenderUTF8_Blended(font->second, value.c_str(), {255,255,255,255});
    if (!s) return nullptr;
    auto* texture = SDL_CreateTextureFromSurface(renderer_, s);
    const int w = s->w, h = s->h;
    SDL_FreeSurface(s);
    if (!texture) return nullptr;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return &texts_.emplace(key, TextTexture{texture,w,h}).first->second;
}
void Video::drawText(const TextTexture& t, int x, int y, SDL_Color c) {
    SDL_Rect dest{x+2,y+3,t.w,t.h};
    SDL_SetTextureColorMod(t.image, 0, 0, 0);
    SDL_SetTextureAlphaMod(t.image, uint8_t(c.a * .70f));
    SDL_RenderCopy(renderer_, t.image, nullptr, &dest);
    dest.x = x; dest.y = y;
    SDL_SetTextureColorMod(t.image, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(t.image, c.a);
    SDL_RenderCopy(renderer_, t.image, nullptr, &dest);
}
void Video::text(const std::string& value, int x, int y, SDL_Color c, int size, int maxWidth) {
    if (value.empty()) return;
    std::string display = value;
    auto font = fonts_.find(size);
    if (font == fonts_.end()) return;
    int width = 0;
    TTF_SizeUTF8(font->second, display.c_str(), &width, nullptr);
    if (width > maxWidth) {
        do {
            size_t last = display.size()-1;
            while (last && (static_cast<unsigned char>(display[last]) & 0xc0) == 0x80) --last;
            display.erase(last);
            TTF_SizeUTF8(font->second, (display + "…").c_str(), &width, nullptr);
        } while (!display.empty() && width > maxWidth);
        display += "…";
    }
    if (auto* t = cachedText(display, size)) drawText(*t, x, y, c);
}
void Video::centered(const std::string& value, int x, int y, SDL_Color c, int size) {
    if (auto* t = cachedText(value, size)) drawText(*t, x-t->w/2, y, c);
}
void Video::clip(int x, int y, int w, int h) { SDL_Rect r{x,y,w,h}; SDL_RenderSetClipRect(renderer_, &r); }
void Video::unclip() { SDL_RenderSetClipRect(renderer_, nullptr); }
bool Video::present(std::string& error) {
    SDL_RenderPresent(renderer_);
    if (surface_ && SDL_UpdateWindowSurface(window_) < 0) { error = SDL_GetError(); return false; }
    return true;
}
bool Video::setVSync(bool enabled, std::string& error) {
    // Software is only used by desktop tests and has no display swap interval.
    if (surface_) return true;
#if SDL_VERSION_ATLEAST(2, 0, 18)
    if (SDL_RenderSetVSync(renderer_, enabled ? 1 : 0) == 0) return true;
    error = SDL_GetError();
#else
    error = "La versión de SDL no permite cambiar VSync";
#endif
    return false;
}
void Video::releaseGameFrame() {
    if (gameTexture_) SDL_DestroyTexture(gameTexture_);
    gameTexture_ = nullptr; gameWidth_ = gameHeight_ = 0;
    gameScaleModeSet_ = false;
}
void Video::clearLibraryArtwork() {
    if (libraryTexture_) SDL_DestroyTexture(libraryTexture_);
    libraryTexture_ = nullptr;
    libraryWidth_ = libraryHeight_ = 0;
    libraryKey_.clear();
}
bool Video::setLibraryArtwork(SDL_Surface* source, const std::string& key, std::string& error) {
    error.clear();
    if (libraryTexture_ && key == libraryKey_) return true;
    clearLibraryArtwork();
    if (!renderer_ || key.empty() || key.size() > 2048 || !source || !source->pixels || !source->format ||
        source->w <= 0 || source->h <= 0 || source->w > 2048 || source->h > 2048) {
        error = "Imagen de biblioteca no válida: se admite hasta 2048 × 2048";
        return false;
    }
    auto* texture = SDL_CreateTextureFromSurface(renderer_, source);
    if (!texture) { error = SDL_GetError(); return false; }
    bool ready = SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) == 0;
#if SDL_VERSION_ATLEAST(2, 0, 12)
    ready = ready && SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear) == 0;
#endif
    if (!ready) { error = SDL_GetError(); SDL_DestroyTexture(texture); return false; }
    libraryTexture_ = texture;
    libraryWidth_ = source->w; libraryHeight_ = source->h;
    libraryKey_ = key;
    return true;
}
bool Video::libraryArtwork(int x, int y, int w, int h) {
    if (!libraryTexture_ || !renderer_ || w <= 0 || h <= 0 || w > 8192 || h > 8192) return false;
    SDL_Rect destination{x,y,w,h};
    if (libraryWidth_ * h > libraryHeight_ * w)
        destination.h = std::max(1, w * libraryHeight_ / libraryWidth_);
    else destination.w = std::max(1, h * libraryWidth_ / libraryHeight_);
    destination.x += (w - destination.w) / 2;
    destination.y += (h - destination.h) / 2;
    return SDL_RenderCopy(renderer_, libraryTexture_, nullptr, &destination) == 0;
}
bool Video::setHandheldOverlay(SystemType system, bool enabled, std::string& error) {
    error.clear();
    overlayIndex_ = -1;
    overlayDrawn_ = false; overlayCheckPending_ = false;
    overlayDetail_.clear();
    if (!enabled) return true;
    int index = -1;
    for (size_t i = 0; i < overlays_.size(); ++i)
        if (handheldOverlays[i].system == system) index = static_cast<int>(i);
    if (index < 0) { error = "Los marcos solo están disponibles para GB, GBC, GBA y SNES"; return false; }
    if (!renderer_) { error = "El renderer no está inicializado"; return false; }
    if (!overlays_[index]) {
        // The user's artwork remains unchanged on disk, including its opaque
        // screen. Draw the game on top inside the measured screen aperture.
        const auto& compressed = overlayBytes_[index];
        const auto path = std::string("overlays/") + handheldOverlays[index].filename;
        SDL_Surface* source = nullptr;
        if (!compressed.empty()) {
            auto* reader = SDL_RWFromConstMem(compressed.data(), static_cast<int>(compressed.size()));
            if (!reader) { error = "No se pudo abrir el marco en memoria: " + std::string(SDL_GetError()); return false; }
            source = IMG_Load_RW(reader, 1);
        } else {
            if (assets_.empty()) {
                error = "Marco " + path + ": recursos no disponibles";
                if (!overlayReadErrors_[index].empty()) error += " (" + overlayReadErrors_[index] + ")";
                return false;
            }
            source = IMG_Load((assets_ + "/" + path).c_str());
        }
        if (!source) { error = "No se pudo cargar el marco: " + path + " (" + IMG_GetError() + ")"; return false; }
        if (source->w != 1920 || source->h != 1080) {
            SDL_FreeSurface(source);
            error = "El marco debe medir 1920x1080: " + path;
            return false;
        }
        // The bundled art is a full opaque backdrop (the game covers its
        // screen). Normalize its format once, independently of PNG byte order
        // and unused alpha, to use the same opaque RGB shader as game frames.
        auto* converted = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGB888, 0);
        SDL_FreeSurface(source);
        if (!converted) { error = SDL_GetError(); return false; }
        if (SDL_LockSurface(converted) < 0) {
            error = SDL_GetError(); SDL_FreeSurface(converted); return false;
        }
        for (size_t sample = 0; sample < 4; ++sample) {
            const auto point = overlaySamplePoints[sample];
            const auto* row = reinterpret_cast<const uint32_t*>(
                static_cast<const uint8_t*>(converted->pixels) + point.y * converted->pitch);
            overlaySamples_[index][sample] = row[point.x] & 0xffffffu;
        }
        auto* texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB888,
                                          SDL_TEXTUREACCESS_STATIC, converted->w, converted->h);
        // SDL 2.0.18 CreateTextureFromSurface ignores UpdateTexture's return
        // value. Check the actual upload so an empty GPU allocation is never
        // reported as an available frame merely because creation succeeded.
        const bool uploaded = texture && SDL_UpdateTexture(texture, nullptr,
                                                           converted->pixels, converted->pitch) == 0;
        if (!uploaded) error = "No se pudo subir el marco: " + path + " (" + SDL_GetError() + ")";
        SDL_UnlockSurface(converted);
        SDL_FreeSurface(converted);
        if (!uploaded) { if (texture) SDL_DestroyTexture(texture); return false; }
        if (SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE) < 0 ||
            SDL_SetTextureColorMod(texture,255,255,255) < 0 || SDL_SetTextureAlphaMod(texture,255) < 0) {
            error = SDL_GetError(); SDL_DestroyTexture(texture); return false;
        }
#if SDL_VERSION_ATLEAST(2, 0, 12)
        if (SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest) < 0) {
            error = SDL_GetError(); SDL_DestroyTexture(texture); return false;
        }
#endif
        overlays_[index] = texture;
        const auto detail = path + "; RGB888 1920x1080; bytes=" + std::to_string(compressed.size()) +
            "; source=" + (compressed.empty() ? "file" : "memory");
        startupLog("Overlay upload ready", detail.c_str());
    }
    overlayIndex_ = index;
    overlayCheckPending_ = true;
    overlayDetail_ = "Marco " + std::string(systemId(system)) + ": cargado; pendiente de dibujo";
    return true;
}
void Video::verifyOverlayComposition() {
    overlayCheckPending_ = false;
    // Four single pixels after the first complete composition, never during
    // subsequent gameplay. This flushes queued SDL draws and distinguishes a
    // successful API submission from pixels actually reaching the backbuffer.
    int width = 0, height = 0;
    unsigned matched = 0;
    std::string failure;
    if (SDL_GetRendererOutputSize(renderer_, &width, &height) < 0 || width <= 0 || height <= 0) {
        failure = "tamaño de salida no disponible";
    } else {
        for (size_t sample = 0; sample < 4; ++sample) {
            const auto point = overlaySamplePoints[sample];
            const SDL_Rect pixel{point.x * width / 1920,point.y * height / 1080,1,1};
            uint32_t actual = 0;
            if (SDL_RenderReadPixels(renderer_, &pixel, SDL_PIXELFORMAT_ARGB8888, &actual, sizeof(actual)) < 0) {
                failure = SDL_GetError(); break;
            }
            const auto expected = overlaySamples_[overlayIndex_][sample];
            bool equal = true;
            for (unsigned shift : {0u,8u,16u})
                if (std::abs(int((actual >> shift) & 255) - int((expected >> shift) & 255)) > 3) equal = false;
            if (equal) ++matched;
        }
    }
    overlayDetail_ = "Marco " + std::string(systemId(handheldOverlays[overlayIndex_].system)) + ": " +
        (failure.empty() ? std::to_string(matched) + "/4 muestras visibles" : "lectura no disponible");
    const auto detail = overlayDetail_ + "; RGB888; " + (failure.empty() ? "readback OK" : failure);
    startupLog("Overlay composition", detail.c_str());
}
bool Video::gameFrame(const uint32_t* pixels, unsigned width, unsigned height, std::string& error,
                      bool integerScaling, bool linearFilter, bool nativeAspect, SystemType system) {
    error.clear();
    overlayDrawn_ = false;
    if (!pixels || !width || !height || width > 2048 || height > 2048) {
        error = "El núcleo no entregó un cuadro válido"; return false;
    }
#if !SDL_VERSION_ATLEAST(2, 0, 12)
    if (linearFilter) { error = "La versión de SDL no permite cambiar el filtro por textura"; return false; }
#endif
    if (!gameTexture_ || width != gameWidth_ || height != gameHeight_) {
        releaseGameFrame();
#if !SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
#endif
        // CoreFrame is XRGB, not alpha-bearing artwork. Its high byte is
        // undefined for libretro XRGB8888 and zero for converted 16-bit frames.
        // Declare an opaque texture so it cannot become transparent on GLES.
        gameTexture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB888,
                                         SDL_TEXTUREACCESS_STREAMING, width, height);
        if (!gameTexture_) { error = SDL_GetError(); return false; }
        if (SDL_SetTextureBlendMode(gameTexture_, SDL_BLENDMODE_NONE) < 0) {
            error = SDL_GetError(); releaseGameFrame(); return false;
        }
        gameWidth_ = width; gameHeight_ = height;
    }
    if (!gameScaleModeSet_ || gameLinearFilter_ != linearFilter) {
#if SDL_VERSION_ATLEAST(2, 0, 12)
        if (SDL_SetTextureScaleMode(gameTexture_, linearFilter ? SDL_ScaleModeLinear : SDL_ScaleModeNearest) < 0) {
            error = SDL_GetError(); return false;
        }
#endif
        gameLinearFilter_ = linearFilter;
        gameScaleModeSet_ = true;
    }
    if (SDL_UpdateTexture(gameTexture_, nullptr, pixels, width * sizeof(uint32_t)) < 0) {
        error = SDL_GetError(); return false;
    }
    // A complete emulation frame owns the canvas. Do not inherit a library
    // clip, viewport or render target which could hide all frame side panels.
    if (SDL_SetRenderTarget(renderer_, nullptr) < 0 || SDL_RenderSetViewport(renderer_, nullptr) < 0 ||
        SDL_RenderSetClipRect(renderer_, nullptr) < 0 || SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255) < 0 ||
        SDL_RenderClear(renderer_) < 0) { error = SDL_GetError(); return false; }
    SDL_Rect viewport{0,0,1920,1080};
    // Explicit systems must match the selected artwork. Legacy handheld calls
    // may still opt in through nativeAspect; an omitted system never opts into
    // the SNES overlay, so old/default N64 calls cannot inherit its artwork.
    const bool matchingOverlay = overlayIndex_ >= 0 &&
        (handheldOverlays[overlayIndex_].system == system ||
         (system == SystemType::Unknown && nativeAspect &&
          handheldOverlays[overlayIndex_].system != SystemType::SuperNintendo));
    if (matchingOverlay) {
        if (SDL_RenderCopy(renderer_, overlays_[overlayIndex_], nullptr, nullptr) < 0) {
            error = SDL_GetError(); return false;
        }
        viewport = handheldOverlays[overlayIndex_].aperture;
        if (SDL_RenderFillRect(renderer_, &handheldOverlays[overlayIndex_].matte) < 0) {
            error = SDL_GetError(); return false;
        }
    }
    // Preserve the console's 4:3 display aspect, including cropped VI output.
    SDL_Rect destination{240, 0, 1440, 1080};
    const int factor = std::min(viewport.w / int(width), viewport.h / int(height));
    if (integerScaling && factor > 0) {
        destination.w = int(width) * factor;
        destination.h = int(height) * factor;
    } else if (nativeAspect || integerScaling) {
        // When a frame exceeds the viewport, fit it instead of cropping at 1x.
        // Integer arithmetic leaves the complete image inside the logical canvas.
        if (width * viewport.h > height * viewport.w) {
            destination.w = viewport.w;
            destination.h = std::max(1, int(viewport.w * height / width));
        } else {
            destination.h = viewport.h;
            destination.w = std::max(1, int(viewport.h * width / height));
        }
    } else {
        // Fit television 4:3 output inside the chosen aperture, independent of
        // the core's cropped/hires pixel dimensions (for example SNES 256x224).
        if (viewport.w * 3 > viewport.h * 4) {
            destination.h = viewport.h;
            destination.w = viewport.h * 4 / 3;
        } else {
            destination.w = viewport.w;
            destination.h = viewport.w * 3 / 4;
        }
    }
    destination.x = viewport.x + (viewport.w - destination.w) / 2;
    destination.y = viewport.y + (viewport.h - destination.h) / 2;
    if (SDL_RenderCopy(renderer_, gameTexture_, nullptr, &destination) < 0) {
        error = SDL_GetError(); return false;
    }
    overlayDrawn_ = matchingOverlay;
    if (matchingOverlay && overlayCheckPending_) verifyOverlayComposition();
    return true;
}
SDL_Surface* Video::captureFrame() {
    // On a GPU renderer capture the backbuffer before present(), not after swapping.
    if (!renderer_) { SDL_SetError("Video no inicializado"); return nullptr; }
    SDL_Surface* capture = surface_;
    if (!capture) {
        int width = 0, height = 0;
        if (SDL_GetRendererOutputSize(renderer_, &width, &height) < 0) return nullptr;
        capture = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
        if (!capture) return nullptr;
        if (SDL_RenderReadPixels(renderer_, nullptr, capture->format->format, capture->pixels, capture->pitch) < 0) {
            SDL_FreeSurface(capture); return nullptr;
        }
    }
    return capture;
}
bool Video::snapshot(const std::string& path) {
    auto* capture = captureFrame();
    if (!capture) return false;
    const bool ok = path.size() >= 4 && path.substr(path.size()-4) == ".png" ?
        IMG_SavePNG(capture, path.c_str()) == 0 : SDL_SaveBMP(capture, path.c_str()) == 0;
    if (capture != surface_) SDL_FreeSurface(capture);
    return ok;
}
bool Video::saveScreenshot(const std::string& dataRoot, SystemType system,
                           std::string& savedPath, std::string& error) {
    savedPath.clear(); error.clear();
    if (std::string(systemId(system)) == "unknown") { error = "Sistema de captura desconocido"; return false; }
    struct stat info{};
    if (lstat(dataRoot.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) {
        error = "Directorio de datos no disponible para capturas"; return false;
    }
    std::string directory = dataRoot;
    for (const auto* component : {"screenshots", systemId(system)}) {
        directory += "/"; directory += component;
        if ((mkdir(directory.c_str(), 0700) != 0 && errno != EEXIST) ||
            lstat(directory.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) {
            error = "Directorio de capturas no disponible"; return false;
        }
    }
    auto* capture = captureFrame();
    if (!capture) { error = SDL_GetError(); return false; }
    const auto release = [&]() { if (capture != surface_) SDL_FreeSurface(capture); };
    const auto now = std::time(nullptr);
    std::tm date{};
    char timestamp[32]{};
    if (!localtime_r(&now, &date) || !std::strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", &date)) {
        release(); error = "No se pudo fechar la captura"; return false;
    }
    int descriptor = -1;
    std::string path;
    for (unsigned sequence = 0; sequence < 10000; ++sequence) {
        char filename[64]{};
        std::snprintf(filename, sizeof(filename), "/R2RETRO-%s-%04u.png", timestamp, sequence);
        path = directory + filename;
        descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
        if (descriptor >= 0 || errno != EEXIST) break;
    }
    if (descriptor < 0) {
        error = "No se pudo crear la captura: " + std::string(std::strerror(errno));
        release(); return false;
    }
    FILE* file = fdopen(descriptor, "wb");
    bool ok = false;
    if (file) {
        // RW owns its wrapper, while we retain the FILE to check flushing and
        // close errors before reporting success. Only explicit captures sync I/O.
        auto* writer = SDL_RWFromFP(file, SDL_FALSE);
        ok = writer && IMG_SavePNG_RW(capture, writer, 1) == 0;
        if (!ok) error = "No se pudo codificar la captura: " + std::string(IMG_GetError());
        if (std::fflush(file) != 0 || fsync(descriptor) != 0) ok = false;
        if (std::fclose(file) != 0) ok = false;
    } else { close(descriptor); }
    release();
    if (!ok) {
        if (error.empty()) error = "No se pudo escribir la captura";
        unlink(path.c_str());
        return false;
    }
    savedPath = path;
    return true;
}
}
