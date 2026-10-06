#include "rom.h"
#include "storage.h"
#include "frontend/system_detector.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
using namespace r2n64;
namespace fs = std::filesystem;
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static std::array<uint8_t, 64> header() {
    std::array<uint8_t, 64> h{};
    h[0]=0x80; h[1]=0x37; h[2]=0x12; h[3]=0x40;
    h[0x10]=0x12; h[0x11]=0x34; h[0x12]=0x56; h[0x13]=0x78;
    h[0x14]=0x9a; h[0x15]=0xbc; h[0x16]=0xde; h[0x17]=0xf0;
    std::memcpy(h.data()+0x20, "SYNTHETIC TEST       ", 20);
    h[0x3b]='N'; h[0x3c]='T'; h[0x3d]='S'; h[0x3e]='E';
    return h;
}
static void fixture(const fs::path& path, const std::array<uint8_t, 64>& h, bool truncate = false) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(h.data()), h.size());
    if (!truncate) { file.seekp(4095); file.put('\0'); }
}
static std::array<uint8_t, 0x150> gbHeader(bool color = false) {
    std::array<uint8_t, 0x150> h{};
    std::memcpy(h.data() + 0x134, color ? "COLOR TEST" : "MONO TEST", color ? 10 : 9);
    h[0x143] = color ? 0x80 : 0;
    h[0x14a] = 1;
    // Original synthetic header: no cartridge logo or game program.
    for (size_t i = 0x134; i <= 0x14c; ++i) h[0x14d] = uint8_t(h[0x14d] - h[i] - 1);
    return h;
}
static std::array<uint8_t, 0xc0> gbaHeader() {
    std::array<uint8_t, 0xc0> h{};
    std::memcpy(h.data() + 0xa0, "ADVANCE TEST", 12);
    std::memcpy(h.data() + 0xac, "TEST", 4);
    h[0xb2] = 0x96;
    h[0xbd] = uint8_t(-0x19);
    for (size_t i = 0xa0; i <= 0xbc; ++i) h[0xbd] = uint8_t(h[0xbd] - h[i]);
    return h;
}
template<size_t N> static void portableFixture(const fs::path& path, const std::array<uint8_t, N>& h, size_t size) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(h.data()), std::min(size, h.size()));
    if (size > h.size()) { file.seekp(size - 1); file.put('\0'); }
}
static std::array<uint8_t, 16> nesHeader() {
    std::array<uint8_t, 16> h{};
    h[0] = 'N'; h[1] = 'E'; h[2] = 'S'; h[3] = 0x1a;
    h[4] = 1; h[5] = 1; // Original 16 KiB PRG + 8 KiB CHR, no borrowed program.
    return h;
}
static std::vector<uint8_t> snesImage(size_t address = 0x7fc0, bool copier = false) {
    const size_t prefix = copier ? 512 : 0;
    const size_t size = address == 0x40ffc0 ? 0x600000 : address == 0xffc0 ? 0x10000 : 0x8000;
    std::vector<uint8_t> rom(size + prefix);
    if (copier) std::fill_n(rom.begin(), prefix, 0x5a); // Synthetic copier metadata.
    auto* h = rom.data() + prefix + address;
    std::memcpy(h, "ORIGINAL SNES TEST    ", 21);
    h[0x15] = address == 0x40ffc0 ? 0x35 : address == 0xffc0 ? 0x21 : 0x20;
    h[0x17] = address == 0x40ffc0 ? 13 : address == 0xffc0 ? 6 : 5;
    h[0x19] = 1;
    h[0x1c] = 0xcb; h[0x1d] = 0xed; h[0x1e] = 0x34; h[0x1f] = 0x12;
    h[0x3c] = 0x00; h[0x3d] = 0x80;
    rom[prefix + (address & ~size_t(0x7fff))] = 0x78; // Single original SEI opcode for header scoring.
    return rom;
}
static void imageFixture(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    require(file.good(), "image fixture write failed");
}
static bool containsPath(const ScanResult& scan, const fs::path& path) {
    return std::any_of(scan.games.begin(), scan.games.end(), [&](const Game& game) { return game.path == path.string(); });
}
int main() {
    char temporary[] = "/tmp/r2n64-tests-XXXXXX";
    const char* created = mkdtemp(temporary);
    if (!created) return 1;
    const fs::path root(created);
    try {
        std::string error;
        require(prepareStorage(root.string(), error), "storage creation failed");
        require(prepareStorage(root.string(), error), "storage is not idempotent");
        for (const auto* directory : {"roms", "saves", "states", "covers", "screenshots", "configs/games"})
            for (const auto* system : {"gb", "gbc", "gba", "n64", "nes", "snes"})
                require(fs::is_directory(root/directory/system), "missing per-system storage");
        std::ofstream(root/"saves/legacy-n64.srm") << "legacy-save";
        require(prepareStorage(root.string(), error), "storage refresh failed");
        std::ifstream legacy(root/"saves/legacy-n64.srm");
        std::string legacyValue; legacy >> legacyValue;
        require(legacyValue == "legacy-save", "legacy N64 save changed during storage preparation");

        for (const auto& pair : {std::pair<const char*, SystemType>{".GB", SystemType::GameBoy},
                               {"portable.GbC", SystemType::GameBoyColor},
                               {"/folder/file.GBA", SystemType::GameBoyAdvance},
                               {"C:\\roms\\test.Z64", SystemType::Nintendo64},
                               {".v64", SystemType::Nintendo64}, {".N64", SystemType::Nintendo64},
                               {"cart.NeS", SystemType::NintendoEntertainmentSystem},
                               {".SfC", SystemType::SuperNintendo}, {"C:\\roms\\test.SMC", SystemType::SuperNintendo}})
            require(detectSystemFromExtension(pair.first) == pair.second, "system extension mapping failed");
        for (const auto* unsupported : {"", "gb", "file.zip", "file.gb.zip", "directory.gb/file", "disk.fds", "cart.unif", "file.sfc.zip"})
            require(detectSystemFromExtension(unsupported) == SystemType::Unknown, "unknown extension accepted");
        require(std::string(systemId(SystemType::GameBoyAdvance)) == "gba" &&
                std::string(systemName(SystemType::Nintendo64)) == "Nintendo 64", "system names changed");
        auto big = header(), swap = big, little = big;
        for (size_t i=0; i<64; i+=2) std::swap(swap[i], swap[i+1]);
        for (size_t i=0; i<64; i+=4) std::reverse(little.begin()+i, little.begin()+i+4);
        Game game;
        for (const auto& h : {big, swap, little}) {
            require(parseHeader(h, game, error), "byte-order detection failed");
            require(game.title == "SYNTHETIC TEST", "title decode failed");
            require(game.system == SystemType::Nintendo64, "N64 system not set by legacy header API");
            require(game.id == "12345678-9ABCDEF0", "CRC decode failed");
            require(game.serial == "NTSE" && game.region == "USA / NTSC", "region/serial decode failed");
        }
        require(game.order == RomOrder::LittleEndian, "order not preserved");
        auto invalid = big; invalid[0]=0;
        require(!parseHeader(invalid, game, error), "invalid magic accepted");
        fixture(root/"roms/test.Z64", big);
        fixture(root/"roms/test.v64", swap);
        fixture(root/"roms/test.n64", little);
        fixture(root/"roms/invalid.z64", invalid);
        fixture(root/"roms/short.z64", big, true);
        fixture(root/"roms/ignored.txt", big);
        fs::create_symlink(root/"roms/test.Z64", root/"roms/link.z64");
        std::atomic<bool> cancel{false};
        auto scan = scanRoms({(root/"roms").string(), (root/"missing").string()}, cancel);
        require(scan.games.size() == 3, "scanner extensions or formats failed");
        require(scan.warnings.size() == 3, "scanner must report invalid/truncated/symlink files");

        const auto gb = gbHeader(), gbc = gbHeader(true);
        const auto gba = gbaHeader();
        require(validateSystemHeader(SystemType::GameBoy, gb.data(), gb.size(), 32768, error), "synthetic GB rejected");
        require(validateSystemHeader(SystemType::GameBoyColor, gbc.data(), gbc.size(), 32768, error), "synthetic GBC rejected");
        require(validateSystemHeader(SystemType::GameBoyAdvance, gba.data(), gba.size(), gba.size(), error), "synthetic GBA rejected");
        for (size_t n = 0; n < gb.size(); ++n)
            require(!validateSystemHeader(SystemType::GameBoy, gb.data(), n, 32768, error), "truncated GB header accepted");
        for (size_t n = 0; n < gba.size(); ++n)
            require(!validateSystemHeader(SystemType::GameBoyAdvance, gba.data(), n, 32768, error), "truncated GBA header accepted");
        require(!validateSystemHeader(SystemType::GameBoy, nullptr, gb.size(), 32768, error), "null GB header accepted");
        require(!validateSystemHeader(SystemType::Unknown, gb.data(), gb.size(), 32768, error), "unknown system validated");
        require(!validateSystemHeader(SystemType::Nintendo64, big.data(), big.size(), 64 * 1024 * 1024 + 4, error), "oversized N64 accepted");
        require(!validateSystemHeader(SystemType::GameBoyAdvance, gba.data(), gba.size(), 32 * 1024 * 1024 + 4, error), "oversized GBA accepted");
        require(!validateSystemHeader(SystemType::GameBoyAdvance, gba.data(), gba.size(), gba.size() + 1, error), "misaligned GBA accepted");
        require(!validateSystemHeader(SystemType::GameBoy, gb.data(), gb.size(), 32767, error), "short GB accepted");
        auto badGb = gb; badGb[0x14d] ^= 1;
        require(!validateSystemHeader(SystemType::GameBoy, badGb.data(), badGb.size(), 32768, error), "bad GB checksum accepted");
        auto largeGb = gb; largeGb[0x148] = 1; --largeGb[0x14d];
        require(!validateSystemHeader(SystemType::GameBoy, largeGb.data(), largeGb.size(), 32768, error), "truncated declared GB banks accepted");
        auto badGba = gba; badGba[0xbd] ^= 1;
        require(!validateSystemHeader(SystemType::GameBoyAdvance, badGba.data(), badGba.size(), 32768, error), "bad GBA checksum accepted");
        auto wrongMagicGba = gba; wrongMagicGba[0xb2] = 0; wrongMagicGba[0xbd] = uint8_t(wrongMagicGba[0xbd] + 0x96);
        require(!validateSystemHeader(SystemType::GameBoyAdvance, wrongMagicGba.data(), wrongMagicGba.size(), 32768, error), "GBA fixed byte not validated");

        portableFixture(root/"roms/mono.GB", gb, 32768);
        portableFixture(root/"roms/gbc/color.GbC", gbc, 32768);
        portableFixture(root/"roms/gba/advance.gba", gba, 32768);
        fixture(root/"roms/n64/direct.z64", big);
        portableFixture(root/"roms/gb/bad.gb", badGb, 32768);
        portableFixture(root/"roms/gba/truncated.gba", gba, gba.size() - 1);
        fs::create_directories(root/"roms/gb/deeper");
        portableFixture(root/"roms/gb/deeper/ignored.gb", gb, 32768);
        fs::create_directory(root/"roms/archive");
        portableFixture(root/"roms/archive/ignored.gb", gb, 32768);
        fs::create_symlink(root/"roms/gbc/color.GbC", root/"roms/gbc/link.gbc");
        require(mkfifo((root/"roms/blocked.gba").c_str(), 0600) == 0, "fifo fixture creation failed");

        require(readRom((root/"roms/mono.GB").string(), game, error), "GB read failed");
        require(game.system == SystemType::GameBoy && game.title == "MONO TEST" && game.id.rfind("gb-", 0) == 0, "GB metadata incorrect");
        const auto monoId = game.id;
        fs::copy_file(root/"roms/mono.GB", root/"renamed.gb");
        require(readRom((root/"renamed.gb").string(), game, error) && game.id == monoId, "portable ID depends on filename");
        std::fstream changed(root/"renamed.gb", std::ios::in | std::ios::out | std::ios::binary);
        changed.seekp(0x200); changed.put('\1'); changed.close();
        require(readRom((root/"renamed.gb").string(), game, error) && game.id != monoId, "portable ID ignores cartridge payload");
        require(readRom((root/"roms/gbc/color.GbC").string(), game, error) && game.title == "COLOR TEST" &&
                game.system == SystemType::GameBoyColor && game.id.rfind("gbc-", 0) == 0, "GBC metadata incorrect");
        require(readRom((root/"roms/gba/advance.gba").string(), game, error) && game.title == "ADVANCE TEST" &&
                game.system == SystemType::GameBoyAdvance && game.serial == "TEST" && game.id.rfind("gba-", 0) == 0, "GBA metadata incorrect");
        require(!readRom((root/"roms/blocked.gba").string(), game, error), "FIFO accepted");
        require(game.system == SystemType::Unknown && game.id.empty(), "failed read retains stale game metadata");

        scan = scanRoms({(root/"roms").string(), (root/"roms/n64").string(), (root/"roms").string()}, cancel);
        require(scan.games.size() == 7, "multi-system scan or duplicate roots failed");
        require(scan.warnings.size() == 7, "multi-system invalid input warning count incorrect");
        require(containsPath(scan, root/"roms/gbc/color.GbC") && containsPath(scan, root/"roms/gba/advance.gba") &&
                containsPath(scan, root/"roms/n64/direct.z64"), "direct system directories not scanned");
        require(!containsPath(scan, root/"roms/gb/deeper/ignored.gb") && !containsPath(scan, root/"roms/archive/ignored.gb"), "scanner escaped directory depth boundary");

        const auto nes = nesHeader();
        constexpr auto nesSystem = SystemType::NintendoEntertainmentSystem;
        constexpr auto snesSystem = SystemType::SuperNintendo;
        constexpr size_t nesSize = 16 + 16384 + 8192;
        require(validateSystemHeader(nesSystem, nes.data(), nes.size(), nesSize, error), "iNES fixture rejected");
        for (size_t length = 0; length < 16; ++length)
            require(!validateSystemHeader(nesSystem, nes.data(), length, nesSize, error), "truncated iNES header accepted");
        require(!validateSystemHeader(nesSystem, nullptr, 16, nesSize, error), "null iNES header accepted");
        require(!validateSystemHeader(nesSystem, nes.data(), 16, nesSize - 1, error), "truncated NES CHR accepted");
        require(!validateSystemHeader(nesSystem, nes.data(), 16, maximumRomFileSize(nesSystem) + 1, error), "oversized NES accepted");
        auto trainer = nes; trainer[6] |= 4;
        require(validateSystemHeader(nesSystem, trainer.data(), 16, nesSize + 512, error), "NES trainer rejected");
        require(!validateSystemHeader(nesSystem, trainer.data(), 16, nesSize, error), "missing NES trainer accepted");
        auto legacyNes = nes; legacyNes[4] = 0; legacyNes[5] = 0; std::memcpy(legacyNes.data() + 12, "TEST", 4);
        require(validateSystemHeader(nesSystem, legacyNes.data(), 16, 16 + 4 * 1024 * 1024, error), "legacy iNES zero/dirty reserved bytes rejected");
        require(!validateSystemHeader(nesSystem, legacyNes.data(), 16, 16, error), "empty PRG legacy accepted");
        auto nes2 = nes; nes2[7] = 8; nes2[9] = 1; // 257 PRG banks.
        require(validateSystemHeader(nesSystem, nes2.data(), 16, 16 + 257 * 16384 + 8192, error), "NES2 extended linear size rejected");
        require(!validateSystemHeader(nesSystem, nes2.data(), 16, nesSize, error), "NES2 high size nibble ignored");
        nes2[9] = 0x0f; nes2[4] = (13 << 2) | 1; // 2^13 * 3 = 24 KiB; deliberately not a power of two.
        require(validateSystemHeader(nesSystem, nes2.data(), 16, 16 + 24576 + 8192, error), "NES2 exponent-multiplier rejected");
        nes2[9] = 0xff; nes2[5] = (12 << 2) | 1;
        require(validateSystemHeader(nesSystem, nes2.data(), 16, 16 + 24576 + 12288, error), "NES2 CHR exponent rejected");
        for (const uint8_t low : {uint8_t(0xff), uint8_t(27 << 2), uint8_t((26 << 2) | 3)}) {
            nes2[4] = low;
            require(!validateSystemHeader(nesSystem, nes2.data(), 16, maximumRomFileSize(nesSystem), error), "NES2 overflowing exponent accepted");
        }
        nes2 = nes; nes2[7] = 8; nes2[4] = 0;
        require(!validateSystemHeader(nesSystem, nes2.data(), 16, nesSize, error), "NES2 empty PRG accepted");
        auto fds = nes; fds[0] = 'F'; fds[1] = 'D'; fds[2] = 'S';
        require(!validateSystemHeader(nesSystem, fds.data(), 16, nesSize, error), "renamed FDS accepted as iNES");

        for (const size_t address : {size_t(0x7fc0), size_t(0xffc0), size_t(0x40ffc0)}) {
            for (bool copier : {false, true}) {
                auto rom = snesImage(address, copier);
                size_t selected = 0;
                require(selectSnesHeader(rom.data(), rom.size(), rom.size(), selected, error), "SNES header candidate rejected");
                require(selected == address + (copier ? 512 : 0), "SNES selected wrong header offset");
                const auto prefix = systemHeaderReadSize(snesSystem, rom.size());
                require(validateSystemHeader(snesSystem, rom.data(), prefix, rom.size(), error), "bounded SNES prefix rejected");
                require(!validateSystemHeader(snesSystem, rom.data(), prefix - 1, rom.size(), error), "truncated SNES prefix accepted");
                require(snesCopierHeaderSize(rom.size()) == (copier ? 512 : 0), "SNES copier detection incorrect");
                const auto name = std::string("map-") + std::to_string(address) + (copier ? ".smc" : ".sfc");
                imageFixture(root / name, rom);
                require(readRom((root / name).string(), game, error) && game.title == "ORIGINAL SNES TEST" &&
                        game.system == snesSystem && game.id.rfind("snes-", 0) == 0 && game.region == "USA / NTSC", "SNES metadata incorrect");
            }
            Game plain, copied;
            require(readRom((root / ("map-" + std::to_string(address) + ".sfc")).string(), plain, error) &&
                    readRom((root / ("map-" + std::to_string(address) + ".smc")).string(), copied, error) && plain.id == copied.id,
                    "SNES copier metadata changed content identity");
        }
        auto low = snesImage();
        require(!validateSystemHeader(snesSystem, nullptr, low.size(), low.size(), error), "null SNES data accepted");
        require(!validateSystemHeader(snesSystem, low.data(), low.size(), 32767, error), "short SNES accepted");
        require(!validateSystemHeader(snesSystem, low.data(), low.size(), maximumRomFileSize(snesSystem) + 1, error), "oversized SNES accepted");
        auto patched = low;
        patched[0x7fc0 + 0x1c] ^= 1; patched[0x7fc0 + 0x17] = 0xff;
        require(validateSystemHeader(snesSystem, patched.data(), patched.size(), patched.size(), error), "patched SNES checksum/size rejected");
        patched[0x7fc0 + 0x15] = 0x32;
        require(validateSystemHeader(snesSystem, patched.data(), patched.size(), patched.size(), error), "ExLoROM map byte rejected");
        patched[0x7fc0 + 0x15] = 0x23;
        require(validateSystemHeader(snesSystem, patched.data(), patched.size(), patched.size(), error), "SA1 map byte rejected");
        patched[0x7fc0 + 0x3d] = 0;
        require(!validateSystemHeader(snesSystem, patched.data(), patched.size(), patched.size(), error), "SNES reset vector into RAM accepted");
        auto blank = low; std::fill(blank.begin(), blank.end(), 0xff);
        require(!validateSystemHeader(snesSystem, blank.data(), blank.size(), blank.size(), error), "blank SNES file accepted");

        portableFixture(root/"roms/nes/ORIGINAL NES.NeS", nes, nesSize);
        portableFixture(root/"renamed.nes", nes, nesSize);
        require(readRom((root/"roms/nes/ORIGINAL NES.NeS").string(), game, error) && game.title == "ORIGINAL NES" &&
                game.system == nesSystem && game.id.rfind("nes-", 0) == 0, "NES filename metadata incorrect");
        const auto nesId = game.id;
        require(readRom((root/"renamed.nes").string(), game, error) && game.id == nesId, "NES ID depends on filename");
        std::fstream nesChange(root/"renamed.nes", std::ios::in | std::ios::out | std::ios::binary);
        nesChange.seekp(1000); nesChange.put('\1'); nesChange.close();
        require(readRom((root/"renamed.nes").string(), game, error) && game.id != nesId, "NES ID ignores payload");
        imageFixture(root/"roms/snes/original.sfc", low);
        imageFixture(root/"roms/snes/copier.SMC", snesImage(0x7fc0, true));
        imageFixture(root/"renamed.sfc", low);
        require(readRom((root/"renamed.sfc").string(), game, error), "SNES identity baseline failed");
        const auto snesId = game.id;
        low[1000] ^= 1; imageFixture(root/"renamed.sfc", low);
        require(readRom((root/"renamed.sfc").string(), game, error) && game.id != snesId, "SNES ID ignores payload");
        fs::create_symlink(root/"roms/snes/original.sfc", root/"roms/snes/link.sfc");
        portableFixture(root/"roms/nes/fds.nes", fds, nesSize);
        portableFixture(root/"roms/nes/excluded.fds", fds, nesSize);
        fs::create_directories(root/"roms/snes/deeper");
        imageFixture(root/"roms/snes/deeper/ignored.sfc", low);
        scan = scanRoms({(root/"roms").string()}, cancel);
        require(scan.games.size() == 10 && scan.warnings.size() == 9, "NES/SNES scan counts incorrect");
        require(containsPath(scan, root/"roms/nes/ORIGINAL NES.NeS") && containsPath(scan, root/"roms/snes/original.sfc") &&
                containsPath(scan, root/"roms/snes/copier.SMC"), "NES/SNES system folders not scanned");
        require(!containsPath(scan, root/"roms/snes/deeper/ignored.sfc"), "SNES scan exceeded directory boundary");
        fs::create_directory_symlink(root/"roms", root/"linked-root");
        auto linked = scanRoms({(root/"linked-root").string()}, cancel);
        require(linked.games.empty() && linked.warnings.size() == 1, "scanner followed symlink root");
        fs::create_directory(root/"another-root");
        fs::create_directory_symlink(root/"roms/gba", root/"another-root/gba");
        linked = scanRoms({(root/"another-root").string()}, cancel);
        require(linked.games.empty() && linked.warnings.size() == 1, "scanner followed symlink system directory");
        cancel = true;
        require(scanRoms({(root/"roms").string()}, cancel).games.empty(), "cancellation failed");

        fs::remove(root/"states/gba");
        fs::create_directory_symlink(root/"saves/gba", root/"states/gba");
        require(!prepareStorage(root.string(), error), "storage accepted symlinked system directory");
        fs::remove(root/"states/gba");
        fs::create_directory(root/"states/gba");
        fs::remove(root/"logs");
        fs::create_directory_symlink(root/"saves", root/"logs");
        require(!prepareStorage(root.string(), error), "storage accepted symlinked output directory");
        fs::remove_all(root);
        std::puts("PASS: N64/GB/GBC/GBA regression, NES/SNES headers and content IDs, bounded scan, cancellation, storage boundaries");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        fs::remove_all(root); return 1;
    }
}
