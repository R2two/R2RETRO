// Real-core frontend integration: uses only the original diagnostic ROM.
#include "emulator.h"
#include "log.h"
#include <libretro.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <unistd.h>
#include <vector>
#ifdef __linux__
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;
using namespace r2n64;
namespace {
void require(bool result, const std::string& reason) {
    if (!result) throw std::runtime_error(reason);
}
uint32_t ramWord(size_t offset) {
    require(retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM) >= offset + 4, "Missing RDRAM");
    const auto* ram = static_cast<const unsigned char*>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
    require(ram != nullptr, "Missing RDRAM pointer");
    uint32_t value;
    std::memcpy(&value, ram + offset, sizeof(value)); // Pinned x86-64 core word-swaps RDRAM.
    return value;
}
void verifyRgb(const CoreFrame& frame) {
    require(frame.width > 0 && frame.height > 0 && frame.pixels.size() == frame.width * frame.height,
            "Missing bounded software frame");
    unsigned red = 0, green = 0, blue = 0;
    for (auto pixel : frame.pixels) {
        const unsigned r = (pixel >> 16) & 255, g = (pixel >> 8) & 255, b = pixel & 255;
        red += r > 160 && g < 80 && b < 80;
        green += g > 160 && r < 80 && b < 80;
        blue += b > 160 && r < 80 && g < 80;
    }
    require(red > 1000 && green > 1000 && blue > 1000, "Diagnostic RGB bands missing");
}
void verifyDeniedJit(const char* rom, const fs::path& output, bool denyAfterLoad) {
#ifdef __linux__
    // Exercise a real kernel permission denial in a separate process before
    // the parent has made its static code cache executable. No production
    // override or exception handler conceals an attempt to execute NX memory.
    const pid_t child = ::fork();
    require(child >= 0, "Cannot fork denied-JIT test");
    if (child == 0) {
        try {
            const sock_filter instructions[] = {
                BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)),
                BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mprotect, 1, 0),
                BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mmap, 0, 3),
                BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, args[2])),
                BPF_JUMP(BPF_JMP | BPF_JSET | BPF_K, PROT_EXEC, 0, 1),
                BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EACCES),
                BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
            };
            sock_fprog filter{static_cast<unsigned short>(sizeof(instructions) / sizeof(instructions[0])),
                              const_cast<sock_filter*>(instructions)};
            const auto deny = [&filter] {
                require(::prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == 0 &&
                        ::prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &filter) == 0, "Cannot install NX permission test");
            };
            if (!denyAfterLoad) deny();
            const auto data = output / ("denied-jit-" + std::to_string(::getpid()));
            fs::create_directories(data / "logs");
            Log log;
            require(log.open(data.string()), "Cannot open denied-JIT log");
            Emulator core;
            GamepadInput input{};
            input.connected = true;
            std::string error;
            for (unsigned session = 0; session < 2; ++session) {
                require(core.load(rom, data.string(), log, error), "Denied-JIT load: " + error);
                // A late denial exercises the core's independent recheck even
                // though the frontend already selected dynamic_recompiler.
                if (denyAfterLoad && session == 0) deny();
                for (unsigned frame = 0; frame < 60; ++frame) require(core.run(input, error), error);
                require(!core.usingRecompiler(), "NX-denied session claimed an active recompiler");
                require(std::string(core.cpuName()) == "Interprete cacheado", "Fallback HUD reported wrong backend");
                require(ramWord(0x400) == 0x52324E36, "NX fallback did not execute the CPU diagnostic");
                verifyRgb(core.frame());
                require(!core.audio().empty(), "NX fallback lost audio");
                core.unload();
            }
            ::_exit(0);
        } catch (const std::exception& error) {
            std::cerr << "Denied-JIT child: " << error.what() << std::endl;
            ::_exit(1);
        }
    }
    int status = 0;
    while (::waitpid(child, &status, 0) < 0) require(errno == EINTR, "Cannot wait for denied-JIT test");
    require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "NX permission denial did not fall back safely");
#else
    (void)rom;
    (void)output;
    (void)denyAfterLoad;
#endif
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "Usage: emulator_tests diagnostic.z64 output-directory");
        const fs::path output = fs::absolute(argv[2]);
        fs::create_directories(output);
        verifyDeniedJit(argv[1], output, false);
        verifyDeniedJit(argv[1], output, true);
        // Keep runs independent without recursively deleting user-selected paths.
        const auto data = output / ("data-" + std::to_string(::getpid()));
        fs::create_directories(data);
        fs::create_directories(data / "logs");
        fs::create_directories(data / "saves");
        fs::path save;
        Log log;
        require(log.open(data.string()), "Cannot open test log");
        Emulator emulator;
        GamepadInput neutral{};
        neutral.connected = true;
        std::string error;

        require(!emulator.run(neutral, error), "run without ROM accepted");
        require(!emulator.load(argv[1], data.string(), log, error, EmulationConfig{0}), "Automatic worker count accepted");
        require(!emulator.load(argv[1], data.string(), log, error, EmulationConfig{4, static_cast<CpuMode>(99)}),
                "Invalid CPU choice accepted");
        const auto invalid = output / "invalid.z64";
        { std::ofstream file(invalid, std::ios::binary); file << "bad"; }
        require(!emulator.load(invalid.string(), data.string(), log, error), "Truncated ROM accepted");
        const auto oversized = output / "oversized.z64";
        { std::ofstream file(oversized, std::ios::binary); file.seekp(64 * 1024 * 1024 + 3); file.put(0); }
        require(!emulator.load(oversized.string(), data.string(), log, error), "Oversized ROM accepted");
        fs::remove(oversized);
        const auto link = output / "symlink.z64";
        fs::remove(link);
        fs::create_symlink(fs::absolute(argv[1]), link);
        require(!emulator.load(link.string(), data.string(), log, error), "Symlink ROM accepted");
        const auto unsafeSystem = output / ("linked-system-" + std::to_string(::getpid()));
        fs::create_directories(unsafeSystem / "system/Mupen64plus");
        const auto sentinel = output / "config-sentinel";
        { std::ofstream file(sentinel); file << "keep"; }
        fs::create_symlink(sentinel, unsafeSystem / "system/Mupen64plus/mupen64plus.ini");
        require(!emulator.load(argv[1], unsafeSystem.string(), log, error), "Linked system INI accepted");
        require(fs::file_size(sentinel) == 4, "Core overwrote the linked INI target");

        // Closing before the first retro_run must not enter EXECUTE or hang.
        require(emulator.load(argv[1], data.string(), log, error), "Pre-run load: " + error);
        emulator.unload();
        require(!emulator.loaded() && emulator.frame().pixels.empty(), "Early unload retained session");
        for (const auto& file : fs::directory_iterator(data / "saves/n64"))
            if (file.path().extension() == ".srm") save = file.path();
        require(!save.empty(), "Early unload did not create a save");

        size_t audioFrames = 0;
        unsigned nonSilent = 0;
        size_t saveSize = 0;
        CoreFrame reference;
        for (unsigned session = 0; session < 4; ++session) {
            // Both CPU engines must survive repeated sessions with both worker
            // counts; every new session restores the preceding engine's SRAM.
            const bool automatic = session % 2 != 0;
            const EmulationConfig config{session < 2 ? 4u : 1u,
                                         automatic ? CpuMode::Automatic : CpuMode::CachedInterpreter};
            require(emulator.load(argv[1], data.string(), log, error, config), "Load session " + std::to_string(session) + ": " + error);
            require(emulator.loaded(), "Missing loaded state");
            if (session > 0) {
                const auto* restored = static_cast<const unsigned char*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
                require(restored && restored[1370] == 0xA5, "SRAM did not survive unload/reload");
            }
            Emulator competing;
            require(!competing.load(argv[1], data.string(), log, error), "Concurrent session accepted");
            for (unsigned frame = 0; frame < 60; ++frame) {
                require(emulator.run(neutral, error), error);
                const auto& audio = emulator.audio();
                require(audio.size() % 2 == 0, "Non-stereo audio buffer");
                audioFrames += audio.size() / 2;
                nonSilent += static_cast<unsigned>(std::count_if(audio.begin(), audio.end(), [](int16_t sample) { return std::abs(int(sample)) > 100; }));
            }
            require(ramWord(0x400) == 0x52324E36, "MIPS diagnostic signature missing");
            require(emulator.usingRecompiler() == automatic, "Requested CPU backend did not actually execute");
            require(!emulator.supportsSaveStates(), "Hybrid N64 audio exposed incomplete savestates");
            verifyRgb(emulator.frame());
            if (reference.pixels.empty()) reference = emulator.frame();
            else require(reference.width == emulator.frame().width && reference.height == emulator.frame().height &&
                         reference.pixels == emulator.frame().pixels, "CPU backends produced different RGB pixels");
            require(ramWord(0x408) == 0, "Neutral controller was not neutral");
            GamepadInput pressed = neutral;
            pressed.buttons = 1u << RETRO_DEVICE_ID_JOYPAD_B;
            for (unsigned frame = 0; frame < 4; ++frame) require(emulator.run(pressed, error), error);
            require((ramWord(0x408) & 0xFFFF0000u) == 0x80000000u, "Libretro B did not reach N64 A via SI/PIF");
            pressed = neutral;
            pressed.buttons = (1u << RETRO_DEVICE_ID_JOYPAD_Y) | (1u << RETRO_DEVICE_ID_JOYPAD_L2) |
                              (1u << RETRO_DEVICE_ID_JOYPAD_START) | (1u << RETRO_DEVICE_ID_JOYPAD_UP);
            for (unsigned frame = 0; frame < 4; ++frame) require(emulator.run(pressed, error), error);
            require((ramWord(0x408) & 0xFFFF0000u) == 0x78000000u, "N64 B/Z/Start/Up mapping failed");
            pressed = neutral;
            pressed.analogX = 24000;
            pressed.analogY = -24000;
            for (unsigned frame = 0; frame < 4; ++frame) require(emulator.run(pressed, error), error);
            require((ramWord(0x408) & 0xFFFFu) != 0, "Analog input did not reach Joybus");
            pressed.analogX = -32768;
            pressed.analogY = -32768;
            for (unsigned frame = 0; frame < 4; ++frame) require(emulator.run(pressed, error), error);
            require((ramWord(0x408) & 0xFFFFu) != 0, "Analog corner overflowed");
            pressed = neutral;
            pressed.rightX = 32767;
            pressed.rightY = -32767;
            for (unsigned frame = 0; frame < 4; ++frame) require(emulator.run(pressed, error), error);
            require((ramWord(0x408) & 0xFFFF0000u) == 0x00090000u, "Right stick did not reach N64 C-right/C-up");
            pressed.connected = false;
            for (unsigned frame = 0; frame < 4; ++frame) require(emulator.run(pressed, error), error);
            require(ramWord(0x408) == 0, "Disconnected controller retained stale input");

            const size_t stateSize = retro_serialize_size();
            require(stateSize > 0 && stateSize < 64 * 1024 * 1024, "Invalid state size");
            std::vector<unsigned char> state(stateSize);
            require(retro_serialize(state.data(), state.size()), "State serialize failed");
            const auto counter = ramWord(0x404);
            for (unsigned frame = 0; frame < 3; ++frame) require(emulator.run(neutral, error), error);
            require(ramWord(0x404) != counter, "CPU did not advance");
            require(retro_unserialize(state.data(), state.size()), "State restore failed");
            require(ramWord(0x404) == counter, "State restored wrong RAM");
            require(emulator.run(neutral, error), error);

            auto* memory = static_cast<unsigned char*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
            saveSize = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
            require(memory && saveSize > 1370, "Missing save memory");
            memory[1370] = 0xA5;
            emulator.unload();
            require(!emulator.loaded(), "Unload retained loaded state");
            require(fs::file_size(save) == saveSize, "Missing full SRAM file");
            require(!fs::exists(save.string() + ".tmp"), "Atomic save left a temporary file");
        }
        require(audioFrames > 1000 && nonSilent > 1000, "Real AI audio did not reach the frontend");
        require(std::abs(emulator.sampleRate() - 44100) < 1, "Unexpected output sample rate");

        // Identical header CRCs must not merge different cartridge contents.
        std::ifstream source(argv[1], std::ios::binary);
        std::vector<unsigned char> variant((std::istreambuf_iterator<char>(source)), {});
        require(variant.size() == 4096, "Unexpected diagnostic size");
        variant[0xF00] ^= 0x7F; // Unused diagnostic padding, leaves its MIPS program intact.
        const auto different = output / "different-content-same-crc.z64";
        { std::ofstream file(different, std::ios::binary); file.write(reinterpret_cast<const char*>(variant.data()), variant.size()); }
        require(emulator.load(different.string(), data.string(), log, error), error);
        auto* alternateSave = static_cast<unsigned char*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
        require(alternateSave && alternateSave[1370] != 0xA5, "Different content reused the same header-CRC save");
        emulator.unload();
        size_t saveCount = 0;
        for (const auto& file : fs::directory_iterator(data / "saves/n64"))
            saveCount += file.path().extension() == ".srm";
        require(saveCount == 2, "Content variants did not get separate saves");

        // Normalized byte order must preserve saves for the same cartridge.
        variant[0xF00] ^= 0x7F;
        for (size_t i = 0; i < variant.size(); i += 2) std::swap(variant[i], variant[i + 1]);
        const auto byteSwapped = output / "same-content.v64";
        { std::ofstream file(byteSwapped, std::ios::binary); file.write(reinterpret_cast<const char*>(variant.data()), variant.size()); }
        require(emulator.load(byteSwapped.string(), data.string(), log, error), error);
        const auto* reorderedSave = static_cast<const unsigned char*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
        require(reorderedSave && reorderedSave[1370] == 0xA5, "Byte-swapped cartridge lost its save identity");
        emulator.unload();

        // Legacy flat N64 saves migrate on write without modifying the source.
        const auto legacy = data / "saves" / save.filename();
        fs::copy_file(save, legacy);
        fs::remove(save);
        require(emulator.load(argv[1], data.string(), log, error), error);
        auto* migrated = static_cast<unsigned char*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
        require(migrated && migrated[1370] == 0xA5, "Legacy N64 save was not restored");
        migrated[1370] = 0xA6;
        emulator.unload();
        { std::ifstream file(legacy, std::ios::binary); file.seekg(1370);
          require(file.get() == 0xA5, "Legacy N64 source was changed during migration"); }
        require(emulator.load(argv[1], data.string(), log, error), error);
        migrated = static_cast<unsigned char*>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
        require(migrated && migrated[1370] == 0xA6, "Legacy save overrode the new per-system save");
        emulator.unload();

        // Exercise the public versioned container and reset using complete LLE.
        const EmulationConfig lle{1, CpuMode::Automatic, false};
        require(emulator.load(argv[1], data.string(), log, error, lle), error);
        for (unsigned frame = 0; frame < 60; ++frame) require(emulator.run(neutral, error), error);
        require(emulator.system() == SystemType::Nintendo64 &&
                std::string(emulator.coreName()) == "Mupen64Plus-Next", "N64 descriptor lost by facade");
        require(emulator.supportsSaveStates(), "N64 LLE states unexpectedly unavailable");
        require(emulator.saveState(error), error);
        // Mupen serializes at its next safe interrupt boundary, reached by
        // resuming the core coroutine. Match that completed snapshot point.
        const auto savedCounter = ramWord(0x404);
        for (unsigned frame = 0; frame < 3; ++frame) require(emulator.run(neutral, error), error);
        require(ramWord(0x404) != savedCounter, "State probe did not advance");
        require(emulator.loadState(error), error);
        require(ramWord(0x404) == savedCounter, "Public N64 state did not restore RAM");
        require(emulator.audio().empty() && emulator.frame().pixels.empty(), "State retained stale output");
        require(emulator.reset(error), error);
        for (unsigned frame = 0; frame < 60; ++frame) require(emulator.run(neutral, error), error);
        require(ramWord(0x400) == 0x52324E36 && emulator.usingRecompiler(), "N64 full reset failed to restart JIT");
        verifyRgb(emulator.frame());
        emulator.unload();

        // Incompatible existing data is never silently destroyed on close.
        { std::ofstream file(save, std::ios::binary | std::ios::trunc); file << "preserve"; }
        require(emulator.load(argv[1], data.string(), log, error), "Load with invalid save failed: " + error);
        require(emulator.run(neutral, error), error);
        emulator.unload();
        require(fs::file_size(save) == 8, "Incompatible save was overwritten");
        std::cout << "PASS: 4 repeated cached/JIT sessions (4/4/1/1 workers), real NX-denied fallback, early unload, identical RGB, CPU/audio/input/state round trip, SRAM persistence/identity and invalid-save preservation. "
                  << audioFrames << " stereo audio frames; SRAM " << saveSize << " bytes.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: " << exception.what() << '\n';
        return 1;
    }
}
