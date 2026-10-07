#include "handheld_settings.h"
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace r2n64;
namespace fs = std::filesystem;
namespace {
void require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
bool equal(const HandheldSettings& a, const HandheldSettings& b) {
    return a.fastForward == b.fastForward && a.stateSlot == b.stateSlot &&
           a.integerScaling == b.integerScaling && a.linearFilter == b.linearFilter &&
           a.gbPalette == b.gbPalette && a.overlay == b.overlay && a.showStats == b.showStats &&
           a.shader == b.shader && a.gbaFrameskip == b.gbaFrameskip && a.gbColor == b.gbColor;
}
std::string read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void write(const fs::path& path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << contents;
    require(file.good(), "fixture write failed");
}
std::string replace(std::string source, const std::string& old, const std::string& value) {
    const auto start = source.find(old);
    require(start != std::string::npos, "test replacement missing");
    source.replace(start, old.size(), value);
    return source;
}
void noTemporaryFiles(const fs::path& directory) {
    for (const auto& entry : fs::directory_iterator(directory))
        require(entry.path().filename().string().find(".tmp.") == std::string::npos,
                "temporary settings file leaked");
}
}

int main() {
    char pattern[] = "/tmp/r2n64-settings-XXXXXX";
    const char* created = ::mkdtemp(pattern);
    if (!created) return 1;
    const fs::path temporary(created);
    try {
        const fs::path root = temporary / "data";
        const fs::path directory = root / "configs/systems";
        const fs::path gb = directory / "gb.json";
        const HandheldSettings defaults;
        HandheldSettings loaded{8, 4, false, true, 3};
        std::string error = "old error";
        require(loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error), "missing root failed");
        require(equal(loaded, defaults) && error.empty() && !fs::exists(root), "missing root changed disk/defaults");
        require(saveHandheldSettings(root.string(), SystemType::GameBoy, defaults, error), "default save failed");
        const std::string baseline = read(gb);
        require(!defaults.gbColor, "GB color must be opt-in");
        write(gb,replace(baseline, ",\n  \"gbColor\": false", ""));
        loaded.gbColor = true;
        require(loadHandheldSettings(root.string(),SystemType::GameBoy,loaded,error) && !loaded.gbColor,
                "Old preferences enabled GB color");
        HandheldSettings colored = defaults;
        colored.gbPalette = 2; colored.gbColor = true;
        require(saveHandheldSettings(root.string(),SystemType::GameBoy,colored,error), "Save GB color failed");
        require(loadHandheldSettings(root.string(),SystemType::GameBoy,loaded,error) && equal(loaded,colored),
                "Color toggle did not retain the original palette");
        colored.gbColor = false;
        require(saveHandheldSettings(root.string(),SystemType::GameBoy,colored,error), "Disable GB color failed");
        require(loadHandheldSettings(root.string(),SystemType::GameBoy,loaded,error) && loaded.gbPalette==2 && !loaded.gbColor,
                "Disabling color lost the base palette");
        write(gb,baseline);
        require(loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error) && equal(loaded, defaults),
                "default round trip failed");
        require(defaults.overlay, "overlay should be enabled by default");
        require(defaults.showStats, "performance HUD should be enabled by default");
        const auto previous = replace(replace(baseline, ",\n  \"shader\": 0", ""), ",\n  \"gbaFrameskip\": 0", "");
        write(gb, previous);
        loaded.shader = loaded.gbaFrameskip = 2;
        require(loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error) && equal(loaded, defaults),
                "previous preferences must default shaders and frameskip off");
        const std::string legacy = replace(replace(previous, ",\n  \"overlay\": true", ""),
                                           ",\n  \"showStats\": true", "");
        write(gb, legacy);
        loaded.overlay = loaded.showStats = false;
        require(loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error) && equal(loaded, defaults),
                "legacy version 1 settings did not default overlay and statistics on");
        require(read(gb) == legacy, "loading legacy settings rewrote them");
        const std::string overlayOnly = replace(baseline, ",\n  \"showStats\": true", "");
        write(gb, overlayOnly);
        loaded.showStats = false;
        require(loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error) && equal(loaded, defaults),
                "v0.3.2 settings did not default statistics on");
        write(gb, baseline);
        const HandheldSettings color{4, 3, false, true, 1, false, false, 1, 0};
        const HandheldSettings advance{8, 4, true, false, 3, true, true, 2, 2};
        require(saveHandheldSettings(root.string(), SystemType::GameBoyColor, color, error), "GBC save failed");
        require(saveHandheldSettings(root.string(), SystemType::GameBoyAdvance, advance, error), "GBA save failed");
        require(loadHandheldSettings(root.string(), SystemType::GameBoyColor, loaded, error) && equal(loaded, color),
                "GBC round trip failed");
        require(loadHandheldSettings(root.string(), SystemType::GameBoyAdvance, loaded, error) && equal(loaded, advance),
                "GBA round trip failed");
        require(read(gb) == baseline, "per-system settings mixed");
        for (const auto system : {SystemType::NintendoEntertainmentSystem, SystemType::SuperNintendo}) {
            HandheldSettings consoleDefaults;
            consoleDefaults.integerScaling = false;
            consoleDefaults.overlay = system == SystemType::SuperNintendo;
            require(loadHandheldSettings(root.string(), system, loaded, error) && equal(loaded, consoleDefaults),
                    "NES/SNES must default to 4:3; only SNES artwork defaults on");
            const auto file = directory / (std::string(systemId(system)) + ".json");
            require(!fs::exists(file), "loading console defaults should not write a file");
            const HandheldSettings changed{8, 4, true, true, 0, false, false};
            require(saveHandheldSettings(root.string(), system, changed, error), "console settings save failed");
            const auto explicitlyDisabled = read(file);
            require(loadHandheldSettings(root.string(), system, loaded, error) && equal(loaded, changed),
                    "explicitly disabled console artwork must remain off after reload");
            require(read(file) == explicitlyDisabled, "loading an existing preference must not migrate or overwrite it");
            write(file, legacy);
            require(loadHandheldSettings(root.string(), system, loaded, error) && loaded.showStats &&
                    loaded.overlay == consoleDefaults.overlay && loaded.integerScaling,
                    "legacy config must default artwork by system and preserve explicit scaling");
            require(read(file) == legacy, "loading optional defaults must not rewrite legacy console preferences");
            if (system == SystemType::SuperNintendo) {
                HandheldSettings enabled = consoleDefaults;
                enabled.fastForward = 4; enabled.stateSlot = 2;
                require(saveHandheldSettings(root.string(), system, enabled, error), "SNES artwork enable save failed");
                require(loadHandheldSettings(root.string(), system, loaded, error) && equal(loaded, enabled),
                        "SNES enabled artwork and 4:3 did not survive reload");
                enabled.overlay = false;
                require(saveHandheldSettings(root.string(), system, enabled, error), "SNES artwork disable save failed");
                require(loadHandheldSettings(root.string(), system, loaded, error) && equal(loaded, enabled),
                        "SNES artwork toggle failed to persist without changing other preferences");
            }
            write(file, "{}");
            require(!loadHandheldSettings(root.string(), system, loaded, error) && equal(loaded, consoleDefaults),
                    "invalid console settings must return console defaults");
            require(read(gb) == baseline, "console settings affected Game Boy configuration");
        }
        fs::remove(directory / "gbc.json");
        require(loadHandheldSettings(root.string(), SystemType::GameBoyColor, loaded, error) && equal(loaded, defaults),
                "missing file did not restore defaults");

        for (const auto system : {SystemType::Unknown, SystemType::Nintendo64, static_cast<SystemType>(99)}) {
            require(!loadHandheldSettings(root.string(), system, loaded, error) && equal(loaded, defaults),
                    "unsupported system load accepted");
            require(!saveHandheldSettings(root.string(), system, defaults, error), "unsupported system save accepted");
        }
        require(!fs::exists(directory / "n64.json"), "N64 settings were written");

        for (const auto& invalid : {
                 std::string(), std::string("{}"), std::string("[]"), baseline + "false",
                 replace(baseline, "\"version\": 1", "\"version\": 2"),
                 replace(baseline, "\"version\": 1,", ""),
                 replace(baseline, "\"fastForward\": 2", "\"fastForward\": 3"),
                 replace(baseline, "\"fastForward\": 2", "\"fastForward\": -2"),
                 replace(baseline, "\"fastForward\": 2", "\"fastForward\": 2.0"),
                 replace(baseline, "\"fastForward\": 2", "\"fastForward\": 2e0"),
                 replace(baseline, "\"fastForward\": 2", "\"fastForward\": 02"),
                 replace(baseline, "\"fastForward\": 2", "\"fastForward\": 4294967298"),
                 replace(baseline, "\"stateSlot\": 0", "\"stateSlot\": 5"),
                 replace(baseline, "\"gbPalette\": 0", "\"gbPalette\": 4"),
                 replace(baseline, "\"gbColor\": false", "\"gbColor\": 1"),
                 replace(baseline, "\"gbColor\": false", "\"gbColor\": \"true\""),
                 replace(baseline, "\"gbColor\": false", "\"gbColor\": false, \"gbColor\": true"),
                 replace(baseline, "\"shader\": 0", "\"shader\": 3"),
                 replace(baseline, "\"shader\": 0", "\"shader\": true"),
                 replace(baseline, "\"shader\": 0", "\"shader\": 1, \"shader\": 2"),
                 replace(baseline, "\"gbaFrameskip\": 0", "\"gbaFrameskip\": 3"),
                 replace(baseline, "\"gbaFrameskip\": 0", "\"gbaFrameskip\": -1"),
                 replace(baseline, "\"gbaFrameskip\": 0", "\"gbaFrameskip\": \"1\""),
                 replace(baseline, "\"integerScaling\": true", "\"integerScaling\": 1"),
                 replace(baseline, "\"linearFilter\": false", "\"linearFilter\": \"false\""),
                 replace(baseline, "\"overlay\": true", "\"overlay\": 1"),
                 replace(baseline, "\"overlay\": true", "\"overlay\": \"false\""),
                 replace(baseline, "\"overlay\": true", "\"overlay\": null"),
                 replace(baseline, "\"overlay\": true", "\"overlay\": true, \"overlay\": false"),
                 replace(baseline, "\"showStats\": true", "\"showStats\": 1"),
                 replace(baseline, "\"showStats\": true", "\"showStats\": \"false\""),
                 replace(baseline, "\"showStats\": true", "\"showStats\": null"),
                 replace(baseline, "\"showStats\": true", "\"showStats\": true, \"showStats\": false"),
                 replace(baseline, "\"stateSlot\": 0", "\"stateSlot\": 0, \"stateSlot\": 1"),
                 replace(baseline, "\"stateSlot\": 0", "\"stateSlot\": 0, \"state\\u0053lot\": 1"),
                 replace(baseline, "\"stateSlot\": 0", "\"stateSlot\": 0, \"future\": {}"),
                 replace(baseline, "\"stateSlot\": 0", "\"stateSlot\": 0, \"future\": []"),
                 replace(baseline, "\"stateSlot\": 0", "\"stateSlot\": 0, \"future\": \"\\ud800\""),
                 replace(baseline, "\"stateSlot\": 0", "\"stateSlot\": 0, \"future\": \"\xc0\x80\""),
                 replace(baseline, "\"gbPalette\": 0", "\"gbPalette\": 0,"),
                 std::string(4097, ' '), baseline + std::string(1, '\0')}) {
            write(gb, invalid);
            loaded = advance;
            require(!loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error), "invalid JSON accepted");
            require(equal(loaded, defaults) && !error.empty() && read(gb) == invalid,
                    "corrupt settings changed disk or returned partial values");
        }
        std::string future = replace(baseline, "\"gbPalette\": 0", "\"gbPalette\": 0, "
            "\"future\": \"escaped \\\"text\\\" \\u00e1 \\ud83c\\udfae\", "
            "\"flag\": true, \"count\": 1.2e+10, \"empty\": null");
        write(gb, future);
        require(loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error) && equal(loaded, defaults),
                "unknown scalar fields rejected");
        future += std::string(4096 - future.size(), ' ');
        write(gb, future);
        require(loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error), "4096 byte boundary rejected");
        write(gb, baseline);
        for (const auto& invalid : {HandheldSettings{1,0,true,false,0}, HandheldSettings{2,5,true,false,0},
                                    HandheldSettings{2,0,true,false,4}}) {
            require(!saveHandheldSettings(root.string(), SystemType::GameBoy, invalid, error), "invalid settings saved");
            require(read(gb) == baseline, "invalid save overwrote existing file");
        }

        // Holding the old file open proves replacement, not truncation in place.
        std::ifstream oldFile(gb);
        struct stat before{}, after{};
        require(::stat(gb.c_str(), &before) == 0, "stat before save failed");
        require(saveHandheldSettings(root.string(), SystemType::GameBoy, advance, error), "replacement save failed");
        require(::stat(gb.c_str(), &after) == 0 && before.st_ino != after.st_ino, "save was not atomic replacement");
        const std::string oldContents{std::istreambuf_iterator<char>(oldFile), std::istreambuf_iterator<char>()};
        require(oldContents == baseline, "open reader observed overwritten old file");
        require(loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error) && equal(loaded, advance),
                "new atomic file did not load");
        const auto saved = read(gb);
        const pid_t child = ::fork();
        require(child >= 0, "fork failed");
        if (child == 0) {
            std::signal(SIGXFSZ, SIG_IGN);
            const struct rlimit limit{0, 0};
            if (::setrlimit(RLIMIT_FSIZE, &limit) < 0) _exit(2);
            const bool accepted = saveHandheldSettings(root.string(), SystemType::GameBoy, defaults, error);
            _exit(!accepted && read(gb) == saved ? 0 : 3);
        }
        int status = 0;
        require(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
                "failed write did not preserve old settings");
        noTemporaryFiles(directory);

        const auto outside = temporary / "outside.json";
        write(outside, baseline);
        fs::remove(gb);
        fs::create_symlink(outside, gb);
        require(!loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error), "symlink load accepted");
        require(!saveHandheldSettings(root.string(), SystemType::GameBoy, defaults, error), "symlink save accepted");
        require(read(outside) == baseline && fs::is_symlink(gb), "symlink target changed");
        fs::remove(gb);
        fs::create_symlink(temporary / "missing.json", gb);
        require(!loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error), "dangling link treated as absent");
        require(!saveHandheldSettings(root.string(), SystemType::GameBoy, defaults, error), "dangling link replaced");
        fs::remove(gb);
        fs::create_hard_link(outside, gb);
        require(!loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error), "hard link load accepted");
        require(!saveHandheldSettings(root.string(), SystemType::GameBoy, defaults, error), "hard link save accepted");
        fs::remove(gb);
        require(::mkfifo(gb.c_str(), 0600) == 0, "fifo fixture failed");
        require(!loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error), "fifo load accepted");
        require(!saveHandheldSettings(root.string(), SystemType::GameBoy, defaults, error), "fifo save accepted");
        fs::remove(gb);
        fs::create_directory(gb);
        require(!loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error), "directory file load accepted");
        require(!saveHandheldSettings(root.string(), SystemType::GameBoy, defaults, error), "directory file save accepted");
        fs::remove(gb);

        for (const auto& component : {root, root / "configs", directory}) {
            const auto original = fs::path(component.string() + ".real");
            fs::rename(component, original);
            fs::create_directory_symlink(original, component);
            require(!loadHandheldSettings(root.string(), SystemType::GameBoy, loaded, error), "parent symlink load accepted");
            require(!saveHandheldSettings(root.string(), SystemType::GameBoy, defaults, error), "parent symlink save accepted");
            fs::remove(component);
            fs::rename(original, component);
        }
        require(!saveHandheldSettings("", SystemType::GameBoy, defaults, error), "empty root accepted");
        const auto traversal = temporary / "should-not-create/../data";
        require(!loadHandheldSettings(traversal.string(), SystemType::GameBoy, loaded, error), "missing traversal root accepted");
        require(!saveHandheldSettings(traversal.string(), SystemType::GameBoy, defaults, error), "traversal root saved");
        require(!fs::exists(temporary / "should-not-create"), "invalid root partially created directories");
        require(std::string(gbPaletteName(0)) == "Gris" && std::string(gbPaletteName(1)) == "Verde cl\xc3\xa1sico" &&
                std::string(gbPaletteName(2)) == "Oliva" && std::string(gbPaletteName(3)) == "Turquesa" &&
                std::string(gbPaletteName(4)) == "Desconocida", "palette names incorrect");
        fs::remove_all(temporary);
        std::cout << "Handheld settings: round trips, validation, atomic writes and link guards passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\nFixtures: " << temporary << '\n';
        return 1;
    }
}
