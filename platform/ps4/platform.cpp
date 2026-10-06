#include "platform.h"
#include "startup.h"
#include "asset_path.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include "GoldHEN.h"

namespace r2n64 {
bool Platform::initialize() {
    // SDL video and Piglet must already be ready; SDL owns USER_SERVICE.
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        status = "SDL video must initialize before PS4 data access";
        return false;
    }
    // Capture the mounted asset directory while /app0 still belongs to the
    // original sandbox. Keep its descriptor across GoldHEN's root transition.
    const int assetDirectory = ::open(assets_.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    const int assetOpenError = assetDirectory < 0 ? errno : 0;
    OrbisAppInfo appInfo{};
    const int appInfoResult = sceKernelGetAppInfo(::getpid(), &appInfo);
    const std::string titleId = appInfoResult == 0 ?
        std::string(appInfo.TitleId, ::strnlen(appInfo.TitleId, sizeof(appInfo.TitleId))) : std::string{};
    const std::string appInfoDetail = "pid=" + std::to_string(::getpid()) + " result=" +
        std::to_string(appInfoResult) + " title=" + (titleId.empty() ? "<unavailable>" : titleId);
    startupLog("AppInfo before data access", appInfoDetail.c_str());
    struct stat assetInfo{};
    if (assetDirectory >= 0 && ::fstat(assetDirectory, &assetInfo) == 0) {
        const std::string identity = "dev=" + std::to_string(static_cast<uint64_t>(assetInfo.st_dev)) +
            " inode=" + std::to_string(static_cast<uint64_t>(assetInfo.st_ino));
        startupLog("Asset identity before data access", identity.c_str());
    }
    startupLog("GoldHEN data access begin");
    jailbreak_backup backup{};
    const int access = sys_sdk_jailbreak(&backup);
    status = access == 0 ? "GoldHEN: acceso a datos habilitado" :
        "GoldHEN: acceso a datos no disponible (" + std::to_string(access) + ")";
    startupDataReady();
    startupLog("GoldHEN data access result", status.c_str());
    std::string assetError;
    if (assetDirectory >= 0) {
        assets_ = resolveAssetPath(assetDirectory, assets_, "/mnt/sandbox", titleId, assetError);
        ::close(assetDirectory);
    } else {
        assets_.clear();
        assetError = "No se pudo conservar el directorio de recursos [open]: errno " + std::to_string(assetOpenError);
    }
    if (assets_.empty()) {
        // Keep the already loaded UI and user ROMs available. Do not guess a
        // different CA file or asset directory when identity cannot be checked.
        status += " | Recursos del paquete no disponibles";
        startupLog("Asset path unavailable", assetError.c_str());
    } else startupLog("Asset path ready", assets_.c_str());
    // SDL_INIT_VIDEO loads PAD but does not initialize SDL's joystick subsystem.
    const int rc = scePadInit();
    if (rc < 0) status += " | Pad init: " + std::to_string(rc);
    return true;
}
Input Platform::poll() {
    Input out;
    SDL_Event event;
    while (SDL_PollEvent(&event)) if (event.type == SDL_QUIT) out.quit = true;
    const auto now = SDL_GetTicks();
    if (pad_ < 0 && (retryAt_ == 0 || int32_t(now - retryAt_) >= 0)) {
        int user = -1;
        if (sceUserServiceGetInitialUser(&user) == 0)
            pad_ = scePadOpen(user, ORBIS_PAD_PORT_TYPE_STANDARD, 0, nullptr);
        retryAt_ = now + 1000;
    }
    uint32_t held = 0;
    if (pad_ >= 0) {
        OrbisPadData data{};
        const int rc = scePadReadState(pad_, &data);
        out.connected = rc == 0 && data.connected;
        if (out.connected) {
            auto& game = out.gamepad;
            game.connected = true;
            auto button = [&game, &data](unsigned id, uint32_t mask) {
                if (data.buttons & mask) game.buttons |= uint16_t(1u << id);
            };
            button(0, ORBIS_PAD_BUTTON_CROSS);     // Libretro B = N64 A.
            button(1, ORBIS_PAD_BUTTON_SQUARE);    // Libretro Y = N64 B.
            button(3, ORBIS_PAD_BUTTON_TOUCH_PAD); // Options remains available to the frontend.
            button(4, ORBIS_PAD_BUTTON_UP);
            button(5, ORBIS_PAD_BUTTON_DOWN);
            button(6, ORBIS_PAD_BUTTON_LEFT);
            button(7, ORBIS_PAD_BUTTON_RIGHT);
            button(10, ORBIS_PAD_BUTTON_L1);
            button(11, ORBIS_PAD_BUTTON_R1);
            button(12, ORBIS_PAD_BUTTON_L2);       // N64 Z trigger.
            // Mupen applies the N64 deadzone and sensitivity itself. Preserve
            // libretro's positive-down Y convention on both sticks.
            auto axis = [](uint8_t value) -> int16_t {
                const int centered = int(value) - 128;
                // Avoid -32768: upstream squares both axes in a signed int.
                return int16_t(centered * 32767 / (centered < 0 ? 128 : 127));
            };
            game.analogX = axis(data.leftStick.x);
            game.analogY = axis(data.leftStick.y);
            game.rightX = axis(data.rightStick.x);
            game.rightY = axis(data.rightStick.y);
            game.menu = (data.buttons & ORBIS_PAD_BUTTON_OPTIONS) != 0;
            game.start = game.menu;
            game.fastForward = (data.buttons & ORBIS_PAD_BUTTON_R2) != 0;
            game.select = (data.buttons & ORBIS_PAD_BUTTON_TOUCH_PAD) != 0;
            game.faceEast = (data.buttons & ORBIS_PAD_BUTTON_CIRCLE) != 0;
            game.faceNorth = (data.buttons & ORBIS_PAD_BUTTON_TRIANGLE) != 0;
            game.quickMenu = (data.buttons & (ORBIS_PAD_BUTTON_L3 | ORBIS_PAD_BUTTON_R3)) ==
                (ORBIS_PAD_BUTTON_L3 | ORBIS_PAD_BUTTON_R3);
            if ((data.buttons & ORBIS_PAD_BUTTON_LEFT) || data.leftStick.x < 64) held |= Left;
            if ((data.buttons & ORBIS_PAD_BUTTON_RIGHT) || data.leftStick.x > 192) held |= Right;
            if ((data.buttons & ORBIS_PAD_BUTTON_UP) || data.leftStick.y < 64) held |= Up;
            if ((data.buttons & ORBIS_PAD_BUTTON_DOWN) || data.leftStick.y > 192) held |= Down;
            if (data.buttons & ORBIS_PAD_BUTTON_CROSS) held |= Confirm;
            if (data.buttons & ORBIS_PAD_BUTTON_CIRCLE) held |= Back;
            if (data.buttons & ORBIS_PAD_BUTTON_TRIANGLE) held |= Refresh;
            if (data.buttons & ORBIS_PAD_BUTTON_SQUARE) held |= Diagnostics;
            if (data.buttons & ORBIS_PAD_BUTTON_L1) held |= PreviousSystem;
            if (data.buttons & ORBIS_PAD_BUTTON_R1) held |= NextSystem;
            if (data.buttons & ORBIS_PAD_BUTTON_OPTIONS) held |= DownloadMetadata;
        } else if (rc < 0) {
            scePadClose(pad_);
            pad_ = -1;
        }
    }
    out.pressed = held & ~previous_;
    out.held = held;
    previous_ = held;
    return out;
}
void Platform::shutdown() {
    if (pad_ >= 0) scePadClose(pad_);
    pad_ = -1;
}
std::string Platform::dataPath() const { return "/data/R2N64"; }
std::string Platform::assetPath() const { return assets_; }
std::vector<std::string> Platform::romRoots() const {
    return {dataPath() + "/roms", "/mnt/usb0/R2N64/roms", "/mnt/usb1/R2N64/roms"};
}
}
