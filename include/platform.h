#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace r2n64 {
enum Button : uint32_t {
    Up = 1 << 0, Down = 1 << 1, Confirm = 1 << 2,
    Back = 1 << 3, Refresh = 1 << 4, Diagnostics = 1 << 5,
    Left = 1 << 6, Right = 1 << 7,
    PreviousSystem = 1 << 8, NextSystem = 1 << 9,
    DownloadMetadata = 1 << 10 // Library-only OPTIONS / desktop M.
};
struct GamepadInput {
    // Button bits use the libretro JOYPAD IDs, not the menu's Button values.
    uint16_t buttons = 0;
    int16_t analogX = 0, analogY = 0;
    int16_t rightX = 0, rightY = 0; // Right stick drives the four N64 C buttons.
    bool menu = false;
    bool connected = false;
    // Physical controls kept separate so non-N64 Start does not open pause.
    bool start = false, select = false, quickMenu = false;
    bool fastForward = false; // Frontend-only R2 for non-N64 cores.
    // Keep the remaining face buttons separate so adding SNES controls cannot
    // change existing N64/portable mappings. East = Circle, North = Triangle.
    bool faceEast = false, faceNorth = false;
};
struct Input {
    uint32_t pressed = 0;
    uint32_t held = 0;
    bool connected = false;
    bool quit = false;
    GamepadInput gamepad;
};
class Platform {
public:
    bool initialize();
    Input poll();
    void shutdown();
    std::string dataPath() const;
    std::string assetPath() const;
    std::vector<std::string> romRoots() const;
    std::string status;
private:
#ifdef R2N64_PS4
    std::string assets_ = "/app0/assets";
    int pad_ = -1;
    uint32_t retryAt_ = 0;
#else
    void* controller_ = nullptr;
#endif
    uint32_t previous_ = 0;
};
}
