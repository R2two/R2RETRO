#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace r2n64 {
// Blocking transport: call from the catalog worker, after SDL video on PS4.
// Uses an explicit PEM CA bundle, verified HTTPS and at most three HTTPS redirects.
// The CA preflight requires a readable regular file of 1 byte to 2 MiB and
// rejects a leaf symlink. Local I/O errors name the stage/errno, never the path.
// On any failure/cancellation body is empty and error is safe to show in the UI.
bool httpGet(const std::string& url, size_t maxBytes, const std::string& caFile,
             const std::atomic<bool>& cancel, std::vector<uint8_t>& body,
             std::string& error);

// Call only after joining all download workers. Shared PS4 networking stays alive.
void httpShutdown();
}
