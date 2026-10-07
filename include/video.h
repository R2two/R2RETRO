#pragma once
#include "gpu_probe.h"
#include "gpu_session.h"
#include "core/system_type.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <array>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>
namespace r2n64 {
enum class VideoBackend { Software, Accelerated };
#ifdef R2N64_PS4
constexpr VideoBackend DefaultVideoBackend = VideoBackend::Accelerated;
#else
constexpr VideoBackend DefaultVideoBackend = VideoBackend::Software;
#endif
class Video {
public:
    ~Video();
    bool initialize(const std::string& assets, std::string& error, VideoBackend backend = DefaultVideoBackend);
    // Main thread, after platform access initialization: refresh late-load
    // paths without reopening SDL/fonts/background. Preloaded artwork survives
    // a namespace change; an empty path only disables uncached file reads.
    void setAssetPath(const std::string& assets);
    // Call after the first presented frame, before platform access changes.
    // Keeps bounded compressed originals; decoding/textures stay lazy.
    bool preloadOverlayAssets(std::string& error);
    void rect(int x, int y, int w, int h, SDL_Color color);
    void line(int x1, int y1, int x2, int y2, SDL_Color color, int thickness = 2);
    void circle(int x, int y, int radius, SDL_Color color, int thickness = 2);
    void path(std::initializer_list<SDL_Point> points, SDL_Color color, int thickness = 2);
    void text(const std::string& value, int x, int y, SDL_Color color, int size = 28, int maxWidth = 1740);
    void centered(const std::string& value, int x, int y, SDL_Color color, int size = 24);
    void clip(int x, int y, int w, int h);
    void unclip();
    void clear(int contrast = 0);
    bool present(std::string& error);
    bool setVSync(bool enabled, std::string& error);
    // Defaults retain the N64 display aspect. Handheld callers opt into the
    // framebuffer's native aspect and can independently select integer/linear.
    // An explicit system prevents a cached overlay from a previous game being
    // drawn. SNES requires its explicit system while retaining the 4:3 default.
    bool gameFrame(const uint32_t* pixels, unsigned width, unsigned height, std::string& error,
                   bool integerScaling = false, bool linearFilter = false, bool nativeAspect = false,
                   SystemType system = SystemType::Unknown, bool pixelsChanged = true);
    // pixelsChanged=false reuses an initialized texture, but still recomposes
    // scaling, filters, shaders, overlays and UI. First use/resize always uploads.
    // Load only at game start or while paused. Reuses the decoded texture when
    // toggled; gameFrame never reads overlay files. Failure falls back to 2D
    // presentation without artwork. Unknown/false clears the selection.
    bool setHandheldOverlay(SystemType system, bool enabled, std::string& error);
    bool handheldOverlayActive() const { return overlayIndex_ >= 0; }
    bool setDisplayShader(unsigned mode, std::string& error);
    unsigned displayShaderMode() const { return displayShader_ ? displayShader_->mode() : 0; }
    const std::string& displayShaderError() const { return displayShaderError_; }
    // Selection/upload and drawing are different states. Drawn means the last
    // frame submitted both artwork and game; the detail reports a bounded
    // readback check performed once after selection, not on every frame.
    bool handheldOverlayDrawn() const { return overlayDrawn_; }
    const std::string& handheldOverlayDetail() const { return overlayDetail_; }
    SDL_Window* window() const { return window_; }
    SDL_Renderer* renderer() const { return renderer_; }
    // Main/render thread only. Source is borrowed and may be released after
    // this call; matching keys reuse the texture without reading source again.
    // A changed-key failure clears stale artwork so another game is not shown.
    bool setLibraryArtwork(SDL_Surface* source, const std::string& key, std::string& error);
    void clearLibraryArtwork();
    // Fits cached artwork inside the rectangle, preserving aspect and alpha.
    // No file access, image decoding or texture creation during drawing.
    bool libraryArtwork(int x, int y, int w, int h);
    // Cached transparent console textures, loaded before sandbox changes.
    bool consoleLogo(SystemType system, int x, int y, int w, int h, uint8_t alpha = 255);
    void releaseGameFrame();
    bool snapshot(const std::string& path);
    // Explicit paused action only: read back the composed frame before drawing
    // pause controls. Unique files under the configured data root never replace
    // an earlier capture; incomplete files are removed on failure.
    bool saveScreenshot(const std::string& dataRoot, SystemType system,
                        std::string& savedPath, std::string& error);
    GpuProbeResult probeGpu();
    bool backgroundReady() const { return background_ != nullptr; }
    const std::string& warning() const { return warning_; }
private:
    struct TextTexture { SDL_Texture* image = nullptr; int w = 0, h = 0; };
    TextTexture* cachedText(const std::string& value, int size);
    void drawText(const TextTexture& texture, int x, int y, SDL_Color color);
    bool loadBackground(const std::string& path);
    SDL_Texture* shadedBackground(int contrast);
    SDL_Surface* captureFrame();
    void verifyOverlayComposition();
    SDL_Window* window_ = nullptr;
    SDL_Surface* surface_ = nullptr;
    SDL_Surface* background_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* gameTexture_ = nullptr;
    std::unique_ptr<DisplayShader> displayShader_;
    std::string displayShaderError_;
    SDL_Texture* libraryTexture_ = nullptr;
    // One bounded, padded texture per console; decoded from the same PNG once.
    std::array<SDL_Texture*, 6> consoleLogos_{};
    int libraryWidth_ = 0, libraryHeight_ = 0;
    std::string libraryKey_;
    unsigned gameWidth_ = 0, gameHeight_ = 0;
    bool gameLinearFilter_ = false, gameScaleModeSet_ = false;
    bool gamePixelsReady_ = false;
    std::array<SDL_Texture*, 3> backgrounds_{};
    std::array<SDL_Texture*, 4> overlays_{};
    std::array<std::vector<uint8_t>, 4> overlayBytes_;
    std::array<std::string, 4> overlayReadErrors_;
    std::array<std::array<uint32_t, 4>, 4> overlaySamples_{};
    int overlayIndex_ = -1;
    bool overlayDrawn_ = false, overlayCheckPending_ = false;
    std::string overlayDetail_;
    std::map<int, TTF_Font*> fonts_;
    std::map<std::string, TextTexture> texts_;
    std::string warning_;
    std::string assets_;
    bool initialized_ = false, ttf_ = false, images_ = false;
};
}
