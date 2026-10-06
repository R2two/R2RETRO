#include "video.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Observe the real renderer: repeated drawing must neither decode files nor
// upload textures. Failure injection also checks that stale art is discarded.
static unsigned uploads=0, decodes=0;
static bool failUpload=false;
extern "C" SDL_Texture* __real_SDL_CreateTextureFromSurface(SDL_Renderer*, SDL_Surface*);
extern "C" SDL_Texture* __wrap_SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* source) {
    ++uploads;
    if (failUpload) { SDL_SetError("Synthetic artwork upload failure"); return nullptr; }
    return __real_SDL_CreateTextureFromSurface(renderer,source);
}
extern "C" SDL_Surface* __real_IMG_Load(const char*);
extern "C" SDL_Surface* __wrap_IMG_Load(const char* path) {
    ++decodes;
    return __real_IMG_Load(path);
}

using namespace r2n64;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_FreeSurface)>;
static void require(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message+": "+SDL_GetError());
}
static Surface source(int w,int h,uint32_t color) {
    Surface surface(SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888),SDL_FreeSurface);
    require(bool(surface),"Cannot create original diagnostic artwork");
    require(SDL_FillRect(surface.get(),nullptr,color)==0,"Cannot paint diagnostic artwork");
    return surface;
}
static std::vector<uint32_t> capture(Video& video,const char* name) {
    const auto path=std::filesystem::path("artwork-previews")/(std::string(name)+".png");
    require(video.snapshot(path.string()),"Cannot capture artwork test");
    Surface png(IMG_Load(path.c_str()),SDL_FreeSurface);
    require(bool(png),"Cannot load artwork capture");
    Surface surface(SDL_ConvertSurfaceFormat(png.get(),SDL_PIXELFORMAT_ARGB8888,0),SDL_FreeSurface);
    require(bool(surface) && surface->w==1920 && surface->h==1080,"Unexpected capture dimensions");
    require(SDL_LockSurface(surface.get())==0,"Cannot read artwork capture");
    std::vector<uint32_t> pixels(1920*1080);
    for (int y=0;y<1080;++y) {
        const auto* row=reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(surface->pixels)+y*surface->pitch);
        for (int x=0;x<1920;++x) pixels[y*1920+x]=row[x]&0xffffffu;
    }
    SDL_UnlockSurface(surface.get());
    return pixels;
}
static void bounds(const std::vector<uint32_t>& pixels,SDL_Rect area,uint32_t color) {
    for (int y=0;y<1080;++y) for (int x=0;x<1920;++x) {
        const bool inside=x>=area.x && x<area.x+area.w && y>=area.y && y<area.y+area.h;
        if (pixels[y*1920+x]!=(inside ? color : 0))
            throw std::runtime_error("Artwork was stretched, cropped, misplaced or stale");
    }
}
static void black(Video& video) { video.rect(0,0,1920,1080,{0,0,0,255}); }

int main(int argc,char** argv) {
    try {
        Video video; std::string error;
        require(!video.libraryArtwork(0,0,200,200),"Uninitialized renderer must not draw");
        auto portrait=source(120,240,0xffeeeeeeu);
        require(!video.setLibraryArtwork(portrait.get(),"before-initialize",error) && !error.empty(),
            "Uninitialized renderer must reject artwork with an explanation");
        const bool accelerated=argc==2 && std::strcmp(argv[1],"--accelerated")==0;
        require(video.initialize(R2N64_ASSETS,error,accelerated ? VideoBackend::Accelerated : VideoBackend::Software),error);
        std::filesystem::create_directories("artwork-previews");
        const auto uploadsBefore=uploads, decodesBefore=decodes;
        require(video.setLibraryArtwork(portrait.get(),"portrait",error) && error.empty(),"Cannot upload portrait");
        portrait.reset(); // The caller may free the source immediately after upload.
        require(uploads==uploadsBefore+1 && decodes==decodesBefore,"Uploading a surface must not read a file");
        require(video.setLibraryArtwork(nullptr,"portrait",error),"Same key must reuse the retained texture");
        for (int i=0;i<24;++i) {
            black(video); require(video.libraryArtwork(100,100,200,200),"Cannot draw cached artwork");
        }
        require(uploads==uploadsBefore+1 && decodes==decodesBefore,"Drawing/reusing artwork must not allocate or decode");
        bounds(capture(video,"portrait"),{150,100,100,200},0xeeeeeeu);

        auto landscape=source(240,120,0xff339977u);
        require(video.setLibraryArtwork(landscape.get(),"portrait",error),"Matching key must remain usable");
        black(video); require(video.libraryArtwork(100,100,200,200),"Cached portrait disappeared");
        bounds(capture(video,"same-key"),{150,100,100,200},0xeeeeeeu);
        require(video.setLibraryArtwork(landscape.get(),"landscape",error),"New key must replace artwork");
        require(uploads==uploadsBefore+2,"Exactly one upload is allowed per successful changed key");
        black(video); require(video.libraryArtwork(100,100,200,200),"Cannot draw landscape");
        bounds(capture(video,"landscape"),{100,150,200,100},0x339977u);

        auto alpha=source(100,100,0x00000000u);
        SDL_Rect center{25,25,50,50}; SDL_FillRect(alpha.get(),&center,0xffbb7733u);
        require(video.setLibraryArtwork(alpha.get(),"alpha",error),"Cannot upload transparent image");
        black(video); require(video.libraryArtwork(400,100,100,100),"Cannot draw transparent artwork");
        bounds(capture(video,"alpha"),{425,125,50,50},0xbb7733u);

        const auto beforeInvalid=uploads;
        require(!video.libraryArtwork(1,1,0,20) && !video.libraryArtwork(1,1,20,-1) &&
                !video.libraryArtwork(1,1,8193,20),"Invalid destination dimensions must be rejected");
        auto oversized=source(2049,1,0xffffffffu);
        require(!video.setLibraryArtwork(oversized.get(),"oversized",error) && !error.empty(),
            "Oversized images must be rejected before GPU allocation");
        require(!video.libraryArtwork(100,100,200,200) && uploads==beforeInvalid,
            "Invalid replacement must clear the previous game's artwork");
        require(!video.setLibraryArtwork(landscape.get(),"",error),"Empty identity must be rejected");
        require(!video.setLibraryArtwork(nullptr,"missing",error),"Missing changed-key source must be rejected");
        require(video.setLibraryArtwork(landscape.get(),"before-failure",error),"Cannot seed failed replacement test");
        failUpload=true;
        require(!video.setLibraryArtwork(landscape.get(),"failed",error) && !error.empty(),"Upload failure must be surfaced");
        failUpload=false;
        require(!video.libraryArtwork(100,100,200,200),"Upload failure must not retain the wrong game's image");

        // Library drawing must not interfere with gameplay, or vice versa.
        require(video.setLibraryArtwork(landscape.get(),"game-independent",error),"Cannot restore artwork");
        std::vector<uint32_t> frame(160*144,0xff8855ccu);
        require(video.gameFrame(frame.data(),160,144,error,true,false,true),error);
        bounds(capture(video,"game-frame"),{400,36,1120,1008},0x8855ccu);
        video.releaseGameFrame();
        black(video); require(video.libraryArtwork(100,100,200,200),"Releasing a game frame must preserve library art");
        bounds(capture(video,"after-game"),{100,150,200,100},0x339977u);
        video.clearLibraryArtwork(); video.clearLibraryArtwork();
        require(!video.libraryArtwork(100,100,200,200),"Clearing artwork must be safe and remove it");
        require(video.gameFrame(frame.data(),160,144,error,true,false,true),error);
        bounds(capture(video,"after-clear"),{400,36,1120,1008},0x8855ccu);
        require(video.present(error),error);
        std::puts("PASS: artwork ownership, cache, aspect, alpha, failures and independent game frames");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1;
    }
}
