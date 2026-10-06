#include "platform.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdlib>
namespace r2n64 {
bool Platform::initialize() {
    status = "Desktop: prueba de frontend SDL2";
    return true;
}
Input Platform::poll() {
    Input out;
    SDL_Event event;
    while (SDL_PollEvent(&event)) if (event.type == SDL_QUIT) out.quit = true;
    auto* pad = static_cast<SDL_GameController*>(controller_);
    if (pad && !SDL_GameControllerGetAttached(pad)) {
        SDL_GameControllerClose(pad); controller_ = nullptr; pad = nullptr;
    }
    if (!pad) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (SDL_IsGameController(i)) { pad = SDL_GameControllerOpen(i); if (pad) break; }
        }
        controller_ = pad;
    }
    out.connected = pad != nullptr;
    const auto* key = SDL_GetKeyboardState(nullptr);
    uint32_t held = 0;
    auto button = [pad](SDL_GameControllerButton b) { return pad && SDL_GameControllerGetButton(pad, b); };
    auto axis = [pad](SDL_GameControllerAxis a) -> int16_t {
        // Upstream Mupen squares both axes in a signed int; two -32768
        // endpoints would overflow when moving diagonally into that corner.
        return pad ? std::max<int16_t>(-32767, SDL_GameControllerGetAxis(pad, a)) : 0;
    };
    auto& game = out.gamepad;
    game.connected = true; // A keyboard remains available when no gamepad is attached.
    auto gameButton = [&game](unsigned id, bool down) {
        if (down) game.buttons |= uint16_t(1u << id);
    };
    gameButton(0, key[SDL_SCANCODE_Z] || button(SDL_CONTROLLER_BUTTON_A));
    gameButton(1, key[SDL_SCANCODE_X] || button(SDL_CONTROLLER_BUTTON_X));
    gameButton(3, key[SDL_SCANCODE_RETURN] || button(SDL_CONTROLLER_BUTTON_BACK));
    gameButton(4, key[SDL_SCANCODE_UP] || button(SDL_CONTROLLER_BUTTON_DPAD_UP));
    gameButton(5, key[SDL_SCANCODE_DOWN] || button(SDL_CONTROLLER_BUTTON_DPAD_DOWN));
    gameButton(6, key[SDL_SCANCODE_LEFT] || button(SDL_CONTROLLER_BUTTON_DPAD_LEFT));
    gameButton(7, key[SDL_SCANCODE_RIGHT] || button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT));
    gameButton(10, key[SDL_SCANCODE_Q] || button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
    gameButton(11, key[SDL_SCANCODE_W] || button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
    gameButton(12, key[SDL_SCANCODE_A] || axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000);
    game.analogX = axis(SDL_CONTROLLER_AXIS_LEFTX);
    game.analogY = axis(SDL_CONTROLLER_AXIS_LEFTY);
    // Number pad provides a keyboard analog stick without stealing the D-pad.
    if (key[SDL_SCANCODE_KP_4] || key[SDL_SCANCODE_KP_6])
        game.analogX = int16_t((int(key[SDL_SCANCODE_KP_6]) - int(key[SDL_SCANCODE_KP_4])) * 32767);
    if (key[SDL_SCANCODE_KP_8] || key[SDL_SCANCODE_KP_2])
        game.analogY = int16_t((int(key[SDL_SCANCODE_KP_2]) - int(key[SDL_SCANCODE_KP_8])) * 32767);
    game.rightX = axis(SDL_CONTROLLER_AXIS_RIGHTX);
    game.rightY = axis(SDL_CONTROLLER_AXIS_RIGHTY);
    if (key[SDL_SCANCODE_J] || key[SDL_SCANCODE_L])
        game.rightX = int16_t((int(key[SDL_SCANCODE_L]) - int(key[SDL_SCANCODE_J])) * 32767);
    if (key[SDL_SCANCODE_I] || key[SDL_SCANCODE_K])
        game.rightY = int16_t((int(key[SDL_SCANCODE_K]) - int(key[SDL_SCANCODE_I])) * 32767);
    game.menu = key[SDL_SCANCODE_ESCAPE] || button(SDL_CONTROLLER_BUTTON_START);
    game.start = key[SDL_SCANCODE_RETURN] || button(SDL_CONTROLLER_BUTTON_START);
    game.select = key[SDL_SCANCODE_TAB] || button(SDL_CONTROLLER_BUTTON_BACK);
    game.faceEast = key[SDL_SCANCODE_C] || button(SDL_CONTROLLER_BUTTON_B);
    game.faceNorth = key[SDL_SCANCODE_S] || button(SDL_CONTROLLER_BUTTON_Y);
    game.fastForward = key[SDL_SCANCODE_SPACE] || axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000;
    game.quickMenu = key[SDL_SCANCODE_ESCAPE] ||
        (button(SDL_CONTROLLER_BUTTON_LEFTSTICK) && button(SDL_CONTROLLER_BUTTON_RIGHTSTICK));
    if (key[SDL_SCANCODE_LEFT] || button(SDL_CONTROLLER_BUTTON_DPAD_LEFT) ||
        (pad && SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) < -16000)) held |= Left;
    if (key[SDL_SCANCODE_RIGHT] || button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ||
        (pad && SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) > 16000)) held |= Right;
    if (key[SDL_SCANCODE_UP] || button(SDL_CONTROLLER_BUTTON_DPAD_UP) ||
        (pad && SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) < -16000)) held |= Up;
    if (key[SDL_SCANCODE_DOWN] || button(SDL_CONTROLLER_BUTTON_DPAD_DOWN) ||
        (pad && SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) > 16000)) held |= Down;
    if (key[SDL_SCANCODE_RETURN] || button(SDL_CONTROLLER_BUTTON_A)) held |= Confirm;
    if (key[SDL_SCANCODE_ESCAPE] || button(SDL_CONTROLLER_BUTTON_B)) held |= Back;
    if (key[SDL_SCANCODE_R] || button(SDL_CONTROLLER_BUTTON_Y)) held |= Refresh;
    if (key[SDL_SCANCODE_D] || button(SDL_CONTROLLER_BUTTON_X)) held |= Diagnostics;
    if (key[SDL_SCANCODE_F] || button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) held |= PreviousSystem;
    if (key[SDL_SCANCODE_G] || button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) held |= NextSystem;
    if (key[SDL_SCANCODE_M] || button(SDL_CONTROLLER_BUTTON_START)) held |= DownloadMetadata;
    out.pressed = held & ~previous_;
    out.held = held;
    previous_ = held;
    return out;
}
void Platform::shutdown() {
    if (controller_) SDL_GameControllerClose(static_cast<SDL_GameController*>(controller_));
    controller_ = nullptr;
}
std::string Platform::dataPath() const {
    const char* path = std::getenv("R2N64_DATA");
    return path && *path ? path : "runtime";
}
std::string Platform::assetPath() const { return R2N64_ASSETS; }
std::vector<std::string> Platform::romRoots() const { return {dataPath() + "/roms"}; }
}
