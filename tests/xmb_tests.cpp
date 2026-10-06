#include "menu.h"
#include "ui.h"
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <cstring>
using namespace r2n64;
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char** argv) {
    try {
        Menu menu;
        menu.handle(NextSystem,0,3,false);
        require(menu.systemFilter == 1 && !menu.details && menu.selected() == 0,
            "system filter must switch before scanning and reset stale selection");
        menu.handle(PreviousSystem,0,3,false);
        require(menu.systemFilter == 0, "system filter must return to all games");
        menu.handle(PreviousSystem,0,3,false);
        require(menu.systemFilter == 6, "previous filter from all must wrap to SNES");
        menu.handle(PreviousSystem,0,3,false);
        require(menu.systemFilter == 5, "NES filter missing before SNES");
        for (unsigned i = 0; i < Menu::SystemFilterCount; ++i) menu.handle(NextSystem,0,3,false);
        require(menu.systemFilter == 5, "all seven system filters must participate in the cycle");
        menu.handle(NextSystem,0,3,false); menu.handle(NextSystem,0,3,false);
        require(menu.systemFilter == 0, "SNES next filter must wrap to all games");
        require(menu.handle(Confirm,0,3,false)==MenuAction::Scan,"empty library must scan");
        require(menu.handle(Confirm,0,3,true)==MenuAction::None,"must not start a duplicate scan");
        menu.handle(Left,0,3,false);
        require(menu.category==Category::Library,"category left bound");
        menu.handle(Down,0,3,false);
        menu.handle(Confirm,0,3,false);
        require(menu.category==Category::Storage && menu.details,"empty library location shortcut");
        menu.handle(Back,0,3,false);
        require(!menu.details && menu.category==Category::Storage,"back must close details first");
        menu.handle(Back,0,3,false);
        require(menu.category==Category::Library,"back must return to library");
        menu.selections[0]=7;
        menu.clamp(2,3);
        require(menu.selected()==1,"rescanning must clamp selection after files disappear");
        require(menu.handle(Confirm,2,3,false)==MenuAction::InspectGame && menu.details,"game selection must show details");
        require(menu.handle(Confirm,2,3,false)==MenuAction::StartGame,"confirming details must launch the selected ROM");
        menu.clamp(0,3);
        require(!menu.details,"removing the ROM must close its details");
        menu.handle(Diagnostics,2,3,false);
        require(menu.category==Category::Settings && menu.selected()==2 && menu.details,"diagnostic shortcut");
        menu.handle(Up,2,3,false); menu.handle(Up,2,3,false);
        menu.handle(Confirm,2,3,false);
        require(menu.contrast==0 && menu.details,"appearance should open without modifying contrast");
        for (int i=0;i<3;++i) menu.handle(Confirm,2,3,false);
        require(menu.contrast==0,"contrast cycle");
        menu.handle(Down,2,3,false); menu.handle(Down,2,3,false); menu.handle(Down,2,3,false);
        require(menu.selected()==3,"performance settings must be reachable");
        menu.handle(Confirm,2,3,false);
        require(menu.parallelRendering && menu.details,"opening performance must preserve default four workers");
        menu.handle(Confirm,2,3,false);
        require(!menu.parallelRendering,"performance must allow single worker comparison");
        menu.handle(Confirm,2,3,false);
        require(menu.parallelRendering,"performance mode must return to four workers");
        menu.handle(Down,2,3,false); menu.handle(Confirm,2,3,false);
        require(menu.selected()==4 && menu.automaticCpu,"CPU automatic default must remain on opening details");
        menu.handle(Confirm,2,3,false);
        require(!menu.automaticCpu,"CPU setting must allow cached interpreter");
        menu.handle(Confirm,2,3,false);
        require(menu.automaticCpu,"CPU setting must restore automatic mode");
        menu.handle(Down,2,3,false); menu.handle(Confirm,2,3,false);
        require(menu.selected()==5 && menu.audioHle,"audio acceleration default");
        menu.handle(Confirm,2,3,false);
        require(!menu.audioHle,"audio must allow full LLE fallback");
        menu.handle(Confirm,2,3,false);
        require(menu.audioHle,"audio must restore HLE choice");
        menu.handle(Down,2,3,false);
        require(menu.selected()==6 && !menu.profileCore && !menu.details,"profiling row must be reachable and disabled by default");
        menu.handle(Confirm,2,3,false);
        require(menu.details && !menu.profileCore,"opening profiling details must not enable measurement");
        menu.handle(Confirm,2,3,false);
        require(menu.profileCore && menu.automaticCpu && menu.audioHle && menu.parallelRendering,
            "profiling toggle must preserve independent CPU/audio/renderer choices");
        menu.handle(Back,2,3,false);
        require(menu.profileCore && !menu.details,"closing profiling details must preserve this session's preference");
        menu.handle(Confirm,2,3,false); menu.handle(Confirm,2,3,false);
        require(!menu.profileCore,"profiling must allow disabling measurement again");
        menu.handle(Down,2,3,false);
        require(menu.selected()==7 && !menu.gpuRendering,"GPU is opt-in and reachable");
        menu.handle(Confirm,2,3,false);
        require(!menu.gpuRendering,"opening GPU details must not enable it");
        menu.handle(Confirm,2,3,false);
        require(menu.gpuRendering && menu.automaticCpu && menu.audioHle,"GPU toggle preserves CPU/audio");
        menu.handle(Confirm,2,3,false);
        require(menu.gpuRendering && !menu.graphicsHle,"GPU offers original LLE for comparison");
        menu.handle(Confirm,2,3,false);
        require(!menu.gpuRendering,"GPU permits CPU fallback");
        menu.handle(Confirm,2,3,false); // Open details for the shared Back sequence below.
        menu.handle(Back,2,3,false); menu.handle(Back,2,3,false);
        require(menu.handle(Back,2,3,false)==MenuAction::None && menu.exitPrompt,"first back must not quit");
        require(menu.handle(Back,2,3,false)==MenuAction::Quit,"second back should quit");
        Menu about;
        about.handle(Right,0,3,false); about.handle(Right,0,3,false); about.handle(Right,0,3,false);
        require(about.category==Category::About && about.selected()==0,"About category must open its first row");
        require(about.handle(Confirm,0,3,false)==MenuAction::None && about.details,"About details must not start a test");
        about.handle(Down,0,3,false);
        require(about.handle(Confirm,0,3,false)==MenuAction::StartDiagnostic,"N64 diagnostic action must remain reachable");
        about.handle(Down,0,3,false);
        require(about.selected()==2 && about.handle(Confirm,0,3,false)==MenuAction::StartGpuDiagnostic,
            "GPU test requires explicit confirmation and must not quit");
        about.handle(Down,0,3,false); about.handle(Down,0,3,false);
        require(about.selected()==3 && about.handle(Confirm,0,3,false)==MenuAction::Quit,"quit must occupy the final About row");
        about.selections[2]=99; about.selections[3]=99; about.clamp(0,3);
        require(about.selections[2]==7 && about.selections[3]==3,"new menu counts must clamp stale selections");
        NavigationRepeat repeat;
        Input input; input.held=Down; input.pressed=Down;
        require(repeat.update(input,0)==Down,"initial navigation press");
        input.pressed=0;
        require(repeat.update(input,300)==0 && repeat.update(input,380)==Down,"navigation repeat delay");
        input.held=Confirm;
        require(repeat.update(input,400)==0 && repeat.update(input,1000)==0,"confirm must never auto-repeat");
        input.held=0;
        require(repeat.update(input,1200)==0,"disconnect/release must stop repeat");

        Menu downloads;
        require(downloads.handle(DownloadMetadata,0,3,false)==MenuAction::None,
            "empty library must not request metadata");
        require(downloads.handle(DownloadMetadata,1,3,true)==MenuAction::None,
            "metadata lookup must not start while library scanning changes selection");
        require(downloads.handle(DownloadMetadata,1,3,false)==MenuAction::DownloadLibraryMetadata && !downloads.details,
            "library shortcut must request metadata without entering or launching the game");
        downloads.handle(Confirm,1,3,false);
        require(downloads.handle(DownloadMetadata,1,3,false)==MenuAction::DownloadLibraryMetadata && downloads.details,
            "metadata shortcut must remain available in game details");
        for (int category=1; category<4; ++category) {
            downloads.category=static_cast<Category>(category);
            require(downloads.handle(DownloadMetadata,1,3,false)==MenuAction::None,
                "metadata download must only be offered in the library");
        }
        input.held=DownloadMetadata; input.pressed=DownloadMetadata;
        require(repeat.update(input,1300)==DownloadMetadata,"metadata press must be delivered once");
        input.pressed=0;
        require(repeat.update(input,1700)==0 && repeat.update(input,3000)==0,
            "holding metadata shortcut must not repeatedly start/cancel requests");

        Video video;
        std::string error;
        const bool accelerated = argc == 2 && std::strcmp(argv[1], "--accelerated") == 0;
        require(video.initialize(R2N64_ASSETS,error,accelerated ? VideoBackend::Accelerated : VideoBackend::Software),error.c_str());
        require(video.backgroundReady(),"supplied JPG must decode");
        std::filesystem::create_directories("previews");
        std::vector<Game> games;
        View view; view.games=&games; view.version="0.4.1"; view.storage=true; view.background=true;
        view.desktop=true; view.dataPath="/data/R2N64";
        view.roots={"/data/R2N64/roms","/mnt/usb0/R2N64/roms","/mnt/usb1/R2N64/roms"};
        view.platform="Desktop: prueba de frontend SDL2";
        auto snapshot=[&](const char* path) {
            view.categoryPosition=int(view.menu.category);
            renderUI(video,view);
            require(video.snapshot(path),"screenshot output failed");
            require(video.present(error),error.c_str());
        };
        snapshot("previews/xmb-empty.png");
        view.menu.handle(Right,0,3,false); view.menu.handle(Confirm,0,3,false);
        snapshot("previews/xmb-storage.png");
        view.menu.handle(Diagnostics,0,3,false);
        snapshot("previews/xmb-diagnostics.png");
        view.menu.handle(Down,0,3,false); view.menu.handle(Confirm,0,3,false);
        snapshot("previews/xmb-performance.png");
        view.menu.handle(Down,0,3,false); view.menu.handle(Confirm,0,3,false);
        snapshot("previews/xmb-cpu.png");
        view.menu.handle(Down,0,3,false); view.menu.handle(Confirm,0,3,false);
        snapshot("previews/xmb-audio-mode.png");
        view.menu.handle(Down,0,3,false); view.menu.handle(Confirm,0,3,false);
        snapshot("previews/xmb-measurement-off.png");
        view.menu.handle(Confirm,0,3,false);
        snapshot("previews/xmb-measurement-on.png");
        view.menu.handle(Down,0,3,false); view.menu.handle(Confirm,0,3,false);
        snapshot("previews/xmb-gpu-rendering-off.png");
        view.menu.handle(Confirm,0,3,false);
        snapshot("previews/xmb-gpu-rendering-on.png");
        view.menu.handle(Right,0,3,false); view.menu.handle(Confirm,0,3,false);
        snapshot("previews/xmb-about.png");
        view.menu.handle(Down,0,3,false); view.menu.handle(Down,0,3,false);
        snapshot("previews/xmb-gpu-test.png");
        view.menu.handle(Down,0,3,false);
        snapshot("previews/xmb-about-quit.png");
        Game fixture; fixture.title="Prueba sintética de cabecera N64"; fixture.region="USA / NTSC";
        fixture.system=SystemType::Nintendo64;
        fixture.id="12345678-9ABCDEF0"; fixture.serial="NTSE"; fixture.size=4096;
        fixture.path="/data/R2N64/roms/synthetic.z64";
        games.push_back(fixture);
        view.menu=Menu{};
        view.menu.handle(Confirm,1,3,false);
        snapshot("previews/xmb-rom-details.png");
        games[0].system=SystemType::GameBoy;
        games[0].title="Prueba original Game Boy";
        games[0].path="/data/R2N64/roms/gb/diagnostic.gb";
        view.menu.systemFilter=1;
        snapshot("previews/xmb-gameboy-details.png");
        games[0].system=SystemType::NintendoEntertainmentSystem;
        games[0].title="Prueba original NES";
        games[0].path="/data/R2N64/roms/nes/diagnostic.nes";
        view.menu.systemFilter=5;
        snapshot("previews/xmb-nes-details.png");
        games[0].system=SystemType::SuperNintendo;
        games[0].title="Prueba original Super Nintendo";
        games[0].path="/data/R2N64/roms/snes/diagnostic.sfc";
        view.menu.systemFilter=6;
        snapshot("previews/xmb-snes-details.png");

        LibraryMetadata metadata;
        metadata.title="Aventura sintética — Edición especial";
        metadata.region="USA / Europe"; metadata.year="1994";
        metadata.genre="Aventura"; metadata.developer="Estudio de pruebas";
        metadata.publisher="Distribuidor de pruebas"; metadata.players="1–2";
        auto* cover=SDL_CreateRGBSurfaceWithFormat(0,180,240,32,SDL_PIXELFORMAT_ARGB8888);
        require(cover!=nullptr,"synthetic cover allocation failed");
        SDL_FillRect(cover,nullptr,0xff177768);
        SDL_Rect band{15,25,150,50}; SDL_FillRect(cover,&band,0xffb5f4dc);
        band={35,115,110,110}; SDL_FillRect(cover,&band,0xff10423b);
        require(video.setLibraryArtwork(cover,"synthetic-cover-v1",error),error.c_str());
        SDL_FreeSurface(cover);
        view.metadata=&metadata;
        view.libraryStatus="Ficha guardada. Disponible sin conexión.";
        snapshot("previews/xmb-library-metadata.png");
        view.downloading=true;
        view.libraryStatus="Descargando carátula de Libretro…";
        snapshot("previews/xmb-library-downloading.png");
        video.clearLibraryArtwork();
        metadata.title.clear(); metadata.region.clear(); metadata.year.clear(); metadata.genre.clear();
        metadata.developer=std::string(180,'A')+" — compañía de prueba 日本語";
        metadata.publisher.clear(); metadata.players.clear();
        view.downloading=false;
        view.libraryStatus="No se pudo descargar la imagen. La ficha sigue disponible.";
        view.desktop=false;
        snapshot("previews/xmb-library-partial.png");
        view.metadata=nullptr; view.libraryStatus.clear();
        renderUI(video,view); require(video.present(error),error.c_str());
        // Render each contrast preset and a long UTF-8 title without corrupting glyphs.
        games[0].title=std::string(180,'A')+" — edición de prueba 日本語";
        for (int i=0;i<3;++i) { view.menu.contrast=i; renderUI(video,view); require(video.present(error),error.c_str()); }
        std::puts("PASS: XMB navigation, empty state, scan, back/exit, repeat, contrast, JPEG and rendered states");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1;
    }
}
