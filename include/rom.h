#pragma once
#include "core/system_type.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace r2n64 {
enum class RomOrder { BigEndian, ByteSwapped, LittleEndian };
struct Game {
    // N64 retains its legacy header CRC ID. Other systems use a full-content
    // fingerprint; SNES excludes an optional 512-byte copier header.
    std::string title, path, id, serial, region;
    SystemType system = SystemType::Unknown;
    uint32_t crc1 = 0, crc2 = 0;
    uint64_t size = 0; // Original file size, including any container/copier header.
    RomOrder order = RomOrder::BigEndian;
};
bool parseHeader(std::array<uint8_t, 64> header, Game& game, std::string& error);
bool readRom(const std::string& path, Game& game, std::string& error);
struct ScanResult {
    std::vector<Game> games;
    std::vector<std::string> warnings;
};
ScanResult scanRoms(const std::vector<std::string>& roots, const std::atomic<bool>& cancel);
}
