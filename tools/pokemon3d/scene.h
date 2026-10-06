#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pokemon3d {
constexpr std::size_t MaxVertices = 3000000;
struct Vertex { float x, y, z, r, g, b; float u = 0, v = 0; };
struct Scene {
    std::vector<Vertex> vertices;
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
    unsigned textureWidth = 0, textureHeight = 0;
    std::vector<std::uint8_t> textureRgba;
};
// R2SCENE1, little-endian uint32 count, followed by six little-endian IEEE
// float32 values per vertex. Triangles are independent; y is elevation.
// R2SCENE2 adds uint32 texture width and height after count, eight float32
// xyzrgbuv values per vertex, then width*height*4 RGBA bytes. UV origin is top
// left. Both dimensions must be 1..2048; payload length must match exactly.
bool loadScene(const std::string& path, Scene& scene, std::string& error);
bool validateScene(Scene& scene, std::string& error);
}
