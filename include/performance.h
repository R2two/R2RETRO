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
    double peakCoreMs = 0, peakPresentMs = 0;
    uint64_t measuredSingleSteps = 0, overBudgetSteps = 0;
    uint64_t imageSteps = 0;

    void add(double elapsed, double nominalHz, double core, double present, unsigned steps = 1, unsigned freshImages = 0) {
        if (!std::isfinite(elapsed) || elapsed <= 0 ||
            !std::isfinite(nominalHz) || nominalHz <= 0 ||
            !std::isfinite(core) || core < 0 ||
            !std::isfinite(present) || present < 0 || !steps) return;
        intervals += steps;
        imageSteps += freshImages > steps ? steps : freshImages;
        elapsedMs += elapsed;
        emulatedMs += steps * 1000.0 / nominalHz;
        coreMs += core;
        presentMs += present;
        // Fast-forward batches are not individual VI timings.
        if (steps == 1) {
            ++measuredSingleSteps;
            if (core + present > 1000.0 / nominalHz) ++overBudgetSteps;
            if (core > peakCoreMs) peakCoreMs = core;
            if (present > peakPresentMs) peakPresentMs = present;
        }
    }
    double speedPercent() const { return elapsedMs > 0 ? emulatedMs * 100.0 / elapsedMs : 0; }
    double intervalsPerSecond() const { return elapsedMs > 0 ? intervals * 1000.0 / elapsedMs : 0; }
    double imagesPerSecond() const { return elapsedMs > 0 ? imageSteps * 1000.0 / elapsedMs : 0; }
    double averageCoreMs() const { return intervals ? coreMs / intervals : 0; }
    double averagePresentMs() const { return intervals ? presentMs / intervals : 0; }
};
}
