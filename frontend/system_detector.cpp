#include "frontend/system_detector.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace r2n64 {
SystemType detectSystemFromExtension(const std::string& extension) {
    const auto dot = extension.find_last_of('.');
    const auto slash = extension.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return SystemType::Unknown;
    std::string ext = extension.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (ext == ".gb") return SystemType::GameBoy;
    if (ext == ".gbc") return SystemType::GameBoyColor;
    if (ext == ".gba") return SystemType::GameBoyAdvance;
    if (ext == ".z64" || ext == ".v64" || ext == ".n64") return SystemType::Nintendo64;
    if (ext == ".nes") return SystemType::NintendoEntertainmentSystem;
    if (ext == ".sfc" || ext == ".smc") return SystemType::SuperNintendo;
    return SystemType::Unknown;
}

uint64_t maximumRomFileSize(SystemType system) {
    switch (system) {
    case SystemType::GameBoy: case SystemType::GameBoyColor: return 8 * 1024 * 1024;
    case SystemType::GameBoyAdvance: return 32 * 1024 * 1024;
    case SystemType::Nintendo64: return 64 * 1024 * 1024;
    case SystemType::NintendoEntertainmentSystem: return 64 * 1024 * 1024 + 528;
    case SystemType::SuperNintendo: return 16 * 1024 * 1024 + 512;
    default: return 0;
    }
}

size_t systemHeaderReadSize(SystemType system, uint64_t fileSize) {
    switch (system) {
    case SystemType::GameBoy: case SystemType::GameBoyColor: return 0x150;
    case SystemType::GameBoyAdvance: return 0xc0;
    case SystemType::Nintendo64: return 64;
    case SystemType::NintendoEntertainmentSystem: return 16;
    case SystemType::SuperNintendo: return static_cast<size_t>(std::min<uint64_t>(fileSize, 0x410200));
    default: return 0;
    }
}

size_t snesCopierHeaderSize(uint64_t fileSize) {
    return fileSize % 0x8000 == 512 ? 512 : 0;
}

namespace {
uint16_t little16(const uint8_t* bytes) {
    return uint16_t(bytes[0]) | uint16_t(uint16_t(bytes[1]) << 8);
}
// NES 2.0 specification: https://www.nesdev.org/wiki/NES_2.0
// Exponent notation is in bytes, not PRG/CHR banks. Bound before multiplication
// and before shifting, even when a corrupt header asks for 2^63 * 7 bytes.
bool nesAreaSize(uint8_t low, uint8_t high, uint64_t bankSize, uint64_t& size) {
    constexpr uint64_t limit = 64 * 1024 * 1024;
    if (high != 15) size = (uint64_t(high) * 256 + low) * bankSize;
    else {
        const unsigned exponent = low >> 2;
        const unsigned multiplier = (low & 3) * 2 + 1;
        if (exponent > 26 || (UINT64_C(1) << exponent) > limit / multiplier) return false;
        size = (UINT64_C(1) << exponent) * multiplier;
    }
    return size <= limit;
}
bool nesHeader(const uint8_t* h, size_t available, uint64_t fileSize, std::string& error) {
    if (!h || available < 16 || fileSize < 17 ||
        fileSize > maximumRomFileSize(SystemType::NintendoEntertainmentSystem)) {
        error = "R2N64-ROM-009: NES requiere cabecera de 16 bytes y hasta 64 MiB de ROM";
        return false;
    }
    if (std::memcmp(h, "NES\x1a", 4) != 0) {
        error = "R2N64-ROM-010: cabecera iNES/NES 2.0 no reconocida; FDS no admitido";
        return false;
    }
    uint64_t prg = 0, chr = 0;
    if ((h[7] & 0x0c) == 0x08) {
        if (!nesAreaSize(h[4], h[9] & 15, 16384, prg) ||
            !nesAreaSize(h[5], h[9] >> 4, 8192, chr)) {
            error = "R2N64-ROM-011: tamano NES 2.0 fuera de los limites admitidos";
            return false;
        }
    } else {
        // Legacy iNES uses zero for 256 PRG banks in FCEU-compatible loaders.
        // Reserved bytes are not required to be zero (old dumps contain text).
        prg = uint64_t(h[4] ? h[4] : 256) * 16384;
        chr = uint64_t(h[5]) * 8192;
    }
    const uint64_t prefix = 16 + ((h[6] & 4) ? 512 : 0);
    if (prg == 0 || prg + chr > 64 * 1024 * 1024 || fileSize < prefix + prg + chr) {
        error = "R2N64-ROM-012: NES incompleta: faltan datos PRG, CHR o trainer declarados";
        return false;
    }
    // Extra bytes may be NES 2.0 miscellaneous ROMs or old dump metadata.
    // Mapper support remains the selected core's responsibility.
    return true;
}

int snesCandidate(const uint8_t* data, size_t available, size_t copier, size_t address) {
    const size_t start = copier + address;
    if (start > available || available - start < 64) return -1;
    const uint8_t* h = data + start;
    const uint16_t reset = little16(h + 0x3c);
    if (reset < 0x8000) return -1;
    const size_t entry = copier + (address & ~size_t(0x7fff)) + (reset & 0x7fff);
    if (entry >= available) return -1;
    // Metadata may be stale; combine independent hints instead of requiring a
    // full-ROM checksum or trusting the claimed size to shift/allocate memory.
    const unsigned mapper = h[0x15] & ~0x10u;
    const bool mapped = address == 0x7fc0 ? (mapper == 0x20 || mapper == 0x22 || mapper == 0x23) :
                        address == 0xffc0 ? (mapper == 0x21 || mapper == 0x2a) : mapper == 0x25;
    const bool checksum = uint32_t(little16(h + 0x1c)) + little16(h + 0x1e) == 0xffff &&
                          little16(h + 0x1c) != 0 && little16(h + 0x1e) != 0;
    const uint8_t opcode = data[entry];
    const bool plausibleStart = opcode == 0x78 || opcode == 0x18 || opcode == 0x38 || opcode == 0x4c ||
        opcode == 0x5c || opcode == 0x9c || opcode == 0xc2 || opcode == 0xe2 || opcode == 0xa9 ||
        opcode == 0xa2 || opcode == 0xa0 || opcode == 0xad || opcode == 0xae || opcode == 0xac ||
        opcode == 0xaf || opcode == 0x20 || opcode == 0x22;
    bool title = false;
    for (unsigned i = 0; i < 21; ++i) title |= h[i] > 32 && h[i] < 127;
    if (!mapped && !checksum && !(plausibleStart && title)) return -1;
    return (mapped ? 6 : 0) + (checksum ? 8 : 0) + (plausibleStart ? 8 : 0) + (title ? 1 : 0) +
           (address == 0x40ffc0 ? 4 : 0);
}
}

bool selectSnesHeader(const uint8_t* data, size_t available, uint64_t fileSize,
                      size_t& offset, std::string& error) {
    offset = 0;
    error.clear();
    const auto copier = snesCopierHeaderSize(fileSize);
    if (!data || fileSize < 32768 + copier || fileSize > maximumRomFileSize(SystemType::SuperNintendo) ||
        fileSize - copier > 16 * 1024 * 1024 ||
        available < systemHeaderReadSize(SystemType::SuperNintendo, fileSize)) {
        error = "R2N64-ROM-013: SNES requiere entre 32 KiB y 16 MiB, con copier de 512 bytes opcional";
        return false;
    }
    // Conventional internal locations used by bsnes-mercury's cartridge loader:
    // ananke/heuristics/super-famicom.hpp. Both .sfc and .smc may contain a copier.
    int best = -1;
    for (size_t address : {size_t(0x7fc0), size_t(0xffc0), size_t(0x40ffc0)}) {
        const int score = snesCandidate(data, std::min<uint64_t>(available, fileSize), copier, address);
        if (score > best) { best = score; offset = copier + address; }
    }
    if (best < 0) {
        error = "R2N64-ROM-014: no se reconoce cabecera SNES LoROM, HiROM o ExHiROM";
        return false;
    }
    return true;
}

bool validateSystemHeader(SystemType system, const uint8_t* h, size_t headerSize,
                          uint64_t fileSize, std::string& error) {
    error.clear();
    if (system == SystemType::NintendoEntertainmentSystem) return nesHeader(h, headerSize, fileSize, error);
    if (system == SystemType::SuperNintendo) {
        size_t offset;
        return selectSnesHeader(h, headerSize, fileSize, offset, error);
    }
    if (system == SystemType::Nintendo64) {
        if (!h || headerSize < 64 || fileSize < 4096 || fileSize > 64 * 1024 * 1024 || fileSize % 4) {
            error = "R2N64-ROM-002: se requiere una ROM N64 de 4 KiB a 64 MiB"; return false;
        }
        const uint32_t magic = (uint32_t(h[0]) << 24) | (uint32_t(h[1]) << 16) | (uint32_t(h[2]) << 8) | h[3];
        if (magic != 0x80371240 && magic != 0x37804012 && magic != 0x40123780) {
            error = "R2N64-ROM-001: cabecera N64 no reconocida"; return false;
        }
        return true;
    }
    if (system == SystemType::GameBoy || system == SystemType::GameBoyColor) {
        if (!h || headerSize < 0x150 || fileSize < 32768 || fileSize > 8 * 1024 * 1024 || fileSize % 16384) {
            error = "R2N64-ROM-003: tamano o cabecera GB/GBC incompletos"; return false;
        }
        uint64_t declaredSize = 0;
        if (h[0x148] <= 8) declaredSize = uint64_t(32768) << h[0x148];
        else if (h[0x148] >= 0x52 && h[0x148] <= 0x54) {
            constexpr unsigned banks[]{72, 80, 96};
            declaredSize = uint64_t(banks[h[0x148] - 0x52]) * 16384;
        }
        if (!declaredSize || fileSize < declaredSize) {
            error = "R2N64-ROM-004: tamano GB/GBC no coincide con la cabecera"; return false;
        }
        // Pan Docs: subtract each header byte and one, modulo 256.
        uint8_t checksum = 0;
        for (size_t i = 0x134; i <= 0x14c; ++i) checksum = uint8_t(checksum - h[i] - 1);
        if (checksum != h[0x14d]) {
            error = "R2N64-ROM-005: checksum de cabecera GB/GBC incorrecto"; return false;
        }
        return true;
    }
    if (system == SystemType::GameBoyAdvance) {
        if (!h || headerSize < 0xc0 || fileSize < 0xc0 || fileSize > 32 * 1024 * 1024 || fileSize % 4) {
            error = "R2N64-ROM-006: tamano o cabecera GBA incompletos"; return false;
        }
        uint8_t checksum = 0x19;
        for (size_t i = 0xa0; i <= 0xbc; ++i) checksum = uint8_t(checksum + h[i]);
        if (h[0xb2] != 0x96 || uint8_t(checksum + h[0xbd]) != 0) {
            error = "R2N64-ROM-007: cabecera o checksum GBA incorrectos"; return false;
        }
        return true;
    }
    error = "R2N64-ROM-008: extension de ROM no soportada";
    return false;
}
}
