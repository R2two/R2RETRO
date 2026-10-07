#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace r2n64 {
// Streaming SHA-256 (FIPS 180-4); byte loads avoid alignment/aliasing assumptions.
class UpdateSha256 {
public:
    void add(const uint8_t*, size_t);
    std::string finish();
private:
    void block();
    std::array<uint32_t,8> state_{{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}};
    std::array<uint8_t,64> buffer_{};
    uint64_t bytes_ = 0;
    size_t used_ = 0;
};
}
