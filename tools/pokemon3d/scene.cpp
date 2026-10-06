#include "scene.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>

namespace pokemon3d {
namespace {
std::uint32_t little32(const unsigned char* bytes) {
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
        (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}
float littleFloat(const unsigned char* bytes) {
    const auto value = little32(bytes);
    float result;
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                  "R2SCENE1 requires IEEE float32");
    std::memcpy(&result, &value, 4);
    return result;
}
}
bool validateScene(Scene& scene, std::string& error) {
    error.clear();
    if (scene.vertices.empty() || scene.vertices.size() > MaxVertices || scene.vertices.size() % 3) {
        error = "Vertex count must be a nonzero multiple of three, at most 3000000";
        return false;
    }
    if (scene.textureWidth > 2048 || scene.textureHeight > 2048 ||
        ((scene.textureWidth == 0) != (scene.textureHeight == 0)) ||
        scene.textureRgba.size() != std::uint64_t(scene.textureWidth) * scene.textureHeight * 4ULL) {
        error = "Invalid scene texture dimensions or payload";
        return false;
    }
    scene.minimum.fill(std::numeric_limits<float>::max());
    scene.maximum.fill(std::numeric_limits<float>::lowest());
    for (const auto& v : scene.vertices) {
        const std::array<float, 8> values{{v.x, v.y, v.z, v.r, v.g, v.b, v.u, v.v}};
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (!std::isfinite(values[i])) { error = "Non-finite vertex value"; return false; }
            if (i < 3) {
                if (std::abs(values[i]) > 1000000.f) { error = "Coordinate exceeds safe scene range"; return false; }
                scene.minimum[i] = std::min(scene.minimum[i], values[i]);
                scene.maximum[i] = std::max(scene.maximum[i], values[i]);
            } else if (values[i] < 0.f || values[i] > 1.f) {
                error = "Vertex color and UV must be between zero and one";
                return false;
            }
        }
    }
    return true;
}
bool loadScene(const std::string& path, Scene& scene, std::string& error) {
    scene = {};
    error.clear();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) { error = "Cannot open scene: " + path; return false; }
    const auto length = file.tellg();
    if (length < 12) { error = "Scene header is truncated"; return false; }
    file.seekg(0);
    unsigned char header[12]{};
    if (!file.read(reinterpret_cast<char*>(header), sizeof(header))) { error = "Cannot read scene header"; return false; }
    const bool textured = std::memcmp(header, "R2SCENE2", 8) == 0;
    if (!textured && std::memcmp(header, "R2SCENE1", 8) != 0) { error = "Invalid scene magic"; return false; }
    const std::uint32_t count = little32(header + 8);
    if (count == 0 || count > MaxVertices || count % 3) {
        error = "Invalid or excessive scene vertex count";
        return false;
    }
    unsigned textureWidth = 0, textureHeight = 0;
    if (textured) {
        unsigned char dimensions[8]{};
        if (!file.read(reinterpret_cast<char*>(dimensions), sizeof(dimensions))) {
            error = "Texture dimensions are truncated"; return false;
        }
        textureWidth = little32(dimensions); textureHeight = little32(dimensions + 4);
        if (textureWidth == 0 || textureHeight == 0 || textureWidth > 2048 || textureHeight > 2048) {
            error = "Texture dimensions exceed the 1..2048 range"; return false;
        }
    }
    const std::size_t stride = textured ? 32 : 24;
    const std::uint64_t textureBytes = std::uint64_t(textureWidth) * textureHeight * 4ULL;
    const std::uint64_t expected = (textured ? 20ULL : 12ULL) + std::uint64_t(count) * stride + textureBytes;
    if (std::uint64_t(length) != expected) { error = "Scene length does not match vertex count"; return false; }
    try {
        Scene candidate;
        candidate.vertices.resize(count);
        candidate.textureWidth = textureWidth; candidate.textureHeight = textureHeight;
        // Decode in bounded chunks rather than allocating a second full mesh.
        std::array<unsigned char, 32 * 1024> bytes{};
        std::size_t offset = 0;
        while (offset < count) {
            const auto batch = std::min<std::size_t>(1024, count - offset);
            if (!file.read(reinterpret_cast<char*>(bytes.data()), batch * stride)) {
                error = "Scene vertex payload is truncated"; return false;
            }
            for (std::size_t i = 0; i < batch; ++i) {
                const auto* p = bytes.data() + i * stride;
                candidate.vertices[offset + i] = {littleFloat(p), littleFloat(p + 4),
                    littleFloat(p + 8), littleFloat(p + 12), littleFloat(p + 16), littleFloat(p + 20)};
                if (textured) {
                    candidate.vertices[offset + i].u = littleFloat(p + 24);
                    candidate.vertices[offset + i].v = littleFloat(p + 28);
                }
            }
            offset += batch;
        }
        candidate.textureRgba.resize(static_cast<std::size_t>(textureBytes));
        if (textureBytes && !file.read(reinterpret_cast<char*>(candidate.textureRgba.data()),
                                       static_cast<std::streamsize>(textureBytes))) {
            error = "Texture payload is truncated"; return false;
        }
        // Reject a concurrent append as well as a mismatched initial length.
        if (file.peek() != std::char_traits<char>::eof()) { error = "Trailing scene data"; return false; }
        if (!validateScene(candidate, error)) return false;
        scene = std::move(candidate);
        return true;
    } catch (const std::bad_alloc&) { error = "Insufficient memory for scene"; return false; }
}
}
