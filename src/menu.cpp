#include "menu.h"
#include <algorithm>
namespace r2n64 {
static size_t itemCount(Category category, size_t games, size_t roots, bool directory) {
    switch (category) {
    case Category::Library: return directory ? librarySystems.size() : games ? games : 2;
    case Category::Storage: return std::max(size_t(1), roots);
    case Category::Settings: return 10;
    case Category::About: return 4;
    }
    return 1;
}
void Menu::clamp(size_t games, size_t roots) {
    if (systemFilter >= SystemFilterCount) systemFilter = 0;
    if (category == Category::Library && games == 0) details = false;
    for (size_t i = 0; i < selections.size(); ++i)
        selections[i] = std::min(selections[i], itemCount(static_cast<Category>(i), games, roots, consoleDirectory()) - 1);
}
void Menu::openConsole(SystemType system) {
    if (!consoleDirectory()) gameSelections[systemFilter - 1] = selections[0];
    systemFilter = libraryFolder(system);
    selections[0] = systemFilter ? gameSelections[systemFilter - 1] : 0;
    details = false;
    exitPrompt = false;
}
MenuAction Menu::handle(uint32_t pressed, size_t games, size_t roots, bool scanning) {
    clamp(games, roots);
    if (!pressed) return MenuAction::None;
    if (pressed & Back) {
        if (details) details = false;
        else if (category != Category::Library) category = Category::Library;
        else if (!consoleDirectory()) {
            gameSelections[systemFilter - 1] = selections[0];
            selections[0] = systemFilter - 1;
            systemFilter = 0;
        }
        else if (exitPrompt) return MenuAction::Quit;
        else exitPrompt = true;
        return MenuAction::None;
    }
    exitPrompt = false;
    if (category == Category::Library && (pressed & (PreviousSystem | NextSystem))) {
        const size_t next = ((consoleDirectory() ? selections[0] : systemFilter - 1) +
            ((pressed & NextSystem) ? 1u : librarySystems.size() - 1u)) % librarySystems.size();
        if (consoleDirectory()) selections[0] = next;
        else openConsole(librarySystems[next]);
        details = false;
        return MenuAction::None;
    }
    if ((pressed & DownloadMetadata) && category == Category::Library && !consoleDirectory() && games && !scanning) {
        return MenuAction::DownloadLibraryMetadata;
    } else if (pressed & Diagnostics) {
        category = Category::Settings; selections[2] = 2; details = true;
    } else if (pressed & (Left | Right)) {
        int next = static_cast<int>(category) + ((pressed & Right) ? 1 : -1);
        category = static_cast<Category>(std::clamp(next, 0, 3));
        details = false;
    } else if (pressed & (Up | Down)) {
        auto& row = selections[static_cast<size_t>(category)];
        const auto count = itemCount(category, games, roots, consoleDirectory());
        if ((pressed & Up) && row > 0) --row;
        if ((pressed & Down) && row + 1 < count) ++row;
        details = false;
    } else if (pressed & Refresh) {
        if (!scanning) return MenuAction::Scan;
    } else if (pressed & Confirm) {
        switch (category) {
        case Category::Library:
            if (consoleDirectory()) {
                openConsole(librarySystems[selected()]);
                return MenuAction::None;
            }
            if (games) {
                if (details) return MenuAction::StartGame;
                details = true; return MenuAction::InspectGame;
            }
            if (selected() == 0) { if (!scanning) return MenuAction::Scan; }
            else { category = Category::Storage; selections[1] = 0; details = true; }
            break;
        case Category::Storage:
            if (details) { if (!scanning) return MenuAction::Scan; }
            else details = true;
            break;
        case Category::Settings:
            if (selected() == 9) return MenuAction::Updates;
            if (details && (selected() == 3 || selected() == 4 || selected() == 5 || selected() == 7))
                automaticProfile = false; // Explicit manual choices always take precedence.
            if (selected() == 8 && details) automaticProfile = !automaticProfile;
            if (selected() == 0 && details) contrast = (contrast + 1) % 3;
            if (selected() == 3 && details) parallelRendering = !parallelRendering;
            if (selected() == 4 && details) automaticCpu = !automaticCpu;
            if (selected() == 5 && details) audioHle = !audioHle;
            if (selected() == 6 && details) profileCore = !profileCore;
            if (selected() == 7 && details) {
                if (!gpuRendering) { gpuRendering = true; graphicsHle = true; }
                else if (graphicsHle) graphicsHle = false;
                else { gpuRendering = false; graphicsHle = true; }
            }
            details = true;
            break;
        case Category::About:
            if (selected() == 3) return MenuAction::Quit;
            if (selected() == 2) return MenuAction::StartGpuDiagnostic;
            if (selected() == 1) return MenuAction::StartDiagnostic;
            details = true;
            break;
        }
    }
    return MenuAction::None;
}
uint32_t NavigationRepeat::update(const Input& input, uint32_t now) {
    constexpr uint32_t directions = Up | Down | Left | Right;
    const uint32_t held = input.held & directions;
    uint32_t result = input.pressed;
    if (held != held_) { held_ = held; next_ = now + 380; }
    else if (held && int32_t(now - next_) >= 0) { result |= held; next_ = now + 130; }
    return result;
}
}
