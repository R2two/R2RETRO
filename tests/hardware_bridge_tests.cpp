// Original fake core/host: exercises libretro HW lifecycle without a GPU or ROM.
#include "core/libretro_core.h"
#include "log.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

using namespace r2n64;
namespace fs = std::filesystem;
namespace {
void require(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
std::vector<std::string> events;
retro_environment_t environment = nullptr;
retro_video_refresh_t video = nullptr;
retro_hw_render_callback hw{};
bool gpuCore = false, contextCreated = false, wrongContext = false;
bool sendPixelFormat = false, negotiate = true, rejectLoad = false, resetError = false;
bool duplicateFrame = false, oversizedFrame = false, softwareFrame = false;
retro_hw_context_type requestedContext = RETRO_HW_CONTEXT_OPENGLES2;
unsigned coreRuns = 0, resets = 0, destroys = 0, coreUnloads = 0, coreDeinits = 0;

struct Host final : HardwareVideoHost {
    bool prepared = false, active = false, rejectPrepare = false, rejectBegin = false, rejectEnd = false;
    unsigned prepares = 0, begins = 0, ends = 0;
    static void dummyProcedure() {}
    bool prepare(unsigned width, unsigned height, bool depth, bool stencil, std::string& error) override {
        events.emplace_back("prepare"); ++prepares;
        require(width == 640 && height == 480 && depth && !stencil,"Unexpected HW framebuffer requirements");
        if (rejectPrepare) { error = "synthetic shader/FBO rejection"; return false; }
        prepared = true; return true;
    }
    bool begin(std::string& error) override {
        events.emplace_back("begin"); ++begins;
        if (rejectBegin) { error = "synthetic context unavailable"; return false; }
        require(prepared && !active,"Nested/unprepared host entry"); active = true; return true;
    }
    bool end(std::string& error) override {
        events.emplace_back("end"); ++ends;
        require(active,"Host end without matching begin"); active = false;
        if (rejectEnd) { error = "synthetic GL error with SDL restored"; return false; }
        return true;
    }
    uintptr_t framebuffer() const override { return prepared ? 123u : 0u; }
    Procedure procedure(const char* name) override {
        return name && std::strcmp(name,"glSynthetic") == 0 ? dummyProcedure : nullptr;
    }
};
Host* host = nullptr;
void contextReset() {
    events.emplace_back("context_reset"); ++resets;
    if (!host || !host->active) wrongContext = true;
    require(hw.get_current_framebuffer && hw.get_current_framebuffer() == 123,"Wrong current framebuffer bridge");
    require(hw.get_proc_address && hw.get_proc_address("glSynthetic") == Host::dummyProcedure,"Missing procedure bridge");
    contextCreated = true;
    if (resetError) environment(RETRO_ENVIRONMENT_SHUTDOWN,nullptr);
}
void contextDestroy() {
    events.emplace_back("context_destroy"); ++destroys;
    if (!host || !host->active) wrongContext = true;
    contextCreated = false;
}
void init() {
    events.emplace_back("init");
    retro_variable variable{"mupen64plus-rdp-plugin",nullptr};
    require(environment(RETRO_ENVIRONMENT_GET_VARIABLE,&variable) && variable.value,"Missing renderer selection");
    gpuCore = std::strcmp(variable.value,"gliden64") == 0;
    retro_variable rsp{"mupen64plus-rsp-plugin",nullptr};
    require(environment(RETRO_ENVIRONMENT_GET_VARIABLE,&rsp) && rsp.value,"Missing RSP mode");
    require(std::strcmp(rsp.value,gpuCore ? "hle" : "cxd4")==0,"Hybrid RSP leaked into software or was not enabled for GPU");
    if (sendPixelFormat || !gpuCore) {
        retro_pixel_format format = RETRO_PIXEL_FORMAT_XRGB8888;
        require(environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT,&format),"Pixel format rejected");
    }
}
void deinit() { events.emplace_back("deinit"); ++coreDeinits; if (contextCreated && (!host || !host->active)) wrongContext = true; }
unsigned apiVersion() { return RETRO_API_VERSION; }
void info(retro_system_info* value) { *value = {"Synthetic HW","hw-test-1","z64",false,false}; }
void av(retro_system_av_info* value) { *value = {{320,240,640,480,4.0f/3.0f},{60,48000}}; }
void setEnv(retro_environment_t value) { environment = value; }
void setVideo(retro_video_refresh_t value) { video = value; }
void setSample(retro_audio_sample_t) {}
void setBatch(retro_audio_sample_batch_t) {}
void setPoll(retro_input_poll_t) {}
void setInput(retro_input_state_t) {}
void controller(unsigned,unsigned) {}
void resetCore() { throw std::runtime_error("N64 reset must use complete load/unload lifecycle"); }
void run() {
    events.emplace_back("run"); ++coreRuns;
    if (gpuCore && (!host || !host->active)) wrongContext = true;
    if (duplicateFrame) { video(nullptr,0,0,0); return; }
    static uint32_t pixel = 0x123456;
    video(gpuCore && !softwareFrame ? RETRO_HW_FRAME_BUFFER_VALID : &pixel,
          gpuCore ? (oversizedFrame ? 641 : 320) : 1, gpuCore ? 240 : 1,4);
}
bool load(const retro_game_info* game) {
    events.emplace_back("load");
    require(game && game->data && game->size == 4096 && !game->path,"Unexpected in-memory diagnostic content");
    if (gpuCore && negotiate) {
        hw = {}; hw.context_type = requestedContext; hw.context_reset = contextReset;
        hw.context_destroy = contextDestroy; hw.depth = true; hw.bottom_left_origin = true;
        if (!environment(RETRO_ENVIRONMENT_SET_HW_RENDER,&hw)) return false;
    }
    return !rejectLoad;
}
void unload() { events.emplace_back("unload"); ++coreUnloads; if (contextCreated && (!host || !host->active)) wrongContext = true; }
size_t serializeSize() { return 4; }
bool serialize(void*,size_t) { return true; }
bool unserialize(const void*,size_t) { return true; }
void* memory(unsigned) { return nullptr; }
size_t memorySize(unsigned) { return 0; }
const CoreApi api{init,deinit,apiVersion,info,av,setEnv,setVideo,setSample,setBatch,setPoll,setInput,
    controller,resetCore,run,load,unload,serializeSize,serialize,unserialize,memory,memorySize};
const CoreDescriptor descriptor{"synthetic_hw","Synthetic hardware",{SystemType::Nintendo64,SystemType::Unknown},"z64",true};
EmulationConfig config(Host* selected, bool gpu=true) {
    host = selected;
    EmulationConfig result; result.graphics = gpu ? N64Graphics::Gles2 : N64Graphics::Software;
    result.hardware = selected; result.cpuMode = CpuMode::CachedInterpreter; result.audioHle = false;
    result.profileCore = true;
    return result;
}
void cleanFlags() {
    require(!contextCreated,"Earlier context leaked into a new test");
    sendPixelFormat = false; negotiate = true; rejectLoad = resetError = false;
    duplicateFrame = oversizedFrame = softwareFrame = false;
    requestedContext = RETRO_HW_CONTEXT_OPENGLES2;
    events.clear(); wrongContext = false;
}
}
int main() {
    try {
        char pattern[] = "hardware-bridge-tests-XXXXXX";
        char* created = ::mkdtemp(pattern); require(created != nullptr,"Cannot create isolated test root");
        const auto root = fs::absolute(created);
        const auto rom = root / "synthetic.z64";
        std::vector<unsigned char> content(4096);
        content[0]=0x80; content[1]=0x37; content[2]=0x12; content[3]=0x40;
        std::memcpy(content.data()+32,"ORIGINAL HW FIXTURE",19); content[62]='E';
        { std::ofstream out(rom,std::ios::binary); out.write(reinterpret_cast<const char*>(content.data()),content.size()); require(bool(out),"Cannot write diagnostic header"); }
        fs::create_directories(root / "logs");
        Log log; require(log.open(root.string()),"Cannot open test log");
        LibretroCore core(descriptor,api);
        std::string error; GamepadInput input{};

        require(!core.load(rom.string(),root.string(),log,error,config(nullptr)) && !error.empty(),"HW load accepted missing host");
        cleanFlags(); Host happy;
        require(core.load(rom.string(),root.string(),log,error,config(&happy)),error);
        require(resets == 1 && !happy.active && happy.ends == 1 && !wrongContext,"context_reset not bracketed by host");
        require(!core.frame().hardware && core.frame().pixels.empty(),"HW output fabricated before run");
        require(core.run(input,error),error);
        require(core.hardwareTiming().calls==1,"GPU run boundary was not measured separately");
        require(core.frame().hardware && core.frame().bottomLeftOrigin && core.frame().width==320 &&
                core.frame().height==240 && core.frame().pixels.empty(),"HW sentinel was treated as a CPU pointer or orientation lost");
        require(!core.supportsSaveStates(),"GPU session offered unsupported quick states");
        duplicateFrame = true;
        require(core.run(input,error) && core.frame().hardware && core.frame().width==320,"NULL duplicate discarded the HW frame");
        duplicateFrame = false;
        const auto beforeReset = resets, beforeDestroy = destroys;
        require(core.reset(error),error);
        require(resets==beforeReset+1 && destroys==beforeDestroy+1 && !happy.active && !wrongContext,
                "N64 reset failed to destroy/recreate HW context inside host scopes");
        require(core.run(input,error),error);
        core.unload();
        const auto afterUnload = coreUnloads, afterDestroy = destroys;
        core.unload();
        require(coreUnloads==afterUnload && destroys==afterDestroy && !wrongContext && !happy.active,
                "Repeated unload used invalid context or destroyed it twice");

        cleanFlags(); Host missing;
        negotiate = false;
        require(!core.load(rom.string(),root.string(),log,error,config(&missing)) &&
                error.find("negocio")!=std::string::npos && !missing.prepares,"Missing HW negotiation was accepted");
        cleanFlags(); requestedContext = RETRO_HW_CONTEXT_OPENGL_CORE;
        require(!core.load(rom.string(),root.string(),log,error,config(&missing)) && !error.empty(),"Unsupported context accepted");

        cleanFlags(); Host preflight; preflight.rejectPrepare = true;
        const auto rejectedReset = resets;
        require(!core.load(rom.string(),root.string(),log,error,config(&preflight)) &&
                error.find("shader/FBO")!=std::string::npos && resets==rejectedReset && !wrongContext,
                "Preflight rejection ran context_reset or lost its diagnostic");
        require(core.load(rom.string(),root.string(),log,error,config(nullptr,false)),error);
        require(core.run(input,error) && !core.frame().hardware && core.frame().pixels.size()==1,
                "Software retry retained failed GPU state");
        core.unload();

        cleanFlags(); Host resetFailure; resetError = true;
        const auto resetDestroy = destroys;
        require(!core.load(rom.string(),root.string(),log,error,config(&resetFailure)) && destroys==resetDestroy+1 &&
                !resetFailure.active && !wrongContext,"context_reset failure did not restore/close HW safely");

        cleanFlags(); Host badFrame;
        require(core.load(rom.string(),root.string(),log,error,config(&badFrame)),error);
        oversizedFrame = true;
        require(!core.run(input,error) && !badFrame.active && !error.empty(),"Oversized HW frame escaped bounds or host scope");
        core.unload();
        cleanFlags();
        require(core.load(rom.string(),root.string(),log,error,config(&badFrame)),error);
        softwareFrame = true;
        require(!core.run(input,error) && !badFrame.active && !error.empty(),"GPU profile accepted a software video pointer");
        core.unload();

        cleanFlags(); Host endFailure;
        require(core.load(rom.string(),root.string(),log,error,config(&endFailure)),error);
        endFailure.rejectEnd = true;
        require(!core.run(input,error) && !endFailure.active && error.find("GL error")!=std::string::npos,
                "Host end failure was hidden or left the host active");
        endFailure.rejectEnd = false; core.unload();
        require(!wrongContext,"Error cleanup called a GPU function outside its host scope");

        // Terminal loss is deliberately last: no subsequent core may be loaded
        // if GL cleanup cannot safely enter the original context. It must not
        // try destroying GL resources in some unrelated current SDL context.
        cleanFlags(); Host lost;
        require(core.load(rom.string(),root.string(),log,error,config(&lost)),error);
        lost.rejectBegin = true;
        const auto terminalRuns=coreRuns, terminalDestroys=destroys, terminalUnloads=coreUnloads, terminalDeinits=coreDeinits;
        require(!core.run(input,error) && coreRuns==terminalRuns,"Core ran after host context entry failed");
        core.unload();
        require(destroys==terminalDestroys && coreUnloads==terminalUnloads && coreDeinits==terminalDeinits && !wrongContext,
                "Lost context invoked GL cleanup without entering the host");
        require(!core.load(rom.string(),root.string(),log,error,config(nullptr,false)) && !error.empty(),
                "A new core was loaded after unrecoverable GPU context loss");
        std::puts("PASS: libretro GLES2 negotiation, sentinel/duplicate/orientation, context scopes, reset/unload, preflight software retry, bounded failures and terminal context-loss guard.");
        return 0;
    } catch (const std::exception& exception) {
        std::fprintf(stderr,"FAIL: %s\n",exception.what()); return 1;
    }
}
