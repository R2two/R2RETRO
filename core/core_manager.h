#pragma once
#include "core/core_interface.h"
#include <memory>

namespace r2n64 {
class CoreManager {
public:
    bool load(const std::string& romPath, const std::string& dataRoot,
              Log& log, std::string& error, EmulationConfig config);
    void unload();
    IEmulationCore* active() { return core_.get(); }
    const IEmulationCore* active() const { return core_.get(); }
private:
    std::unique_ptr<IEmulationCore> core_;
};
}
