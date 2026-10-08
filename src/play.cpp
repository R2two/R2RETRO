#include "app.h"
#include "startup.h"
#include "performance.h"
#include "playback_speed.h"
#include "handheld_settings.h"
#include "gpu_session.h"
#include "frontend/system_detector.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace r2n64 {
namespace {
std::pair<std::string, std::string> profileText(const R2N64CoreProfile& p, bool gpu = false) {
    if (p.timer_failures || p.dropped_scopes)
        return {"Perfil incompleto: revisa el log de medición.", "No usar estos tiempos para comparar rendimiento."};
    if (!p.run_calls) return {"Perfil: esperando intervalos de emulación…", ""};
    const double perVI = 0.001 / static_cast<double>(p.run_calls);
    const double components = static_cast<double>(p.rsp_us) + p.rdp_us + p.scanout_us + p.audio_hle_us;
    const double remainder = std::max(0.0, static_cast<double>(p.run_us) - components);
    char first[240], second[240];
    if (gpu) std::snprintf(first, sizeof(first), "Perfil host: RSP (incluye llamadas GL) %.2f ms/VI", p.rsp_us * perVI);
    else std::snprintf(first, sizeof(first), "Perfil medio: RSP %.2f · RDP %.2f · Vídeo %.2f ms/VI",
                       p.rsp_us * perVI, p.rdp_us * perVI, p.scanout_us * perVI);
    std::snprintf(second, sizeof(second), "Audio HLE %.2f · CPU y resto %.2f ms/VI · %llu VI",
                  p.audio_hle_us * perVI, remainder * perVI, static_cast<unsigned long long>(p.run_calls));
    return {first, second};
}
}
bool App::play(const std::string& path, const std::string& title, std::string& error,
               bool smoke, const std::string& screenshot, unsigned smokeFrames) {
    constexpr SDL_Color white{245,249,255,255}, muted{186,203,222,255}, accent{169,234,217,255};
    video_.clear(1);
    video_.text("Iniciando…", 120, 110, white, 42);
    video_.text(title, 120, 180, muted, 28);
    if (!video_.present(error)) return false;
    log_.write("INFO", "Loading ROM: " + path);
    startupLog("Emulation load begin", path.c_str());
    const auto requestedSystem = detectSystemFromExtension(path);
    HandheldSettings handheld;
    std::string settingsError;
    auto config = emulationConfig_;
    GpuSession gpu(video_.window(), video_.renderer());
    // A core borrowing this host must stop before the local GPU resources,
    // including when a C++ allocation or log operation throws.
    struct SessionGuard { Emulator& emulator; ~SessionGuard() { emulator.unload(); } } guard{emulator_};
    std::string gpuNotice;
    if (requestedSystem == SystemType::Nintendo64 && (config.graphics == N64Graphics::Gles2 || config.automaticProfile))
        config.hardware = &gpu;
    if (requestedSystem != SystemType::Unknown && requestedSystem != SystemType::Nintendo64) {
        if (!loadHandheldSettings(platform_.dataPath(), requestedSystem, handheld, settingsError))
            log_.write("WARNING", "Preferencias del sistema: " + settingsError);
        config.gbPalette = requestedSystem == SystemType::GameBoy && handheld.gbColor ?
            GameBoyColorPalette : handheld.gbPalette;
        config.gbaFrameskip = handheld.gbaFrameskip;
    }
    if (!emulator_.load(path, platform_.dataPath(), log_, error, config)) {
        if (!config.hardware || emulator_.effectiveConfig().graphics != N64Graphics::Gles2) return false;
        config = emulator_.effectiveConfig();
        gpuNotice = "GPU no disponible: " + error + " · Se usa Angrylion CPU.";
        log_.write("WARNING", gpuNotice);
        gpu.reset();
        config.graphics = N64Graphics::Software;
        config.automaticProfile = false; // A failed auto GPU profile must not select GPU again.
        config.hardware = nullptr;
        if (!emulator_.load(path, platform_.dataPath(), log_, error, config)) return false;
    }
    config = emulator_.effectiveConfig();
    if (config.graphics == N64Graphics::Gles2)
        log_.write("INFO", "GPU driver: " + gpu.driverDescription());
    const bool extended = emulator_.system() != SystemType::Nintendo64;
    const bool handheldSystem = emulator_.system() == SystemType::GameBoy ||
        emulator_.system() == SystemType::GameBoyColor || emulator_.system() == SystemType::GameBoyAdvance;
    const bool snes = emulator_.system() == SystemType::SuperNintendo;
    const bool nes = emulator_.system() == SystemType::NintendoEntertainmentSystem;
    const bool overlaySystem = handheldSystem || snes || nes;
    std::string shaderError;
    if (!video_.setDisplayShader(extended ? handheld.shader : 0, shaderError))
        log_.write("WARNING", shaderError);
    struct ShaderGuard {
        Video& video;
        ~ShaderGuard() { std::string ignored; video.setDisplayShader(0, ignored); }
    } shaderGuard{video_};
    if (extended) log_.write("INFO", "Preferencias " + std::string(systemId(emulator_.system())) +
        ": avance=" + std::to_string(handheld.fastForward) + "x; espacio=" + std::to_string(handheld.stateSlot + 1) +
        "; entero=" + std::to_string(handheld.integerScaling) + "; suavizado=" + std::to_string(handheld.linearFilter) +
        "; paleta=" + std::to_string(handheld.gbPalette) + "; marco=" + std::to_string(handheld.overlay) +
        "; estadísticas=" + std::to_string(handheld.showStats) +
        "; colorGB=" + std::to_string(emulator_.system() == SystemType::GameBoy && handheld.gbColor));
    // Asset I/O and texture creation belong to loading or the paused menu,
    // never to the active emulation/frame path. A missing frame is non-fatal.
    std::string overlayError;
    if (!video_.setHandheldOverlay(overlaySystem ? emulator_.system() : SystemType::Unknown,
                                  overlaySystem && handheld.overlay, overlayError))
        log_.write("WARNING", "Marco no disponible; se usa la imagen sin marco: " + overlayError);
    if (overlaySystem) log_.write("INFO", "Marco " + std::string(systemId(emulator_.system())) +
        ": preferencia=" + std::to_string(handheld.overlay) + "; activo=" +
        std::to_string(video_.handheldOverlayActive()));
    startupLog("Emulation core loaded");
    double audioRate = emulator_.sampleRate();
    std::string audioError;
    bool sound = audio_.start(audioRate, audioError);
    if (!sound) log_.write("WARNING", "Playing without audio: " + audioError);
    // The emulation deadline below already limits speed. Waiting for a second
    // display clock quantizes a late 20 ms VI to 33 ms on a 60 Hz screen.
    // Preserve the known working startup renderer and restore its VSync later.
    std::string syncError;
    const bool vsyncChanged = video_.setVSync(false, syncError);
    log_.write(vsyncChanged ? "INFO" : "WARNING", vsyncChanged ?
        "Emulation presentation: VSync off; paced by core video intervals" :
        "Could not disable emulation VSync: " + syncError);
    bool paused = false, menuWasHeld = false, acceptingInput = false, ok = true;
    bool captureRequested = false;
    unsigned pauseSelection = 0, frames = 0;
    enum class PauseAction { Continue, Slot, SaveState, LoadState, Battery, Reset, Speed, Scale, Filter, Shader, Frameskip, Overlay, Stats, Palette, Color, Capture, Close };
    std::vector<std::pair<PauseAction, std::string>> pauseItems{{PauseAction::Continue, "Continuar"}};
    const auto refreshPauseItems = [&]() {
        pauseItems.resize(1);
        if (emulator_.supportsSaveStates()) {
            if (extended) pauseItems.emplace_back(PauseAction::Slot, "Espacio de estado: " + std::to_string(handheld.stateSlot + 1) + " / 5");
            pauseItems.emplace_back(PauseAction::SaveState, "Guardar estado");
            pauseItems.emplace_back(PauseAction::LoadState, "Cargar estado");
        }
        pauseItems.emplace_back(PauseAction::Reset, "Reiniciar juego");
        if (extended) {
            pauseItems.emplace_back(PauseAction::Battery, "Escribir partida SRAM/RTC");
            const char* shaderName = handheld.shader == 1 ? "LCD suave" : handheld.shader == 2 ? "CRT suave" : "desactivado";
            pauseItems.emplace_back(PauseAction::Shader, std::string("Shader: ") + shaderName +
                (handheld.shader && !video_.displayShaderMode() ? " (no disponible)" : ""));
            if (emulator_.system() == SystemType::GameBoyAdvance)
                pauseItems.emplace_back(PauseAction::Frameskip, handheld.gbaFrameskip ?
                    "Salto GBA: dibujar 1 de cada " + std::to_string(handheld.gbaFrameskip + 1) + " cuadros" :
                    "Salto GBA: desactivado");
            pauseItems.emplace_back(PauseAction::Speed, "Avance rápido: " + std::to_string(handheld.fastForward) + "x · mantener R2");
            if (overlaySystem)
                pauseItems.emplace_back(PauseAction::Overlay, !handheld.overlay ? "Marco: desactivado" :
                    video_.handheldOverlayActive() ? "Marco: activado" : "Marco: no disponible");
            pauseItems.emplace_back(PauseAction::Scale, handheldSystem ?
                (handheld.integerScaling ? "Imagen: escala entera" : "Imagen: ajustar con proporción") :
                (handheld.integerScaling ? "Imagen: píxeles cuadrados (entera)" : "Imagen: proporción 4:3"));
            pauseItems.emplace_back(PauseAction::Filter, handheld.linearFilter ? "Filtro: suavizado" : "Filtro: píxeles nítidos");
            pauseItems.emplace_back(PauseAction::Stats, handheld.showStats ? "Estadísticas: activadas" : "Estadísticas: desactivadas");
            if (emulator_.system() == SystemType::GameBoy) {
                pauseItems.emplace_back(PauseAction::Color, handheld.gbColor ? "Color GB: activado (4 tonos)" : "Color GB: desactivado");
                pauseItems.emplace_back(PauseAction::Palette, std::string(handheld.gbColor ? "Paleta al desactivar color: " : "Paleta: ") + gbPaletteName(handheld.gbPalette));
            }
        }
        pauseItems.emplace_back(PauseAction::Capture, "Capturar imagen");
        pauseItems.emplace_back(PauseAction::Close, "Volver a la biblioteca");
        pauseSelection = std::min<unsigned>(pauseSelection, unsigned(pauseItems.size() - 1));
    };
    std::string pauseNotice = settingsError.empty() ? "" : "Preferencias no válidas: se usan valores iniciales.";
    if (!gpuNotice.empty()) pauseNotice = gpuNotice;
    if (!overlayError.empty()) pauseNotice = overlayError;
    if (!shaderError.empty()) pauseNotice = shaderError;
    PlaybackSpeed speed;
    unsigned acceleratedSteps = 0, speedTransitions = 0;
    bool audioResumedAfterFastForward = false;
    size_t samples = 0;
    const double clockRate = static_cast<double>(SDL_GetPerformanceFrequency());
    auto clockMs = [clockRate]() { return SDL_GetPerformanceCounter() * 1000.0 / clockRate; };
    double deadline = clockMs();
    EmulationPerformance intervalPerformance, sessionPerformance;
    uint64_t uploadedSerial = 0;
    // A newly loaded game must not borrow the last session's software texture.
    video_.releaseGameFrame();
    std::string performanceText = "Midiendo emulación…";
    std::string imageRateText = "Entregas de imagen: midiendo…";
    std::string performanceDetail;
    std::string videoDetail;
    unsigned inspectedVideoFrame = ~0u;
    std::string lastOverlayDetail;
    double nextPerformanceLog = 10000;
    const auto logPerformance = [this, extended, &config](const EmulationPerformance& stats, const char* label) {
        if (!stats.intervals) return;
        char message[256];
        std::snprintf(message, sizeof(message),
            "%s: %.1f%% speed, %.1f %s, core %.2f ms/%s, present %.2f ms/%s, %.1f active seconds",
            label, stats.speedPercent(), stats.intervalsPerSecond(), extended ? "FPS" : "VI/s",
            stats.averageCoreMs(), extended ? "frame" : "VI", stats.averagePresentMs(),
            extended ? "frame" : "VI", stats.elapsedMs / 1000.0);
        log_.write("INFO", message);
        std::snprintf(message, sizeof(message), "Video delivery: %.1f steps/s with a new image; %llu/%llu emulated steps (not unique-pixel FPS)",
            stats.imagesPerSecond(), static_cast<unsigned long long>(stats.imageSteps),
            static_cast<unsigned long long>(stats.intervals));
        log_.write("INFO", message);
        if (!extended && config.profileCore) {
            std::snprintf(message, sizeof(message),
                "N64 pacing: over budget %llu/%llu VI; peak core %.3f ms, peak present %.3f ms; profile=%s",
                static_cast<unsigned long long>(stats.overBudgetSteps),
                static_cast<unsigned long long>(stats.measuredSingleSteps),
                stats.peakCoreMs, stats.peakPresentMs, emulator_.profileName());
            log_.write("INFO", message);
        }
        log_.write("INFO", std::string("CPU: ") + emulator_.cpuName() +
            "; recognized audio tasks accelerated: " + std::to_string(emulator_.audioHleTasks()));
        if (emulationConfig_.profileCore && emulator_.system() == SystemType::Nintendo64) {
            const auto profile = emulator_.coreProfile();
            const auto labels = profileText(profile, config.graphics == N64Graphics::Gles2);
            log_.write("INFO", labels.first + "; " + labels.second);
            char detail[400];
            std::snprintf(detail, sizeof(detail),
                "Core profile us: run=%llu rsp=%llu rdp=%llu scanout=%llu audio_hle=%llu; timer_failures=%llu dropped_scopes=%llu",
                static_cast<unsigned long long>(profile.run_us), static_cast<unsigned long long>(profile.rsp_us),
                static_cast<unsigned long long>(profile.rdp_us), static_cast<unsigned long long>(profile.scanout_us),
                static_cast<unsigned long long>(profile.audio_hle_us), static_cast<unsigned long long>(profile.timer_failures),
                static_cast<unsigned long long>(profile.dropped_scopes));
            log_.write("INFO", detail);
            const auto hw = emulator_.hardwareTiming();
            if (hw.calls) {
                std::snprintf(detail, sizeof(detail), "GPU boundary: begin %.3f ms/VI end %.3f ms/VI calls %llu; graphics HLE %llu LLE fallback %llu",
                    hw.beginMs/hw.calls, hw.endMs/hw.calls, static_cast<unsigned long long>(hw.calls),
                    static_cast<unsigned long long>(emulator_.graphicsHleTasks()), static_cast<unsigned long long>(emulator_.graphicsLleTasks()));
                log_.write("INFO", detail);
            }
        }
    };
    while (!quitting_) {
        const double frameBegin = clockMs();
        double coreMs = 0;
        const auto input = platform_.poll();
        if (input.quit) { quitting_ = true; break; }
        const bool menuHeld = extended ? input.gamepad.quickMenu : input.gamepad.menu;
        const bool menuEdge = menuHeld && !menuWasHeld;
        menuWasHeld = menuHeld;
        bool connected = true;
#ifdef R2N64_PS4
        connected = input.gamepad.connected;
#endif
        if (menuEdge || (!connected && !paused)) {
            paused = !paused; pauseSelection = 0;
            if (!connected) paused = true;
            // mGBA computes serialize_size by creating a snapshot; query this
            // only when opening the menu, never in the active frame loop.
            if (paused) {
                refreshPauseItems();
                if (!video_.displayShaderError().empty()) pauseNotice = video_.displayShaderError();
            }
            acceptingInput = false;
            audio_.pause(paused); audio_.clear(); deadline = clockMs();
        }
        if (paused && !menuEdge) {
            if ((input.pressed & Up) && pauseSelection) --pauseSelection;
            if ((input.pressed & Down) && pauseSelection + 1 < pauseItems.size()) ++pauseSelection;
            if (input.pressed & Confirm) {
                const auto action = pauseItems[pauseSelection].first;
                if (action == PauseAction::Close) break;
                if (action == PauseAction::Continue && connected) {
                    paused = false; acceptingInput = false; audio_.pause(false); deadline = clockMs();
                } else if (action == PauseAction::Slot || action == PauseAction::Speed ||
                           action == PauseAction::Scale || action == PauseAction::Filter ||
                           action == PauseAction::Shader || action == PauseAction::Frameskip ||
                           action == PauseAction::Overlay || action == PauseAction::Stats || action == PauseAction::Palette || action == PauseAction::Color) {
                    auto next = handheld;
                    if (action == PauseAction::Slot) next.stateSlot = (next.stateSlot + 1) % 5;
                    if (action == PauseAction::Speed) next.fastForward = next.fastForward == 2 ? 4 : next.fastForward == 4 ? 8 : 2;
                    if (action == PauseAction::Scale) next.integerScaling = !next.integerScaling;
                    if (action == PauseAction::Filter) next.linearFilter = !next.linearFilter;
                    if (action == PauseAction::Overlay) next.overlay = !next.overlay;
                    if (action == PauseAction::Stats) next.showStats = !next.showStats;
                    if (action == PauseAction::Palette) next.gbPalette = (next.gbPalette + 1) % 4;
                    if (action == PauseAction::Color) next.gbColor = !next.gbColor;
                    if (action == PauseAction::Shader) next.shader = (next.shader + 1) % 3;
                    if (action == PauseAction::Frameskip) next.gbaFrameskip = (next.gbaFrameskip + 1) % 3;
                    std::string settingError;
                    const bool applied = ((action != PauseAction::Palette && action != PauseAction::Color) ||
                        emulator_.setGameBoyPalette(next.gbColor ? GameBoyColorPalette : next.gbPalette, settingError)) &&
                        (action != PauseAction::Frameskip || emulator_.setGbaFrameskip(next.gbaFrameskip, settingError));
                    if (applied) {
                        handheld = next;
                        bool shaderAvailable = true;
                        if (action == PauseAction::Shader) {
                            shaderAvailable = video_.setDisplayShader(handheld.shader, shaderError);
                            if (!shaderAvailable) log_.write("WARNING", shaderError);
                        }
                        bool overlayAvailable = true;
                        if (action == PauseAction::Overlay) {
                            overlayAvailable = video_.setHandheldOverlay(emulator_.system(), handheld.overlay, overlayError);
                            if (!overlayAvailable)
                                log_.write("WARNING", "Marco no disponible; se usa la imagen sin marco: " + overlayError);
                            log_.write("INFO", "Marco " + std::string(systemId(emulator_.system())) +
                                ": preferencia=" + std::to_string(handheld.overlay) + "; activo=" +
                                std::to_string(video_.handheldOverlayActive()));
                        }
                        const bool persisted = saveHandheldSettings(platform_.dataPath(), emulator_.system(), handheld, settingError);
                        pauseNotice = persisted ? (action == PauseAction::Palette ? "Preferencia guardada. La paleta cambia al continuar." :
                            "Preferencia guardada para este sistema.") : "Aplicado a esta sesión. No se pudo guardar: " + settingError;
                        if (!persisted) log_.write("WARNING", pauseNotice);
                        if (!overlayAvailable) pauseNotice = overlayError +
                            (persisted ? "" : " · No se pudo guardar la preferencia.");
                        if (!shaderAvailable) pauseNotice = shaderError;
                        if (persisted && action == PauseAction::Frameskip) pauseNotice =
                            "Aplicado al continuar. Reduce dibujo, no la velocidad objetivo; puede verse menos fluido.";
                        if (persisted && action == PauseAction::Color) pauseNotice = handheld.gbColor ?
                            "Colorización de 4 tonos al continuar. No añade gráficos de Game Boy Color." :
                            "Se recuperará tu paleta anterior al continuar.";
                        if (persisted && action == PauseAction::Palette && handheld.gbColor)
                            pauseNotice = "Paleta guardada para cuando desactives Color GB.";
                        refreshPauseItems();
                    } else pauseNotice = settingError;
                } else if (action == PauseAction::Battery) {
                    std::string saveError;
                    const bool saved = emulator_.saveBattery(saveError);
                    pauseNotice = saved ? "SRAM/RTC escrita. Para registrar progreso usa Guardar dentro del juego." : saveError;
                    log_.write(saved ? "INFO" : "WARNING", pauseNotice);
                } else if (action == PauseAction::Capture) {
                    // Render the paused game and frame first; never capture the
                    // pause menu, advance emulation, or trigger a state load.
                    captureRequested = true;
                } else if (action != PauseAction::Continue) {
                    std::string stateError;
                    const bool success = action == PauseAction::SaveState ? emulator_.saveState(stateError, extended ? handheld.stateSlot : 0) :
                        action == PauseAction::LoadState ? emulator_.loadState(stateError, extended ? handheld.stateSlot : 0) : emulator_.reset(stateError);
                    pauseNotice = success ? (action == PauseAction::SaveState ? "Estado guardado." :
                        action == PauseAction::LoadState ? "Estado restaurado." : "Juego reiniciado.") : stateError;
                    log_.write(success ? "INFO" : "ERROR", pauseNotice);
                    audio_.clear(); deadline = clockMs(); acceptingInput = false;
                    if (success && action != PauseAction::SaveState) {
                        intervalPerformance = {}; sessionPerformance = {}; nextPerformanceLog = 10000;
                        performanceText = "Midiendo emulación…"; performanceDetail.clear();
                        imageRateText = "Entregas de imagen: midiendo…";
                    }
                }
            } else if ((input.pressed & Back) && connected) {
                paused = false; acceptingInput = false; audio_.pause(false); deadline = clockMs();
            }
        }
        unsigned completedSteps = 0, freshImageSteps = 0;
        auto gamepad = input.gamepad;
        if (!paused) {
            if (extended) {
                gamepad.buttons &= uint16_t(~((1u << 3) | (1u << 2)));
                if (gamepad.start) gamepad.buttons |= 1u << 3;
                if (gamepad.select) gamepad.buttons |= 1u << 2;
            }
            if (!gamepad.buttons && !menuHeld && (!snes || (!gamepad.faceEast && !gamepad.faceNorth))) acceptingInput = true;
            if (!acceptingInput) {
                gamepad.buttons = 0; gamepad.start = gamepad.select = false;
                gamepad.fastForward = false;
                gamepad.faceEast = gamepad.faceNorth = false;
                gamepad.analogX = gamepad.analogY = gamepad.rightX = gamepad.rightY = 0;
            }
            if (smoke) {
                gamepad.connected = true; gamepad.buttons = frames > 30 ? 1 : 0;
                gamepad.fastForward = extended && frames >= 240 && frames < 480;
            }
        }
        if (speed.update(extended, !paused && connected, gamepad.fastForward, handheld.fastForward)) {
            ++speedTransitions;
            audio_.pause(paused || speed.accelerated());
            audio_.clear(); deadline = clockMs(); intervalPerformance = {};
            performanceText = "Midiendo emulación…"; performanceDetail.clear();
            imageRateText = "Entregas de imagen: midiendo…";
            log_.write("INFO", "Velocidad solicitada: " + std::to_string(speed.factor()) + "x" +
                (speed.accelerated() ? "; audio silenciado mientras R2 está pulsado" : "; audio normal"));
        }
        if (!paused) {
            const double batchBegin = clockMs();
            for (unsigned step = 0; step < speed.factor(); ++step) {
            const double coreBegin = clockMs();
            const uint64_t priorSerial = emulator_.frame().serial;
            if (!emulator_.run(gamepad, error)) { ok = false; break; }
            if (emulator_.frame().serial && emulator_.frame().serial != priorSerial) ++freshImageSteps;
            coreMs += clockMs() - coreBegin;
            const auto& pcm = emulator_.audio();
            samples += pcm.size();
            if (sound && !speed.accelerated() && emulator_.sampleRate() != audioRate) {
                audioRate = emulator_.sampleRate();
                sound = audio_.start(audioRate, audioError);
                if (!sound) log_.write("WARNING", "Audio rate change failed: " + audioError);
            }
            if (sound && !speed.accelerated()) {
                audio_.push(pcm.data(), pcm.size() / 2);
                if (!audio_.ready()) { sound = false; log_.write("ERROR", audio_.error()); }
                if (acceleratedSteps && sound && audio_.queuedFrames()) audioResumedAfterFastForward = true;
            }
            if (smoke && speed.accelerated() && audio_.queuedFrames()) {
                error = "El avance rápido dejó audio en cola"; ok = false; break;
            }
            ++frames; ++completedSteps;
            if (speed.accelerated()) ++acceleratedSteps;
            // Bound a batch so input and pause continue to be polled promptly.
            if ((smoke && frames >= smokeFrames) || clockMs() - batchBegin >= 8.0) break;
            }
            if (!ok) break;
            if (smoke && frames >= smokeFrames) { paused = true; refreshPauseItems(); }
        }
        const auto& frame = emulator_.frame();
        // Inspect only on pause (or the first output), never every active
        // frame. This distinguishes a black core buffer from presentation
        // failure on hardware that cannot be reproduced on the desktop.
        if ((paused || frames == 1 || frames == 180) && inspectedVideoFrame != frames) {
            inspectedVideoFrame = frames;
            const auto visible = std::count_if(frame.pixels.begin(), frame.pixels.end(),
                [](uint32_t pixel) { return (pixel & 0xffffffu) != 0; });
            videoDetail = frame.hardware ? "Imagen GPU: " + std::to_string(frame.width) + "×" + std::to_string(frame.height) :
                frame.pixels.empty() ? "Núcleo: sin imagen recibida" :
                "Imagen " + std::to_string(frame.width) + "×" + std::to_string(frame.height) +
                " · No negros: " + std::to_string(visible);
            log_.write("INFO", videoDetail + " (cuadro " + std::to_string(frames) + ")");
        }
        if (frame.hardware) {
            if (SDL_SetRenderTarget(video_.renderer(), nullptr) < 0 ||
                SDL_RenderSetViewport(video_.renderer(), nullptr) < 0 ||
                SDL_RenderSetClipRect(video_.renderer(), nullptr) < 0 ||
                SDL_SetRenderDrawColor(video_.renderer(), 0, 0, 0, 255) < 0 ||
                SDL_RenderClear(video_.renderer()) < 0) { error = SDL_GetError(); ok = false; break; }
            if (!gpu.draw(frame.width, frame.height, frame.bottomLeftOrigin, SDL_Rect{240,0,1440,1080}, error)) {
                ok = false; break;
            }
        } else if (!frame.pixels.empty()) {
            if (!video_.gameFrame(frame.pixels.data(), frame.width, frame.height, error,
                extended && handheld.integerScaling, extended && handheld.linearFilter, handheldSystem,
                emulator_.system(), !frame.serial || frame.serial != uploadedSerial)) { ok = false; break; }
            uploadedSerial = frame.serial;
        } else video_.clear(1);
        if (overlaySystem && lastOverlayDetail != video_.handheldOverlayDetail()) {
            lastOverlayDetail = video_.handheldOverlayDetail();
            if (!lastOverlayDetail.empty()) log_.write("INFO", "Marco: " + lastOverlayDetail);
        }
        if (smoke && frames >= smokeFrames && !screenshot.empty() &&
            !video_.snapshot(screenshot + ".game.png")) {
            error = "No se pudo capturar el juego sin menú"; ok = false; break;
        }
        // The desktop smoke exercises the same explicit paused capture path.
        // Normal gameplay never reads back the renderer or writes a PNG.
        if (smoke && paused && frames >= smokeFrames && !screenshot.empty()) captureRequested = true;
        if (paused && captureRequested) {
            captureRequested = false;
            std::string savedPath, captureError;
            const bool captured = (frame.hardware || !frame.pixels.empty()) &&
                video_.saveScreenshot(platform_.dataPath(), emulator_.system(), savedPath, captureError);
            if (captured) {
                const auto separator = savedPath.find_last_of("/\\");
                pauseNotice = "Imagen guardada en screenshots/" + std::string(systemId(emulator_.system())) + "/" +
                    savedPath.substr(separator == std::string::npos ? 0 : separator + 1);
                log_.write("INFO", "Captura guardada: " + savedPath);
            } else {
                pauseNotice = "No se pudo capturar la imagen: " +
                    (captureError.empty() ? "el núcleo todavía no produjo imagen." : captureError);
                log_.write("ERROR", pauseNotice);
                if (smoke) { error = pauseNotice; ok = false; break; }
            }
        }
        if (paused) {
            video_.rect(0, 0, 1920, 1080, {3,9,17,190});
            video_.text(connected ? "Pausa" : "Mando desconectado", 160, 75, white, 48);
            video_.text(title, 160, 147, muted, 28, 1560);
            video_.text(std::string(systemName(emulator_.system())) + " · " + emulator_.coreName(), 160, 197, accent, 24);
            if (!extended) {
              video_.text(std::string("CPU: ") + emulator_.cpuName() + " · RDP: " +
                (config.graphics == N64Graphics::Gles2 ? (config.graphicsHle ? "GLideN64 GPU + HLE gráfico" : "GLideN64 GPU + RSP LLE") :
                "Angrylion CPU · " + std::to_string(config.workers) + (config.workers == 1 ? " hilo" : " hilos")),
                160, 240, muted, 20);
              video_.text(config.audioHle ?
                "Audio HLE: " + std::to_string(emulator_.audioHleTasks()) + " tareas aceleradas" :
                "Audio: RSP original (LLE) · Activa HLE en Ajustes antes de abrir el juego", 160, 274, muted, 20);
              if (config.graphics == N64Graphics::Gles2 && config.graphicsHle)
                video_.text("Gráficos HLE: " + std::to_string(emulator_.graphicsHleTasks()) +
                    " · respaldo LLE: " + std::to_string(emulator_.graphicsLleTasks()), 160, 301, muted, 20);
              video_.text(std::string("Perfil: ") + emulator_.profileName(), 1080, 197, accent, 20, 690);
            } else video_.text("Preferencias por sistema · X cambia la opción seleccionada", 160, 247, muted, 20);
            const unsigned firstItem = extended && pauseSelection >= 8 ? pauseSelection - 7 : 0;
            const unsigned lastItem = std::min<unsigned>(unsigned(pauseItems.size()), firstItem + 8);
            for (unsigned i = firstItem; i < lastItem; ++i)
                video_.text(pauseItems[i].second, 210, 326 + int(i - firstItem) * 46, i == pauseSelection ? accent : white, 28, extended ? 850 : 1560);
            video_.text("›", 162, 325 + int(pauseSelection - firstItem) * 46, accent, 28);
            if (extended) {
                video_.text("↑ ↓  Más opciones · " + std::to_string(pauseSelection + 1) + " / " + std::to_string(pauseItems.size()), 210, 718, muted, 20);
                video_.text(performanceText, 1130, 335, accent, 24, 630);
                video_.text(performanceDetail, 1130, 382, muted, 20, 630);
                video_.text("100% = velocidad normal", 1130, 434, muted, 20, 630);
                video_.text(imageRateText, 1130, 470, muted, 20, 630);
                video_.text(videoDetail, 1130, 501, muted, 20, 630);
                video_.text("Mantén R2 / Espacio: " + std::to_string(handheld.fastForward) + "x", 1130, 536, accent, 24, 630);
                video_.text("El audio se silencia durante el avance.", 1130, 578, muted, 20, 630);
                video_.text("La velocidad alcanzada depende del juego.", 1130, 614, muted, 20, 630);
                if (overlaySystem && handheld.overlay) {
                    video_.text(video_.handheldOverlayDrawn() ? "Marco: composición enviada" : "Marco: sin composición",
                        1130, 657, accent, 20, 630);
                    video_.text(lastOverlayDetail, 1130, 694, muted, 20, 630);
                }
            } else {
                video_.text(performanceText, 160, 600, accent, 24);
                video_.text(performanceDetail, 160, 641, muted, 20);
                video_.text("100% = velocidad normal · VI/s mide intervalos de vídeo, no FPS del juego", 160, 680, muted, 20);
            }
            if (emulationConfig_.profileCore && !extended) {
                const auto labels = profileText(emulator_.coreProfile(), config.graphics == N64Graphics::Gles2);
                video_.text(labels.first, 160, 730, accent, 24);
                video_.text(labels.second, 160, 767, muted, 24);
                const auto hw = emulator_.hardwareTiming();
                char boundary[180];
                std::snprintf(boundary, sizeof(boundary), "SDL ↔ núcleo: %.2f ms/VI · tiempo CPU, no ejecución GPU",
                    hw.calls ? (hw.beginMs + hw.endMs) / hw.calls : 0.0);
                video_.text(config.graphics == N64Graphics::Gles2 ? boundary :
                    "Medición activa: tiempos con instrumentación; RDP en CPU.", 160, 807, muted, 20);
            }
            if (!pauseNotice.empty()) video_.text(pauseNotice, 160, 837, accent, 20, 1560);
            video_.text("X  Elegir     Círculo  Continuar", 160, 870, muted, 24);
            video_.text(snes ? "X: B · Círculo: A · Cuadrado: Y · Triángulo: X · L1/R1: L/R (PC: Z/C/X/S · Q/W)" :
                extended ? "X: A · Cuadrado: B · Cruceta: dirección · GBA L1/R1: L/R" :
                "X: A · Cuadrado: B · L2: Z · L1/R1: L/R · Stick derecho: C", 160, 934, muted, 20);
            video_.text(extended ? "OPTIONS: Start · Panel táctil: Select · L3+R3: Pausa (PC: Esc)" :
                "Panel táctil: Start · OPTIONS: Pausa", 160, 976, muted, 20);
        } else {
            if (speed.accelerated()) {
                video_.rect(260, 20, 740, 42, {0,0,0,180});
                video_.text("Avance " + std::to_string(speed.factor()) + "x · audio silenciado · suelta R2 / Espacio", 278, 28, accent, 20, 704);
            }
            if (!extended || handheld.showStats) {
                video_.rect(1210, 20, 450, 42, {0,0,0,165});
                video_.text(performanceText, 1228, 28, white, 20, 414);
            }
            if (frames < 240 || !sound) {
                video_.rect(260, 1000, 1400, 55, {0,0,0,165});
                video_.text(sound ? (extended ? "OPTIONS / Enter  Start · Táctil / Tab  Select · L3+R3 / Esc  Pausa" :
                    "OPTIONS / Esc  Pausa     Panel táctil / Enter  Start") :
                    "Audio no disponible · Abre la pausa para volver a la biblioteca", 292, 1012, white, 20, 1320);
            }
        }
        if (smoke && frames >= smokeFrames) {
            if ((!frame.hardware && frame.pixels.empty()) || samples == 0 || !sound ||
                (extended && smokeFrames >= 600 && (!acceleratedSteps || speedTransitions < 2 || speed.accelerated() || !audioResumedAfterFastForward))) {
                error = "La prueba integrada no produjo imagen y audio"; ok = false;
            } else if (!screenshot.empty() && !video_.snapshot(screenshot)) {
                error = "No se pudo capturar la emulación"; ok = false;
            }
            break;
        }
        const double presentBegin = clockMs();
        if (!video_.present(error)) { ok = false; break; }
        const double presentMs = clockMs() - presentBegin;
        if (paused) SDL_Delay(16);
        else if (!smoke) {
            deadline += speed.periodMs(emulator_.fps(), completedSteps);
            const double remaining = deadline - clockMs();
            if (remaining > 1) SDL_Delay(static_cast<uint32_t>(std::min(remaining, 50.0)));
            else if (remaining < -250) deadline = clockMs();
        }
        if (!paused) {
            const double elapsedMs = clockMs() - frameBegin;
            const double nominalHz = std::clamp(emulator_.fps(), 20.0, 65.0);
            intervalPerformance.add(elapsedMs, nominalHz, coreMs, presentMs, completedSteps, freshImageSteps);
            sessionPerformance.add(elapsedMs, nominalHz, coreMs, presentMs, completedSteps, freshImageSteps);
            if (intervalPerformance.elapsedMs >= 1000) {
                char label[128], detail[160];
                std::snprintf(label, sizeof(label), "Emulación %.0f%% · %.1f %s",
                    intervalPerformance.speedPercent(), intervalPerformance.intervalsPerSecond(), extended ? "FPS" : "VI/s");
                std::snprintf(detail, sizeof(detail), extended ? "Núcleo %.1f · Presentación %.1f ms/cuadro" :
                    "Núcleo %.1f ms/VI · Presentación %.1f ms/VI",
                    intervalPerformance.averageCoreMs(), intervalPerformance.averagePresentMs());
                performanceText = label;
                performanceDetail = detail;
                std::snprintf(label, sizeof(label), "Entregas de imagen: %.1f/s · FPS emulados arriba", intervalPerformance.imagesPerSecond());
                imageRateText = label;
                intervalPerformance = {};
            }
            if (sessionPerformance.elapsedMs >= nextPerformanceLog) {
                logPerformance(sessionPerformance, "Emulation performance");
                nextPerformanceLog = sessionPerformance.elapsedMs + 10000;
            }
        }
    }
    audio_.pause(true); audio_.clear();
    if (extended) log_.write("INFO", "Avance rápido: " + std::to_string(acceleratedSteps) +
        " cuadros; transiciones " + std::to_string(speedTransitions) + "; audio reanudado=" +
        std::to_string(audioResumedAfterFastForward) + "; audio limpio al cerrar");
    logPerformance(sessionPerformance, smoke ? "Emulation smoke (unpaced)" : "Emulation session");
    emulator_.unload();
    gpu.reset();
    video_.releaseGameFrame();
    video_.setHandheldOverlay(SystemType::Unknown, false, overlayError);
    if (vsyncChanged && !video_.setVSync(true, syncError))
        log_.write("WARNING", "Could not restore XMB VSync: " + syncError);
    log_.write(ok ? "INFO" : "ERROR", ok ? "ROM stopped; returning to XMB" : error);
    startupLog("Emulation stopped");
    return ok;
}
}
