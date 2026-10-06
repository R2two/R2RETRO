// Original CXD4 vector workload; no game ROM or Nintendo microcode is used.
// Build twice against the pinned upstream VU sources to compare the existing
// scalar implementation with ARCH_MIN_SSE2 under the same compiler/options.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "vu/vu.h"
u32 inst_word = 0;
void message(const char* body) {
    std::fprintf(stderr, "Unexpected RSP diagnostic: %s\n", body);
    std::abort();
}
}

namespace {
std::uint32_t randomState = 0x52324e36;
std::uint32_t randomWord() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
}
std::uint64_t digest = UINT64_C(14695981039346656037);
void hash(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    while (size--) { digest ^= *bytes++; digest *= UINT64_C(1099511628211); }
}
void execute(unsigned opcode, unsigned vd, unsigned vs, unsigned vt,
             unsigned element = 0) {
    inst_word = 0x4a000000u | (element << 21) | (vt << 16) |
                (vs << 11) | (vd << 6) | opcode;
#ifdef ARCH_MIN_SSE2
    const v16 result = COP2_C2[opcode](_mm_load_si128((v16*)VR[vs]),
                                     _mm_load_si128((v16*)VR[vt]));
    _mm_store_si128((v16*)VR[vd], result);
#else
    COP2_C2[opcode](VR[vs], VR[vt]);
    std::memcpy(VR[vd], V_result, sizeof(i16) * N);
#endif
}
void reset() {
    std::memset(VR, 0, sizeof(VR));
    std::memset(VACC, 0, sizeof(VACC));
    set_VCO(0); set_VCC(0); set_VCE(0);
}
void verifyArithmetic() {
    const i16 a[N] = {32767, -32768, 15000, -15000, 0, 1, -1, 2345};
    const i16 b[N] = {1, -1, 20000, -20000, 0, -1, 1, -1234};
    for (unsigned opcode : {0x10u, 0x11u, 0x28u, 0x2cu}) {
        reset();
        std::memcpy(VR[1], a, sizeof(a)); std::memcpy(VR[2], b, sizeof(b));
        execute(opcode, 3, 1, 2);
        for (int lane = 0; lane < N; ++lane) {
            const int sum = opcode == 0x10 ? int(a[lane]) + b[lane] :
                                             int(a[lane]) - b[lane];
            const i16 expected = opcode == 0x28 ? i16(a[lane] & b[lane]) :
                opcode == 0x2c ? i16(a[lane] ^ b[lane]) :
                i16(std::max(-32768, std::min(32767, sum)));
            if (VR[3][lane] != expected) {
                std::fprintf(stderr, "RSP opcode %02x lane %d: %d != %d\n",
                             opcode, lane, int(VR[3][lane]), int(expected));
                std::exit(1);
            }
        }
    }
}

unsigned compareWorkload() {
    // Implemented operations only. Multiply/accumulate, carry, saturation,
    // clipping, merge, logic, accumulator read, reciprocal and square-root.
    constexpr unsigned operations[] = {
        0x00,0x01,0x04,0x05,0x06,0x07,0x08,0x09,0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x13,0x14,0x15,0x1d,0x20,0x21,0x22,0x23,0x24,0x25,
        0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x30,0x31,0x32,0x33,
        0x34,0x35,0x36,0x37
    };
    constexpr i16 edges[] = {-32768, -32767, -1, 0, 1, 16384, 32766, 32767};
    unsigned count = 0;
    reset();
    for (unsigned test = 0; test < 4096; ++test) {
        for (auto& reg : VR) for (auto& value : reg)
            value = test < 64 ? edges[randomWord() & 7] : i16(randomWord());
        for (auto& accumulator : VACC) for (auto& value : accumulator)
            value = i16(randomWord());
        set_VCO(u16(randomWord())); set_VCC(u16(randomWord())); set_VCE(u8(randomWord()));
        for (unsigned opcode : operations) {
            // Results feed later operations, exercising carry/clip and the
            // 48-bit accumulator across instruction boundaries.
            const unsigned vd = 3 + (count % 29);
            execute(opcode, vd, count % 32, (count + 7) % 32,
                    opcode == 0x1d ? 8 + (test % 3) : test % 8);
            hash(VR[vd], sizeof(i16) * N);
            hash(VACC, sizeof(VACC));
            const std::uint32_t flags = get_VCO() | (std::uint32_t(get_VCC()) << 16);
            const std::uint8_t extension = get_VCE();
            hash(&flags, sizeof(flags)); hash(&extension, sizeof(extension));
            ++count;
        }
    }
    return count;
}
double benchmark() {
    // Fixed-point transforms/filter-style work. This measures the VU only;
    // it is not an emulator FPS benchmark or a prediction for PS4 games.
    constexpr unsigned operations[] = {0x00,0x08,0x08,0x08,0x10,0x11,0x14,0x10,
                                       0x20,0x27,0x2c,0x06,0x0e,0x07,0x0f,0x1d};
    std::array<double, 5> times{};
    for (double& duration : times) {
        reset(); randomState = 0x12345678;
        for (auto& reg : VR) for (auto& value : reg) value = i16(randomWord());
        const auto begin = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < 200000; ++i) {
            for (unsigned j = 0; j < 16; ++j)
                execute(operations[j], 3 + j, 1 + ((i + j) % 16),
                        2 + ((i + j * 3) % 16), operations[j] == 0x1d ? 9 : 0);
        }
        duration = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        hash(VR, sizeof(VR)); hash(VACC, sizeof(VACC));
    }
    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}
}
int main() {
    verifyArithmetic();
    const unsigned comparisons = compareWorkload();
    const double milliseconds = benchmark();
#ifdef ARCH_MIN_SSE2
    constexpr const char* profile = "sse2";
#else
    constexpr const char* profile = "scalar";
#endif
    std::printf("{\"profile\":\"%s\",\"compared_operations\":%u,"
                "\"digest\":\"%016llx\",\"timed_operations\":3200000,"
                "\"median_ms\":%.6f}\n", profile, comparisons,
                static_cast<unsigned long long>(digest), milliseconds);
}
