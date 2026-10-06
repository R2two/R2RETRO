#pragma once
#include "platform.h"
#include <array>
#include <cstddef>
namespace r2n64 {
enum class Category { Library, Storage, Settings, About };
enum class MenuAction { None, Scan, InspectGame, StartGame, StartDiagnostic, StartGpuDiagnostic, Quit, DownloadLibraryMetadata };
struct Menu {
    Category category = Category::Library;
    std::array<size_t, 4> selections{};
    bool details = false;
    bool exitPrompt = false;
    int contrast = 0;
    bool parallelRendering = true;
    bool automaticCpu = true;
    bool audioHle = true;
    bool profileCore = false;
    bool gpuRendering = false;
    bool graphicsHle = true;
    const char* graphicsLabel() const {
        return !gpuRendering ? "Angrylion CPU" : graphicsHle ? "GLideN64 GPU + HLE gráfico" : "GLideN64 GPU + RSP LLE";
    }
    // 0 = all, followed by GB, GBC, GBA, N64, NES and SNES.
    static constexpr unsigned SystemFilterCount = 7;
    unsigned systemFilter = 0;
    size_t selected() const { return selections[static_cast<size_t>(category)]; }
    void clamp(size_t games, size_t roots);
    MenuAction handle(uint32_t pressed, size_t games, size_t roots, bool scanning);
};
class NavigationRepeat {
public:
    uint32_t update(const Input& input, uint32_t now);
private:
    uint32_t held_ = 0, next_ = 0;
};
}
