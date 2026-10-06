#include "platform.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0, SDL_GetError());
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,
            SDL_CONTROLLER_AXIS_MAX, SDL_CONTROLLER_BUTTON_MAX, 0);
        require(index >= 0, "could not attach a virtual SDL game controller");
        SDL_Joystick* joystick = SDL_JoystickOpen(index);
        require(joystick != nullptr, "could not open the virtual controller");
        r2n64::Platform platform;
        require(platform.initialize(), "desktop platform initialization");
        auto input = platform.poll();
        require(input.connected, "virtual controller must be discovered");
        auto button = [joystick](SDL_GameControllerButton id, bool down) {
            require(SDL_JoystickSetVirtualButton(joystick, id, down ? 1 : 0) == 0,
                "could not inject a controller button");
        };
        button(SDL_CONTROLLER_BUTTON_A, true);
        button(SDL_CONTROLLER_BUTTON_X, true);
        button(SDL_CONTROLLER_BUTTON_B, true);
        button(SDL_CONTROLLER_BUTTON_Y, true);
        button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, true);
        button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, true);
        button(SDL_CONTROLLER_BUTTON_BACK, true);
        button(SDL_CONTROLLER_BUTTON_START, true);
        button(SDL_CONTROLLER_BUTTON_DPAD_UP, true);
        SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, -20000);
        SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, 23000);
        SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, 27000);
        SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, -29000);
        SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 32767);
        SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767);
        input = platform.poll();
        const uint16_t expected = (1u << 0) | (1u << 1) | (1u << 3) | (1u << 4) |
            (1u << 10) | (1u << 11) | (1u << 12);
        require(input.gamepad.buttons == expected, "N64 A/B/Start/Dpad/L/R/Z must use upstream libretro IDs");
        require(input.gamepad.faceEast && input.gamepad.faceNorth,
            "SNES east/north face controls must remain separate from N64/portable button bits");
        require(input.gamepad.analogX == -20000 && input.gamepad.analogY == 23000,
            "left stick must preserve libretro signed axes and positive-down Y");
        require(input.gamepad.rightX == 27000 && input.gamepad.rightY == -29000,
            "right stick must remain separate for simultaneous N64 C and A/B input");
        require(input.gamepad.menu, "Start/Options must request the frontend menu");
        require(input.gamepad.fastForward, "R2 must reach portable speed control separately from N64 buttons");
        require(input.gamepad.start && input.gamepad.select && !input.gamepad.quickMenu,
            "portable Start/Select must remain separate from its quick menu");
        require((input.pressed & r2n64::Confirm) != 0 && (input.pressed & r2n64::Up) != 0,
            "XMB input must still work alongside emulation input");
        input = platform.poll();
        require(input.pressed == 0 && input.gamepad.buttons == expected,
            "held emulation controls must persist without repeating menu presses");
        button(SDL_CONTROLLER_BUTTON_LEFTSTICK, true);
        button(SDL_CONTROLLER_BUTTON_RIGHTSTICK, true);
        input = platform.poll();
        require(input.gamepad.quickMenu, "portable pause must respond to the stick-button chord");
        SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, -32768);
        SDL_JoystickSetVirtualAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, -32768);
        input = platform.poll();
        require(input.gamepad.analogX == -32767 && input.gamepad.analogY == -32767,
            "maximum diagonal input must avoid overflow in the upstream squared radius");
        SDL_JoystickDetachVirtual(index);
        input = platform.poll();
        require(!input.connected && input.gamepad.buttons == 0 && !input.gamepad.menu && !input.gamepad.fastForward &&
            input.gamepad.analogX == 0 && input.gamepad.rightX == 0 && !input.gamepad.faceEast && !input.gamepad.faceNorth,
            "disconnect must release held gamepad input");
        require(input.gamepad.connected, "desktop keyboard must remain available without a controller");
        platform.shutdown();
        SDL_JoystickClose(joystick);
        SDL_Quit();
        std::puts("Input: virtual controller mapping, simultaneous C buttons, menu and disconnect passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Input test: %s\n", e.what());
        SDL_Quit();
        return 1;
    }
}
