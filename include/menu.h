#pragma once
#include "platform.h"
#include "core/system_catalog.h"
#include <array>
#include <cstddef>
namespace r2n64 {
enum class Category { Library, Storage, Settings, About };
enum class MenuAction { None, Scan, InspectGame, StartGame, StartDiagnostic, StartGpuDiagnostic, Quit, DownloadLibraryMetadata, Updates };
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
    bool automaticProfile = true;
    const char* graphicsLabel() const {
        return !gpuRendering ? "Angrylion CPU" : graphicsHle ? "GLideN64 GPU + HLE gráfico" : "GLideN64 GPU + RSP LLE";
    }
    // 0 is the console directory, never an unfiltered list of games.
    static constexpr unsigned SystemFilterCount = librarySystems.size() + 1;
    unsigned systemFilter = 0;
    std::array<size_t, librarySystems.size()> gameSelections{};
    bool consoleDirectory() const { return systemFilter == 0; }
    SystemType librarySystem() const {
        return systemFilter && systemFilter < SystemFilterCount ? librarySystems[systemFilter - 1] : SystemType::Unknown;
    }
    void openConsole(SystemType system);
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
