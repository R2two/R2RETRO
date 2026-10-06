#pragma once
#include <filesystem>
#include <string>
namespace pokemon3d {
// Lab-only preference; independent of the cartridge and emulator state.
bool loadViewPreference(const std::filesystem::path& data, bool& enabled, std::string& error);
bool saveViewPreference(const std::filesystem::path& data, bool enabled, std::string& error);
}
