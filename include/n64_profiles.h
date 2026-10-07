#pragma once
#include "core/core_interface.h"
#include <cstddef>
#include <string>

namespace r2n64 {
// Uses the already computed, canonical whole-ROM fingerprint + header CRCs.
// Names, paths and header CRCs alone never select a profile.
const char* applyN64Profile(const std::string& identity, size_t bytes, EmulationConfig& config);
}
