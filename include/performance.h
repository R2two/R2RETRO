#pragma once
#include <cmath>
#include <cstdint>

namespace r2n64 {
// Samples are completed emulation steps, including presentation and pacing.
// Paused/menu iterations are deliberately never submitted. A libretro step is
// a video interval (VI), not necessarily a newly rendered game frame.
struct EmulationPerformance {
    uint64_t intervals = 0;
    double elapsedMs = 0, emulatedMs = 0, coreMs = 0, presentMs = 0;

    void add(double elapsed, double nominalHz, double core, double present, unsigned steps = 1) {
        if (!std::isfinite(elapsed) || elapsed <= 0 ||
            !std::isfinite(nominalHz) || nominalHz <= 0 ||
            !std::isfinite(core) || core < 0 ||
            !std::isfinite(present) || present < 0 || !steps) return;
        intervals += steps;
        elapsedMs += elapsed;
        emulatedMs += steps * 1000.0 / nominalHz;
        coreMs += core;
        presentMs += present;
    }
    double speedPercent() const { return elapsedMs > 0 ? emulatedMs * 100.0 / elapsedMs : 0; }
    double intervalsPerSecond() const { return elapsedMs > 0 ? intervals * 1000.0 / elapsedMs : 0; }
    double averageCoreMs() const { return intervals ? coreMs / intervals : 0; }
    double averagePresentMs() const { return intervals ? presentMs / intervals : 0; }
};
}
