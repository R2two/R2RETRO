// Original tasks exercise the real Angrylion barrier with uneven completion.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "parallel_al.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <thread>

namespace {
std::array<std::uint64_t, PARALLEL_MAX_WORKERS> results{};
unsigned roundNumber;
void task(std::uint32_t worker) {
    // Both main-first and background-first completion, without clock sleeps.
    for (unsigned delay = 0; delay < (roundNumber + worker) % 7; ++delay)
        std::this_thread::yield();
    results[worker] = (std::uint64_t(roundNumber) << 32) | worker;
}
}

int main() {
    unsigned completed = 0;
    for (unsigned session = 0; session < 12; ++session)
    for (unsigned workers : {1u, 2u, 3u, 4u, 8u}) {
        parallel_alinit(workers);
        assert(parallel_num_workers() == workers);
        for (unsigned batch = 0; batch < 500; ++batch) {
            ++roundNumber;
            parallel_run(task);
            for (unsigned worker = 0; worker < workers; ++worker)
                assert(results[worker] == ((std::uint64_t(roundNumber) << 32) | worker));
            ++completed;
        }
        parallel_close();
    }
    std::cout << "PASS: " << completed << " barriers across 60 worker lifecycles\n";
}
