#pragma once
#include <algorithm>
#include <cmath>

namespace r2n64 {
class PlaybackSpeed {
public:
    bool update(bool portable, bool running, bool held, unsigned configured) {
        const unsigned next = portable && running && held &&
            (configured == 2 || configured == 4 || configured == 8) ? configured : 1;
        const bool changed = next != factor_;
        factor_ = next;
        return changed;
    }
    unsigned factor() const { return factor_; }
    bool accelerated() const { return factor_ > 1; }
    double periodMs(double fps, unsigned completedSteps) const {
        if (!std::isfinite(fps)) fps = 60;
        return 1000.0 * completedSteps / (std::clamp(fps, 20.0, 65.0) * factor_);
    }
private:
    unsigned factor_ = 1;
};
}
