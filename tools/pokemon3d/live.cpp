// Opt-in desktop experiment. Game logic always runs in the existing SameBoy
// integration. This file must never patch WRAM, ROM, collision or event flags.
#include "red_state.h"
#include "renderer.h"
#include "emulator.h"
#include "core/libretro_core.h"
#include "log.h"
#include "audio.h"
#include "view_settings.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <cmath>
#include <map>
#include <tuple>
namespace fs=std::filesystem;
using namespace pokemon3d;
namespace {
void require(bool ok,const std::string& error) { if (!ok) throw std::runtime_error(error); }
unsigned number(const std::string& value,unsigned max) {
    require(!value.empty() && value.find_first_not_of("0123456789")==std::string::npos,"Invalid integer");
    auto result=std::stoul(value); require(result<=max,"Integer outside budget"); return unsigned(result);
}
std::vector<std::uint8_t> readRom(const fs::path& path) {
    require(fs::file_size(path)==1048576,"Unsupported ROM length");
    std::ifstream file(path,std::ios::binary);
    std::vector<std::uint8_t> data(1048576);
    require(bool(file.read(reinterpret_cast<char*>(data.data()),data.size())),"ROM read failed");
    require(supportedRed(data),"Unsupported ROM: this lab accepts only verified Pokemon Red USA/Europe");
    return data;
}
r2n64::GamepadInput button(const std::string& name) {
    r2n64::GamepadInput pad{}; pad.connected=true;
    if(name=="A") pad.buttons=1u<<RETRO_DEVICE_ID_JOYPAD_B;
    else if(name=="B") pad.buttons=1u<<RETRO_DEVICE_ID_JOYPAD_Y;
    else if(name=="START") pad.start=true;
    else if(name=="SELECT") pad.select=true;
    else if(name=="UP") pad.buttons=1u<<RETRO_DEVICE_ID_JOYPAD_UP;
    else if(name=="DOWN") pad.buttons=1u<<RETRO_DEVICE_ID_JOYPAD_DOWN;
    else if(name=="LEFT") pad.buttons=1u<<RETRO_DEVICE_ID_JOYPAD_LEFT;
    else if(name=="RIGHT") pad.buttons=1u<<RETRO_DEVICE_ID_JOYPAD_RIGHT;
    else require(name=="WAIT","Unknown input script button: "+name);
    return pad;
}
struct ScriptInput { r2n64::GamepadInput pad; bool toggle3d=false; };
std::vector<ScriptInput> readScript(const fs::path& path) {
    std::vector<ScriptInput> inputs;
    if(path.empty()) return inputs;
    std::ifstream file(path); require(bool(file),"Cannot open input script");
    std::string line;
    while(std::getline(file,line)) {
        line=line.substr(0,line.find('#'));
        std::istringstream row(line); std::string count,key,extra;
        if(!(row>>count)) continue;
        require(bool(row>>key) && !(row>>extra),"Input script requires: frame_count BUTTON");
        unsigned frames=number(count,120000);
        require(inputs.size()+frames<=120000,"Input script exceeds budget");
        if(key=="TOGGLE3D") {
            require(frames==1,"TOGGLE3D is a single press: use 1 TOGGLE3D");
            inputs.push_back({button("WAIT"),true});
        } else inputs.insert(inputs.end(),frames,ScriptInput{button(key),false});
    }
    return inputs;
}
bool snapshot(r2n64::Emulator& emulator,WorkRam& copy) {
    copy.fill(0);
    if(!emulator.loaded() || emulator.system()!=r2n64::SystemType::GameBoy) return false;
    const auto* api=r2n64::coreApiFor(r2n64::SystemType::GameBoy);
    // Reacquire after EVERY run/reset/state-load. Never borrow across calls.
    const auto size=api->get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
    const auto* ptr=api->get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
    if(!ptr || size!=copy.size()) return false;
    std::memcpy(copy.data(),ptr,copy.size());
    return true;
}
void framePng(const r2n64::CoreFrame& frame,const fs::path& path) {
    require(frame.pixels.size()==std::size_t(frame.width)*frame.height && frame.width && frame.height,"No frame to capture");
    auto* surface=SDL_CreateRGBSurfaceWithFormatFrom(const_cast<std::uint32_t*>(frame.pixels.data()),
        int(frame.width),int(frame.height),32,int(frame.width*4),SDL_PIXELFORMAT_RGB888);
    require(surface!=nullptr,SDL_GetError());
    const int status=IMG_SavePNG(surface,path.string().c_str()); SDL_FreeSurface(surface);
    require(status==0,IMG_GetError());
}
}
int main(int argc,char** argv) {
    try {
        fs::path rom,scenePath,worldPath,data,script; unsigned frames=600;
        bool resume=false,interactive=false,verify=false,save=true,renderAll=false;
        std::string requestedView;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            if(arg=="--resume") {resume=true;continue;}
            if(arg=="--interactive") {interactive=true;continue;}
            if(arg=="--verify-lifecycle") {verify=true;continue;}
            if(arg=="--no-save") {save=false;continue;}
            if(arg=="--render-all") {renderAll=true;continue;}
            require(i+1<argc,"Missing argument value"); std::string value=argv[++i];
            if(arg=="--rom") rom=value; else if(arg=="--scene") scenePath=value;
            else if(arg=="--world") worldPath=value;
            else if(arg=="--view") {require(value=="2d"||value=="3d","--view accepts 2d or 3d");requestedView=value;}
            else if(arg=="--data") data=value; else if(arg=="--script") script=value;
            else if(arg=="--frames") frames=number(value,120000); else throw std::runtime_error("Unknown argument: "+arg);
        }
        require(!rom.empty()&&(!scenePath.empty()||!worldPath.empty())&&!data.empty()&&frames,
                "--rom, --scene or --world, --data and nonzero --frames required");
        require(scenePath.empty()||worldPath.empty(),"Choose either --scene or --world");
        const auto build=fs::weakly_canonical(fs::path(R2N64_LAB_ROOT)/"build");
        data=fs::weakly_canonical(data);
        const auto relative=data.lexically_relative(build);
        require(!relative.empty() && *relative.begin()!=".." && relative!=".","Lab data must live inside repository build/");
        SpriteAtlas sprites;std::string error;
        bool presentationEnabled=true;
        if(!loadViewPreference(data,presentationEnabled,error)) std::cerr<<error<<'\n';
        bool preferenceDirty=!requestedView.empty();
        if(preferenceDirty) presentationEnabled=requestedView=="3d";
        const bool initialPresentation=presentationEnabled;
        require(loadSpriteAtlas(readRom(rom),sprites,error),error);
        require(save||!verify,"Lifecycle verification needs a disposable save-state checkpoint");
        const auto inputs=readScript(script);
        // Decode/load every supported map before emulation; map transitions do
        // not access the filesystem or compile shaders.
        std::map<unsigned,Scene> scenes;
        if(!worldPath.empty()) {
            for(const unsigned id : {0u,37u,38u})
                require(loadScene((worldPath/("map-"+std::to_string(id)+".r2scene")).string(),scenes[id],error),error);
            // Older three-map exports remain usable. Newer exports add Oak's lab.
            if(fs::exists(worldPath/"map-40.r2scene"))
                require(loadScene((worldPath/"map-40.r2scene").string(),scenes[40],error),error);
        } else require(loadScene(scenePath.string(),scenes[0],error),error);
        if(!interactive && !SDL_getenv("SDL_VIDEODRIVER")) SDL_setenv("SDL_VIDEODRIVER","offscreen",0);
        // The emulator's fractional clock owns pacing; do not stack a second
        // display-vsync wait on top of the Game Boy's 59.73 Hz deadline.
        Renderer renderer;require(renderer.initialize(interactive,error,false),error);
        renderer.setPresentationEnabled(presentationEnabled);
        require(renderer.uploadSprites(sprites,error),error);
        fs::create_directories(data/"logs");
        const auto runId=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto evidence=data/("run-"+std::to_string(runId));fs::create_directories(evidence);
        std::cout<<"Evidence: "<<evidence.string()<<'\n';
        r2n64::Log log;require(log.open(data.string()),"Log failed");
        r2n64::Emulator emulator;require(emulator.load(rom.string(),data.string(),log,error),error);
        require(emulator.run(button("WAIT"),error),error);
        if(resume) require(emulator.loadState(error),error);
        r2n64::Audio audio;
        if(interactive) {require(audio.initialize(error),error);require(audio.start(emulator.sampleRate(),error),error);}
        WorkRam ram{}; RedState state; LiveGate gate; Camera camera;camera.yaw=0;camera.zoom=1.25f;camera.follow=true;
        std::vector<std::uint32_t> overlayPixels;
        const double timerFrequency=double(SDL_GetPerformanceFrequency());
        double deadline=double(SDL_GetPerformanceCounter());
        // Keep telemetry in memory while emulating; flush only after stopping.
        std::ostringstream trace;
        trace<<"frame,map,x,y,width,height,font,battle,pallet,actors,reason,supported,overlay,world_x,world_z,sprite_frame,view_3d\n";
        unsigned palletFrames=0,mapChanges=0,printed=0,lastMap=255,executed=0,captures=0,restores=0;
        unsigned worldFrames=0,overlayFrames=0,renderedFrames=0,sceneUploads=0,activeMap=255,cameraMap=255;
        unsigned viewSwitches=0,view3dFrames=0,view2dFrames=0;
        std::map<unsigned,unsigned> mapFrames;
        std::set<std::pair<unsigned,unsigned>> positions;
        std::set<std::tuple<unsigned,unsigned,unsigned>> worldPositions;
        std::set<unsigned> playerAnimations;
        std::set<std::tuple<unsigned,int,int>> playerSubpixels;
        unsigned overlayAge=0,stableWindowAge=0;
        UiRect previousWindow;
        unsigned lastCaptureMap=255;
        std::string lastStatus;
        for(unsigned i=0;i<frames;++i) {
            const auto command=i<inputs.size()?inputs[i]:ScriptInput{button("WAIT"),false};
            auto input=command.pad;
            bool toggleView=!interactive&&command.toggle3d;
            if(interactive) {
                SDL_Event event;bool quit=false;
                while(SDL_PollEvent(&event)) {
                    if(event.type==SDL_QUIT) quit=true;
                    if(event.type==SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym==SDLK_F1) toggleView=true;
                }
                const auto* keys=SDL_GetKeyboardState(nullptr); if(quit||keys[SDL_SCANCODE_ESCAPE]) break;
                input=button("WAIT");
                for(auto pair:{std::pair<SDL_Scancode,const char*>{SDL_SCANCODE_Z,"A"},{SDL_SCANCODE_X,"B"},
                               {SDL_SCANCODE_UP,"UP"},{SDL_SCANCODE_DOWN,"DOWN"},{SDL_SCANCODE_LEFT,"LEFT"},{SDL_SCANCODE_RIGHT,"RIGHT"}})
                    if(keys[pair.first]) input.buttons|=button(pair.second).buttons;
                input.start=keys[SDL_SCANCODE_RETURN];input.select=keys[SDL_SCANCODE_TAB];
                if(keys[SDL_SCANCODE_Q]) camera.yaw-=1;
                if(keys[SDL_SCANCODE_E]) camera.yaw+=1;
                if(keys[SDL_SCANCODE_W]) camera.zoom=std::min(4.f,camera.zoom+0.01f);
                if(keys[SDL_SCANCODE_S]) camera.zoom=std::max(0.5f,camera.zoom-0.01f);
            }
            if(toggleView) {
                presentationEnabled=!presentationEnabled;preferenceDirty=true;++viewSwitches;
                renderer.setPresentationEnabled(presentationEnabled);
            }
            require(emulator.run(input,error),error);++executed;
            require(snapshot(emulator,ram),"Cannot read a complete SameBoy WRAM copy");
            state=gate.update(decodeRed(ram,true));
            if(state.supported && scenes.find(state.map)==scenes.end()) {
                state.supported=false;state.pallet=false;state.overlay=false;state.actors.clear();state.reason="map scene unavailable";
            }
            overlayAge=state.overlay?overlayAge+1:0;
            const auto& window=state.ui;
            const bool sameWindow=window.x==previousWindow.x && window.y==previousWindow.y &&
                window.width==previousWindow.width && window.height==previousWindow.height;
            stableWindowAge=state.overlay && window.width && window.height ? (sameWindow?stableWindowAge+1:1):0;
            previousWindow=window;
            // The CPU tilemap precedes the multi-frame VRAM transfer. Avoid
            // briefly magnifying the old overworld pixels as a new menu.
            const bool showOverlay=state.overlay && overlayAge>=8 && (!window.width || stableWindowAge>=6);
            const bool drawWorld=presentationEnabled&&state.supported;
            if(drawWorld) ++view3dFrames;else ++view2dFrames;
            const std::string status=std::to_string(state.map)+":"+std::to_string(state.x)+":"+std::to_string(state.y)+":"+state.reason+":"+(presentationEnabled?"3d":"2d");
            if(state.map!=lastMap) {++mapChanges;lastMap=state.map;}
            bool newPosition=false;
            if(state.pallet) {++palletFrames;positions.emplace(state.x,state.y);}
            if(state.supported) {
                ++worldFrames;++mapFrames[state.map];
                if(state.overlay) ++overlayFrames;
                newPosition=worldPositions.emplace(state.map,state.x,state.y).second;
                if(!state.actors.empty()) {
                    const auto& player=state.actors.front();
                    playerAnimations.insert(player.frame);
                    playerSubpixels.emplace(state.map,int(std::lround(player.worldX*8)),int(std::lround(player.worldZ*8)));
                    const float blend=cameraMap!=state.map?1.f:0.25f;
                    camera.targetX+=(player.worldX-camera.targetX)*blend;
                    camera.targetZ+=(player.worldZ-camera.targetZ)*blend;
                    cameraMap=state.map;
                }
            } else cameraMap=255;
            if(status!=lastStatus || i%60==0) {
                trace<<i<<','<<state.map<<','<<state.x<<','<<state.y<<','<<state.width<<','<<state.height<<','
                     <<unsigned(ram[0xFC4])<<','<<unsigned(ram[0x1057])<<','<<state.pallet<<','<<state.actors.size()<<','<<state.reason
                     <<','<<state.supported<<','<<state.overlay<<','<<(state.actors.empty()?0:state.actors.front().worldX)
                     <<','<<(state.actors.empty()?0:state.actors.front().worldZ)<<','<<(state.actors.empty()?0:state.actors.front().frame)
                     <<','<<drawWorld<<'\n';
                if(!interactive && status!=lastStatus && printed++<100) std::cout<<i<<": "<<status<<'\n';
                lastStatus=status;
            }
            const bool capture=(state.supported && (lastCaptureMap!=state.map || (showOverlay && overlayAge==24))) ||
                (newPosition && captures<24) || toggleView || i+1==frames;
            if(interactive || renderAll || capture) {
                const auto& frame=emulator.frame();
                if(drawWorld) {
                    if(activeMap!=state.map) {
                        require(renderer.upload(scenes.at(state.map),error),error);activeMap=state.map;++sceneUploads;
                    }
                    require(renderer.draw(camera,error),error);
                    require(renderer.drawActors(state.actors,error),error);
                    if(showOverlay) {
                        const auto& box=state.ui;
                        if(box.width && box.height && box.x+box.width<=frame.width && box.y+box.height<=frame.height) {
                            overlayPixels.resize(std::size_t(box.width)*box.height);
                            for(unsigned y=0;y<box.height;++y)
                                std::copy_n(frame.pixels.data()+(box.y+y)*frame.width+box.x,box.width,
                                            overlayPixels.data()+y*box.width);
                            require(renderer.drawGameOverlay(overlayPixels.data(),box.width,box.height,error),error);
                        } else require(renderer.drawGameOverlay(frame.pixels.data(),frame.width,frame.height,error),error);
                    }
                } else {
                    require(renderer.drawGameFrame(frame.pixels.data(),frame.width,frame.height,false,error),error);
                }
                ++renderedFrames;
                if(!interactive && capture && captures<64) {
                    const std::string prefix=drawWorld?(showOverlay?"overlay-":"world-")+std::to_string(state.map)+"-":
                        presentationEnabled?"fallback-":"original-";
                    require(renderer.savePng((evidence/(prefix+std::to_string(i)+".png")).string(),error),error);
                    ++captures;
                    if(state.supported) lastCaptureMap=state.map;
                }
                if(interactive) renderer.present();
            }
            if(i+1==frames) framePng(emulator.frame(),evidence/"game-last.png");
            if(interactive) {
                audio.push(emulator.audio().data(),emulator.audio().size()/2);
                deadline+=timerFrequency/emulator.fps();
                double now=double(SDL_GetPerformanceCounter());
                if(now-deadline>timerFrequency/10) deadline=now;
                while(now<deadline) {
                    SDL_Delay(1);
                    now=double(SDL_GetPerformanceCounter());
                }
            }
        }
        if(save) require(emulator.saveState(error),error);
        if(verify) {
            const auto expected=ram;
            for(unsigned i=0;i<30;++i) require(emulator.run(button("WAIT"),error),error);
            require(emulator.loadState(error),error);
            require(snapshot(emulator,ram)&&ram==expected,"WRAM differs after state restoration");++restores;
            require(emulator.reset(error)&&emulator.run(button("WAIT"),error),error);
            require(snapshot(emulator,ram)&&!decodeRed(ram,true).supported,"Reset retained stale world state");
            require(emulator.loadState(error),error);
            require(snapshot(emulator,ram)&&ram==expected,"WRAM differs after reset and restore");++restores;
            emulator.unload();require(!snapshot(emulator,ram),"Unload exposed stale WRAM");
            require(emulator.load(rom.string(),data.string(),log,error)&&emulator.run(button("WAIT"),error),error);
            require(emulator.loadState(error),error);
            require(snapshot(emulator,ram)&&ram==expected,"WRAM differs after reopen and restore");++restores;
        }
        std::ofstream traceFile(evidence/"trace.csv");traceFile<<trace.str();
        std::ofstream dump(evidence/"last-wram.bin",std::ios::binary);dump.write(reinterpret_cast<const char*>(ram.data()),ram.size());
        std::ofstream report(evidence/"live-report.json");
        report<<"{\"frames\":"<<executed<<",\"pallet_frames\":"<<palletFrames<<",\"pallet_positions\":"<<positions.size()
              <<",\"world_frames\":"<<worldFrames<<",\"overlay_frames\":"<<overlayFrames<<",\"fallback_frames\":"<<(executed-worldFrames)
              <<",\"rendered_frames\":"<<renderedFrames<<",\"scene_uploads\":"<<sceneUploads
              <<",\"world_positions\":"<<worldPositions.size()<<",\"player_animation_frames\":"<<playerAnimations.size()
              <<",\"player_pixel_positions\":"<<playerSubpixels.size()<<",\"map0_frames\":"<<mapFrames[0]
              <<",\"map37_frames\":"<<mapFrames[37]<<",\"map38_frames\":"<<mapFrames[38]
              <<",\"map40_frames\":"<<mapFrames[40]<<",\"view_switches\":"<<viewSwitches
              <<",\"view3d_frames\":"<<view3dFrames<<",\"view2d_frames\":"<<view2dFrames
              <<",\"view_initial_enabled\":"<<(initialPresentation?"true":"false")
              <<",\"view_final_enabled\":"<<(presentationEnabled?"true":"false")
              <<",\"map_changes\":"<<mapChanges<<",\"lifecycle_restore_checks\":"<<restores
              <<",\"ps4_hardware_tested\":false,\"memory_writes\":false,\"actors\":\"ROM sprites with authored palette\"}\n";
        require(bool(report)&&bool(dump)&&bool(traceFile),"Cannot finish evidence files");
        emulator.unload();
        // Persist presentation only when requested, after gameplay has stopped.
        if(preferenceDirty) require(saveViewPreference(data,presentationEnabled,error),error);
        WorkRam unloaded{};require(!snapshot(emulator,unloaded),"Unloaded session retained memory");
        std::cout<<"Completed "<<executed<<" frames; Pallet="<<palletFrames<<", positions="<<positions.size()<<'\n';
        return 0;
    } catch(const std::exception& e) {std::cerr<<"pokemon3d_live: "<<e.what()<<'\n';return 1;}
}
