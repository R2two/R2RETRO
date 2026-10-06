#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace r2n64 {
// assetsFd is a borrowed directory descriptor opened before changing the PS4
// sandbox. Return the original path if it still names that exact directory;
// otherwise search only <titleId>_<numeric-slot>/app0/assets under sandboxRoot.
// Every opened path component must be a real directory, never a symlink. The
// descriptor remains owned by the caller on all paths. No cwd changes occur.
// titleId and sandboxRoot are needed/validated only for the fallback search.
// An empty result reports a bounded search/read/identity failure in error.
std::string resolveAssetPath(int assetsFd, const std::string& original,
                             const std::string& sandboxRoot, const std::string& titleId,
                             std::string& error);

// Read a bundled asset before changing the filesystem namespace. assetRoot
// must be absolute; relativePath cannot escape it or traverse symlinks. Only
// nonempty regular files of at most maxBytes are read, with exact/complete I/O.
// bytes owns an independent copy after success and is empty after any failure.
// This does not authenticate a new root after a sandbox transition: callers
// must preload from their original trusted package root and retain those bytes.
bool readAssetBytes(const std::string& assetRoot, const std::string& relativePath,
                    size_t maxBytes, std::vector<uint8_t>& bytes, std::string& error);
}
