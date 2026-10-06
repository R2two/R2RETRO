#pragma once
#include "core/system_type.h"
#include <cstddef>
#include <cstdint>
#include <string>

namespace r2n64 {
// Accept an extension including its dot, or a filename/path. ASCII case-insensitive.
SystemType detectSystemFromExtension(const std::string& extension);
// Bounds apply to complete files, including container/copier headers.
uint64_t maximumRomFileSize(SystemType system);
// Prefix needed for header validation (SNES includes all three candidate banks).
size_t systemHeaderReadSize(SystemType system, uint64_t fileSize);
// Matches the core's conventional 512-byte copier-header detection. Call only
// after successful SNES validation before stripping it for content identity/load.
size_t snesCopierHeaderSize(uint64_t fileSize);
// Offset includes the copier header, if present. data starts at file offset zero
// and must contain systemHeaderReadSize(SuperNintendo, fileSize) bytes.
bool selectSnesHeader(const uint8_t* data, size_t available, uint64_t fileSize,
                      size_t& offset, std::string& error);
// Validate enough original header data to reject obvious mismatches/truncation.
// Data starts at file offset zero. Logo bytes are deliberately not required;
// this is not a compatibility verdict. SNES checksum/declared ROM size are hints,
// not strict checks: patched/homebrew images frequently leave them stale.
bool validateSystemHeader(SystemType system, const uint8_t* header, size_t headerSize,
                          uint64_t fileSize, std::string& error);
}
