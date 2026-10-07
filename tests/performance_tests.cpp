#include "performance.h"
#include "playback_speed.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

static void near(double value, double expected, const char* message) {
    if (std::abs(value - expected) > 0.0001) throw std::runtime_error(message);
}
int main() {
    try {
        r2n64::EmulationPerformance ntsc;
        for (unsigned i = 0; i < 60; ++i) ntsc.add(1000.0 / 60.0, 60, 8, 2);
        near(ntsc.speedPercent(), 100, "60 VI/s NTSC must mean 100% speed");
        near(ntsc.intervalsPerSecond(), 60, "normal NTSC interval frequency");
        near(ntsc.averageCoreMs(), 8, "core cost excludes presentation and pacing");
        near(ntsc.averagePresentMs(), 2, "presentation cost is separate");
        // The caller submits no samples while paused; a session with a pause
        // must retain these averages instead of counting time in the menu.
        r2n64::EmulationPerformance pal;
        for (unsigned i = 0; i < 50; ++i) pal.add(20, 50, 9, 1);
        near(pal.speedPercent(), 100, "50 VI/s PAL must also mean 100% speed");
        near(pal.intervalsPerSecond(), 50, "PAL must not be reported as 83% NTSC");
        r2n64::EmulationPerformance slow;
        for (unsigned i = 0; i < 30; ++i) slow.add(1000.0 / 30, 60, 25, 5);
        near(slow.speedPercent(), 50, "30 VI/s for 60 Hz must be 50% speed");
        near(slow.intervalsPerSecond(), 30, "interval frequency is not game FPS");
        near(slow.overBudgetSteps,30,"slow VI work exceeds the 60 Hz budget");
        near(ntsc.overBudgetSteps,0,"pacing sleep must not count as expensive emulation");
        near(slow.peakCoreMs,25,"peak core cost");
        near(slow.peakPresentMs,5,"peak presentation cost");
        auto batch=slow;
        batch.add(100,60,90,10,8);
        near(batch.measuredSingleSteps,30,"fast-forward batches excluded from individual VI peaks");
        near(batch.peakCoreMs,25,"batch cost is not a per-VI peak");
        r2n64::PlaybackSpeed speed;
        r2n64::EmulationPerformance fast;
        for (unsigned factor : {2u, 4u, 8u}) {
            if (!speed.update(true, true, true, factor)) throw std::runtime_error("speed transition missing");
            near(speed.factor(), factor, "requested portable multiplier");
            near(speed.periodMs(60, factor), 1000.0 / 60, "batch pacing must preserve display cadence");
            near(speed.periodMs(60, 1), 1000.0 / (60 * factor), "partial batch must not lose speed accounting");
        }
        for (unsigned n = 0; n < 60; ++n) fast.add(1000.0 / 60, 60, 8, 2, 4);
        near(fast.speedPercent(), 400, "four complete steps per display must measure 400 percent");
        near(fast.averageCoreMs(), 2, "batch core timing must be averaged per emulated step");
        near(fast.averagePresentMs(), .5, "one present must not be counted four times");
        speed.update(false, true, true, 8);
        near(speed.factor(), 1, "N64 must ignore portable fast forward");
        speed.update(true, false, true, 8);
        near(speed.factor(), 1, "pause/disconnect must disable acceleration");
        speed.update(true, true, true, 3);
        near(speed.factor(), 1, "invalid multiplier must fall back to normal");
        speed.update(true, true, false, 8);
        near(speed.factor(), 1, "release must restore normal speed");
        r2n64::EmulationPerformance mixed;
        mixed.add(1000.0 / 60, 60, 8, 1);
        mixed.add(20, 50, 10, 2);
        near(mixed.speedPercent(), 100, "timing changes must weight emulated seconds, not average percentages");
        const auto before = mixed.intervals;
        mixed.add(0, 60, 0, 0);
        mixed.add(10, 0, 0, 0);
        mixed.add(std::numeric_limits<double>::quiet_NaN(), 60, 0, 0);
        mixed.add(10, 60, -1, 0);
        if (mixed.intervals != before) throw std::runtime_error("invalid clock samples must not corrupt aggregates");
        std::puts("Emulation timing: NTSC/PAL speed, slow core, timing changes and invalid samples passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Performance test: %s\n", e.what());
        return 1;
    }
}
