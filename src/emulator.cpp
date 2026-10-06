#include "emulator.h"
#include "core/core_manager.h"

namespace r2n64 {
struct Emulator::State {
    CoreManager manager;
    CoreFrame emptyFrame;
    std::vector<int16_t> emptyAudio;
};
Emulator::Emulator() : state_(new State) {}
Emulator::~Emulator() { unload(); }
bool Emulator::load(const std::string& path, const std::string& data, Log& log,
                    std::string& error, EmulationConfig config) {
    return state_->manager.load(path, data, log, error, config);
}
bool Emulator::run(const GamepadInput& input, std::string& error) {
    auto* core = state_->manager.active();
    if (!core) { error = "No hay ROM cargada."; return false; }
    return core->run(input, error);
}
void Emulator::unload() { state_->manager.unload(); }
bool Emulator::loaded() const {
    const auto* core = state_->manager.active();
    return core && core->loaded();
}
const CoreFrame& Emulator::frame() const {
    const auto* core = state_->manager.active();
    return core ? core->frame() : state_->emptyFrame;
}
const std::vector<int16_t>& Emulator::audio() const {
    const auto* core = state_->manager.active();
    return core ? core->audio() : state_->emptyAudio;
}
double Emulator::fps() const {
    const auto* core = state_->manager.active();
    return core ? core->fps() : 60.0;
}
double Emulator::sampleRate() const {
    const auto* core = state_->manager.active();
    return core ? core->sampleRate() : 44100.0;
}
SystemType Emulator::system() const {
    const auto* core = state_->manager.active();
    return core ? core->system() : SystemType::Unknown;
}
const char* Emulator::coreName() const {
    const auto* core = state_->manager.active();
    return core ? core->coreName() : "Sin nucleo";
}
bool Emulator::usingRecompiler() const {
    const auto* core = state_->manager.active();
    return core && core->usingRecompiler();
}
const char* Emulator::cpuName() const {
    const auto* core = state_->manager.active();
    return core ? core->cpuName() : "Sin sesion";
}
uint64_t Emulator::audioHleTasks() const {
    const auto* core = state_->manager.active();
    return core ? core->audioHleTasks() : 0;
}
R2N64CoreProfile Emulator::coreProfile() const {
    const auto* core = state_->manager.active();
    return core ? core->coreProfile() : R2N64CoreProfile{};
}
uint64_t Emulator::graphicsHleTasks() const {
    const auto* core = state_->manager.active(); return core ? core->graphicsHleTasks() : 0;
}
uint64_t Emulator::graphicsLleTasks() const {
    const auto* core = state_->manager.active(); return core ? core->graphicsLleTasks() : 0;
}
HardwareTiming Emulator::hardwareTiming() const {
    const auto* core = state_->manager.active(); return core ? core->hardwareTiming() : HardwareTiming{};
}
bool Emulator::supportsSaveStates() const {
    const auto* core = state_->manager.active();
    return core && core->supportsSaveStates();
}
bool Emulator::saveState(std::string& error, unsigned slot) {
    auto* core = state_->manager.active();
    if (!core) { error = "No hay ROM cargada."; return false; }
    return core->saveState(error, slot);
}
bool Emulator::loadState(std::string& error, unsigned slot) {
    auto* core = state_->manager.active();
    if (!core) { error = "No hay ROM cargada."; return false; }
    return core->loadState(error, slot);
}
bool Emulator::setGameBoyPalette(unsigned palette, std::string& error) {
    auto* core = state_->manager.active();
    if (!core) { error = "No hay ROM cargada."; return false; }
    return core->setGameBoyPalette(palette, error);
}
bool Emulator::reset(std::string& error) {
    auto* core = state_->manager.active();
    if (!core) { error = "No hay ROM cargada."; return false; }
    return core->reset(error);
}
}
