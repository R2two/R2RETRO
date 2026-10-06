#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>
namespace pokemon3d {
using WorkRam = std::array<std::uint8_t, 8192>;
struct Actor {
    unsigned slot = 0, picture = 0, x = 0, y = 0, facing = 0;
    // Eight game pixels per world unit, measured at the foot of the sprite.
    // NaN retains compatibility with legacy synthetic actors using x/y only.
    float worldX = std::numeric_limits<float>::quiet_NaN();
    float worldZ = std::numeric_limits<float>::quiet_NaN();
    unsigned frame = 0;
    bool flipX = false;
};
struct SpriteAtlas {
    unsigned width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
};
// Pixel rectangle in the original 160x144 frame. Zero dimensions request the
// full-frame UI fallback while a window is incomplete or not recognisable.
struct UiRect { unsigned x = 0, y = 0, width = 0, height = 0; };
struct RedState {
    unsigned map = 255, x = 0, y = 0, width = 0, height = 0;
    bool pallet = false;
    bool supported = false;
    bool overlay = false;
    UiRect ui;
    std::string reason;
    std::vector<Actor> actors;
};
// Only this exact Red cartridge is supported. No checksums/addresses from its
// screen name are trusted. SHA-1 is for revision identification, not signing.
std::string sha1(const std::vector<std::uint8_t>& bytes);
bool supportedRed(const std::vector<std::uint8_t>& rom);
// Original planar decoder; four GB tiles in TL, TR, BL, BR order.
std::array<std::uint8_t, 256> spritePixels(const std::array<std::uint8_t, 64>& tiles);
// Original artistic material colouring, preserving the cartridge silhouette
// and every transparent pixel. Directions share the same six-frame palette.
std::array<std::uint8_t, 1024> colourSprite(unsigned picture, unsigned frame,
    const std::array<std::uint8_t, 256>& pixels);
// Decode only the verified cartridge into memory. Rows are picture ID - 1;
// six 16x16 frames per row: front/back/side standing, front/back/side walking.
// Recolouring is artistic, not an original Game Boy colour palette.
bool loadSpriteAtlas(const std::vector<std::uint8_t>& rom, SpriteAtlas& atlas, std::string& error);
// Decode a frontend-owned copy after retro_run. Never keep a core pointer or
// write game memory. Constants match the pinned pokered symbols in the report.
RedState decodeRed(const WorkRam& ram, bool verifiedRom);
// Map headers can become valid one frame before coordinates finish changing.
// Require two consecutive ready samples when entering/re-entering 3D.
class LiveGate {
    bool ready_ = false;
    unsigned map_ = 255;
public:
    RedState update(RedState state) {
        const bool ready = state.supported;
        const unsigned map = state.map;
        if (ready && (!ready_ || map != map_)) {
            state.pallet = false;
            state.supported = false;
            state.overlay = false;
            state.ui = {};
            state.actors.clear();
            state.reason = "map settling";
        }
        ready_ = ready;
        map_ = ready ? map : 255;
        return state;
    }
};
}
