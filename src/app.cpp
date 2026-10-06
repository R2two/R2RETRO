#include "app.h"
#include "storage.h"
#include "ui.h"
#include "startup.h"
#include "http.h"
#include <exception>
#include <algorithm>
#include <cmath>
namespace r2n64 {
App::~App() {
    cancel_ = true;
    if (worker_.joinable()) worker_.join();
    library_.stop();
    httpShutdown();
    emulator_.unload();
    audio_.shutdown();
    if (platformReady_) platform_.shutdown();
}
void App::scan() {
    ready_ = false;
    const auto roots = platform_.romRoots();
    worker_ = std::thread([this, roots]() {
        try {
            if (scanOverride_.empty()) pending_ = scanRoms(roots, cancel_);
            else {
                pending_ = {};
                Game game; std::string error;
                if (readRom(scanOverride_, game, error)) pending_.games.push_back(std::move(game));
                else pending_.warnings.push_back(error);
            }
        }
        catch (const std::exception& e) { pending_ = {}; pending_.warnings.push_back(e.what()); }
        ready_ = true; // Publishes the result before the main thread reads it.
    });
}
int App::run(bool smoke, const std::string& screenshot, bool emulationSmoke, bool accelerated, bool gpuSmoke,
             const std::string& romSmoke, const std::string& librarySmoke, bool libraryOffline, bool n64Gpu, bool n64GraphicsHle) {
    std::string error;
    // PacBrew SDL owns USER_SERVICE and loads Piglet using the original sandbox.
    // Complete graphics initialization and presentation before requesting GoldHEN access.
    if (!video_.initialize(platform_.assetPath(), error, accelerated ? VideoBackend::Accelerated : DefaultVideoBackend))
        return startupFailure("Video initialization", error.c_str());
    video_.clear(0);
    video_.text("R2RETRO", 120, 100, {245,247,252,255}, 48);
    video_.text("Preparando biblioteca…", 120, 166, {214,224,240,255}, 28);
    if (!video_.present(error)) return startupFailure("First frame", error.c_str());
    startupLog("First frame presented");
    std::string overlayPreloadError;
    video_.preloadOverlayAssets(overlayPreloadError);
    if (!platform_.initialize()) return startupFailure("Platform initialization", platform_.status.c_str());
    platformReady_ = true;
    const auto assets = platform_.assetPath();
    video_.setAssetPath(assets);
    const auto diagnosticPath = assets.empty() ? std::string{} : assets + "/diagnostic.z64";
    startupDataReady();
    startupLog("Platform ready", platform_.status.c_str());
    const auto root = platform_.dataPath();
    const bool storage = prepareStorage(root, error);
    const bool logging = storage && log_.open(root);
    log_.write("INFO", "R2RETRO " R2N64_VERSION " starting");
    log_.write("INFO", platform_.status);
    if (!storage || !logging) log_.write("ERROR", "R2N64-DATA-001: " + error + " (log persistente no disponible)");
    log_.write("INFO", "SDL2 video initialized: 1920x1080");
    if (!video_.warning().empty()) log_.write("WARNING", video_.warning());
    if (!overlayPreloadError.empty()) log_.write("WARNING", "Precarga de marcos: " + overlayPreloadError);
    if (gpuSmoke) {
        if (!gpuDiagnostic(error, true, screenshot)) return startupFailure("GPU smoke", error.c_str());
        return 0;
    }
    if (emulationSmoke) {
        if (!storage) return startupFailure("Emulation storage", error.c_str());
        emulationConfig_.profileCore = true;
        emulationConfig_.graphics = n64Gpu ? N64Graphics::Gles2 : N64Graphics::Software;
        emulationConfig_.graphicsHle = n64GraphicsHle;
        // Exercise two complete load/run/unload sessions through the real app.
        const std::string testPath = romSmoke.empty() ? diagnosticPath : romSmoke;
        Game testGame;
        if (!readRom(testPath, testGame, error)) return startupFailure("Test cartridge", error.c_str());
        for (unsigned session = 0; session < 2; ++session)
            if (!play(testPath, testGame.title, error, true, screenshot, romSmoke.empty() ? 90 : 1200))
                return startupFailure("Emulation smoke", error.c_str());
        video_.clear(0);
        video_.text("Prueba completada", 120, 120, {245,249,255,255}, 42);
        if (!video_.present(error)) return startupFailure("Return to frontend", error.c_str());
        log_.write("INFO", "Emulation smoke passed: two sessions, video and audio");
        return 0;
    }
    std::vector<Game> allGames, games;
    LibraryMetadata selectedMetadata;
    std::string selectedPath, loadedPath;
    Game queuedDownload;
    bool downloadQueued = false, librarySmokeStarted = false, librarySmokeDone = false, librarySmokePassed = false;
    MenuAction deferredAction = MenuAction::None;
    std::string deferredPath, deferredTitle;
    uint64_t artworkRevision = 0;
    const auto caFile = assets.empty() ? std::string{} : assets + "/certs/cacert.pem";
    View view;
    view.menu.gpuRendering = n64Gpu;
    view.menu.graphicsHle = n64GraphicsHle;
    view.games = &games;
    auto filterGames = [&]() {
        games.clear();
        const SystemType systems[] = {SystemType::Unknown, SystemType::GameBoy,
            SystemType::GameBoyColor, SystemType::GameBoyAdvance, SystemType::Nintendo64,
            SystemType::NintendoEntertainmentSystem, SystemType::SuperNintendo};
        static_assert(sizeof(systems) / sizeof(systems[0]) == Menu::SystemFilterCount, "Keep XMB filters synchronized");
        for (const auto& game : allGames)
            if (!view.menu.systemFilter || game.system == systems[view.menu.systemFilter]) games.push_back(game);
        view.menu.clamp(games.size(), view.roots.size());
    };
    view.version = R2N64_VERSION;
    view.storage = storage && logging;
    view.dataPath = root;
    view.roots = platform_.romRoots();
    view.background = video_.backgroundReady();
    view.dataError = !storage ? error : !logging ? "No se pudo abrir el log persistente" : "";
#ifndef R2N64_PS4
    view.desktop = true;
#endif
    view.platform = platform_.status;
    view.scanning = true;
    startupLog("Starting ROM scanner");
    scanOverride_ = librarySmoke;
    scan();
    bool dirty = true;
    bool motionPending = false;
    const auto started = SDL_GetTicks();
    uint32_t transitionStart = started - 200;
    float categoryFrom = 0, itemFrom = 0;
    NavigationRepeat repeat;
    unsigned rendered = 0;
    while (true) {
        const auto input = platform_.poll();
        if (input.quit) break;
        const auto now = SDL_GetTicks();
        if (input.connected != view.connected) {
            view.connected = input.connected;
            log_.write("INFO", input.connected ? "Controller connected" : "Controller disconnected");
            dirty = true;
        }
        if (view.scanning && ready_) {
            worker_.join();
            allGames = std::move(pending_.games);
            filterGames();
            loadedPath.clear(); selectedMetadata = {}; view.metadata = nullptr;
            video_.clearLibraryArtwork();
            view.scanning = false;
            view.message = pending_.warnings.empty() ? "" : "No se pudo leer: " + pending_.warnings.front();
            log_.write("INFO", "ROM scanner completed: " + std::to_string(games.size()));
            for (const auto& warning : pending_.warnings) log_.write("WARNING", warning);
            dirty = true;
        }
        const auto previous = view.menu;
        const auto pressed = repeat.update(input,now);
        auto action = view.menu.handle(pressed,games.size(),view.roots.size(),view.scanning);
        if (previous.systemFilter != view.menu.systemFilter) filterGames();
        if (pressed & (Back | Up | Down | Left | Right | PreviousSystem | NextSystem | Refresh | Diagnostics)) {
            if (deferredAction != MenuAction::None) view.message.clear();
            deferredAction = MenuAction::None;
            if (pressed & Back) downloadQueued = false;
        }
        LibraryResult result;
        if (library_.poll(result)) {
            if (result.downloaded) {
                view.libraryStatus = result.cancelled ? "Descarga cancelada. Se conservan las fichas guardadas." :
                    result.found ? "Ficha guardada para usar sin conexion." : "No se pudo descargar la ficha.";
                if (!result.cancelled && !result.error.empty()) view.libraryStatus = result.error;
                log_.write(result.found ? "INFO" : "WARNING", "Libretro: " + view.libraryStatus);
            }
            if (!result.cancelled && result.game.path == selectedPath && view.menu.category == Category::Library &&
                view.menu.selected() < games.size() &&
                games[view.menu.selected()].id == result.game.id) {
                // Keep an earlier successful selection if refresh fails.
                if (result.found) {
                    selectedMetadata = std::move(result.metadata);
                    view.metadata = &selectedMetadata;
                    video_.clearLibraryArtwork();
                    std::string imageError;
                    if (result.artwork && !video_.setLibraryArtwork(result.artwork.get(),
                            selectedPath + ":" + std::to_string(++artworkRevision), imageError))
                        view.libraryStatus = imageError;
                } else if (!result.downloaded && !result.error.empty()) view.libraryStatus = result.error;
                loadedPath = selectedPath;
            }
            if (!librarySmoke.empty() && librarySmokeStarted && (result.downloaded || libraryOffline)) {
                librarySmokeDone = true;
                librarySmokePassed = result.found && !result.cancelled;
            }
            dirty = true;
        }
        if (!librarySmoke.empty() && !view.scanning && !librarySmokeStarted) {
            if (games.empty()) { log_.write("ERROR", "Library smoke ROM unavailable"); return 1; }
            librarySmokeStarted = true;
            view.menu.details = true;
            if (!libraryOffline) action = MenuAction::DownloadLibraryMetadata;
        }
        if (action == MenuAction::Quit) break;
        if (action == MenuAction::Scan) {
            library_.cancel(); downloadQueued = false;
            view.scanning = true; view.message.clear(); scan();
        } else if (action == MenuAction::DownloadLibraryMetadata) {
            view.message.clear();
            if (library_.downloading() || downloadQueued) {
                const bool activeDownload = library_.downloading();
                library_.cancel(); downloadQueued = false;
                view.libraryStatus = activeDownload ? "Cancelando descarga…" : "Descarga cancelada.";
            } else if (caFile.empty()) {
                view.libraryStatus = "No se pudo localizar el certificado CA del paquete; consulta startup.log.";
            } else if (storage) {
                queuedDownload = games[view.menu.selected()];
                downloadQueued = true;
                if (library_.busy()) library_.cancel();
                view.libraryStatus = "Buscando ficha en Libretro…";
            } else view.libraryStatus = "No se puede guardar la ficha: almacenamiento no disponible.";
            dirty = true;
        } else if (action == MenuAction::InspectGame) {
            const auto& g = games[view.menu.selected()];
            log_.write("INFO", "ROM selected: " + g.path + " CRC=" + g.id);
        }
        if (action == MenuAction::StartGame || action == MenuAction::StartDiagnostic || action == MenuAction::StartGpuDiagnostic) {
            deferredAction = action;
            const bool diagnostic = action == MenuAction::StartDiagnostic;
            if (action != MenuAction::StartGpuDiagnostic) {
                deferredPath = diagnostic ? diagnosticPath : games[view.menu.selected()].path;
                deferredTitle = diagnostic ? "Prueba Nintendo 64" : games[view.menu.selected()].title;
            }
            downloadQueued = false;
            library_.cancel();
            view.libraryStatus.clear();
            if (library_.busy()) view.message = "Preparando juego…";
            if (diagnostic && diagnosticPath.empty()) {
                deferredAction = MenuAction::None;
                view.message = "No se pudo localizar la prueba incluida; consulta startup.log.";
                dirty = true;
            }
        }
        // Wait through the event loop, never join a running transfer on Play.
        // No network/hash/image worker overlaps an emulation or GPU session.
        if (deferredAction == MenuAction::StartGpuDiagnostic && !library_.busy()) {
            deferredAction = MenuAction::None;
            view.libraryStatus.clear(); view.message.clear();
            if (!gpuDiagnostic(error)) return startupFailure("GPU diagnostic", error.c_str());
            if (quitting_) break;
            repeat = NavigationRepeat{};
            dirty = true;
            transitionStart = SDL_GetTicks() - 200;
        } else if ((deferredAction == MenuAction::StartGame || deferredAction == MenuAction::StartDiagnostic) && !library_.busy()) {
            deferredAction = MenuAction::None;
            view.libraryStatus.clear();
            view.message = "";
            emulationConfig_.workers = view.menu.parallelRendering ? 4 : 1;
            emulationConfig_.cpuMode = view.menu.automaticCpu ? CpuMode::Automatic : CpuMode::CachedInterpreter;
            emulationConfig_.audioHle = view.menu.audioHle;
            emulationConfig_.profileCore = view.menu.profileCore;
            emulationConfig_.graphics = view.menu.gpuRendering ? N64Graphics::Gles2 : N64Graphics::Software;
            emulationConfig_.graphicsHle = view.menu.graphicsHle;
            if (!play(deferredPath, deferredTitle, error)) view.message = error;
            if (quitting_) break;
            repeat = NavigationRepeat{};
            dirty = true;
            transitionStart = SDL_GetTicks() - 200;
        }
        const std::string desired = view.menu.category == Category::Library && !games.empty() ?
            games[view.menu.selected()].path : "";
        if (desired != selectedPath) {
            selectedPath = desired; loadedPath.clear();
            selectedMetadata = {}; view.metadata = nullptr;
            video_.clearLibraryArtwork();
            if (library_.busy() && !library_.downloading()) library_.cancel();
            dirty = true;
        }
        if (!view.scanning && deferredAction == MenuAction::None && !library_.busy()) {
            if (downloadQueued) {
                downloadQueued = false;
                if (!library_.start(root, caFile, queuedDownload, true)) {
                    view.libraryStatus = "No se pudo iniciar la descarga.";
                    if (!librarySmoke.empty()) librarySmokeDone = true;
                }
                dirty = true;
            } else if (!selectedPath.empty() && loadedPath != selectedPath) {
                if (!library_.start(root, caFile, games[view.menu.selected()], false)) loadedPath = selectedPath;
            }
        }
        const bool downloading = library_.downloading() || downloadQueued;
        if (view.downloading != downloading) { view.downloading = downloading; dirty = true; }
        if (pressed) dirty = true;
        if (previous.category != view.menu.category || previous.selected() != view.menu.selected()) {
            categoryFrom = view.categoryPosition;
            itemFrom = previous.category != view.menu.category ? 0 : previous.selected() < view.menu.selected() ? 42 : -42;
            transitionStart = now;
            motionPending = true;
        }
        const float progress = std::min(1.0f,float(now-transitionStart)/180.0f);
        const float ease = 1.0f-std::pow(1.0f-progress,3.0f);
        view.categoryPosition = categoryFrom+(int(view.menu.category)-categoryFrom)*ease;
        view.itemOffset = itemFrom*(1.0f-ease);
        view.contentAlpha = .55f+.45f*ease;
        if (dirty || motionPending) {
            renderUI(video_, view);
            if (!video_.present(error)) { log_.write("ERROR", error); return 1; }
            if (rendered == 0) startupLog("XMB first frame presented");
            dirty = false;
            motionPending = progress < 1.0f;
            ++rendered;
        }
        if (librarySmokeDone) {
            renderUI(video_, view);
            if (!screenshot.empty() && !video_.snapshot(screenshot)) return 1;
            log_.write(librarySmokePassed ? "INFO" : "ERROR", libraryOffline ?
                "Library offline smoke complete" : "Library download smoke complete");
            return librarySmokePassed ? 0 : 1;
        }
        if (smoke && !view.scanning && !library_.busy() && rendered > 0) {
            // GPU backbuffers need to be populated again after a present/swap.
            if (!screenshot.empty()) renderUI(video_, view);
            if (!screenshot.empty() && !video_.snapshot(screenshot)) {
                log_.write("ERROR", "No se pudo guardar la captura desktop"); return 1;
            }
            log_.write("INFO", "Desktop smoke passed");
            return view.storage && view.background ? 0 : 1;
        }
        if (smoke && SDL_GetTicks() - started > 10000) {
            log_.write("ERROR", "Smoke timeout"); return 1;
        }
        SDL_Delay(16);
    }
    log_.write("INFO", "R2RETRO closing");
    return 0;
}
}
