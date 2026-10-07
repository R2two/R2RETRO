#include "n64_profiles.h"
#include <cstdio>
#include <stdexcept>
#include <cstring>
using namespace r2n64;
struct Host : HardwareVideoHost {
    bool prepare(unsigned,unsigned,bool,bool,std::string&) override { return false; }
    bool begin(std::string&) override { return false; }
    bool end(std::string&) override { return false; }
    uintptr_t framebuffer() const override { return 0; }
    Procedure procedure(const char*) override { return nullptr; }
};
static void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        Host host;
        const char* ids[]={"635A2BFF-8B022326-623B80DDBD00E7B7","693BA2AE-B7F14E9F-6E8332B2F362A221"};
        const size_t sizes[]={8*1024*1024,32*1024*1024};
        for (unsigned i=0;i<2;++i) {
            EmulationConfig c; c.hardware=&host; c.automaticProfile=true;
            c.cpuMode=CpuMode::CachedInterpreter; c.audioHle=false;
            c.graphicsHle=false; c.profileCore=true; c.workers=1;
            auto manual=c; manual.automaticProfile=false;
            require(std::strcmp(applyN64Profile(ids[i],sizes[i],manual),"Manual")==0 &&
                manual.graphics==N64Graphics::Software && !manual.audioHle,"manual overrides recognized profiles");
            auto missing=c; missing.hardware=nullptr;
            applyN64Profile(ids[i],sizes[i],missing);
            require(missing.graphics==N64Graphics::Software && !missing.audioHle,"no GPU without host");
            auto wrong=c;
            applyN64Profile(ids[i],sizes[i]+4,wrong);
            require(wrong.graphics==N64Graphics::Software,"exact revision size required");
            auto changed=c; std::string id=ids[i]; id.back()='0';
            applyN64Profile(id,sizes[i],changed);
            require(changed.graphics==N64Graphics::Software && !changed.audioHle,"modified ROM with original header must not match");
            const auto* label=applyN64Profile(ids[i],sizes[i],c);
            require(std::strstr(label,"experimental") && c.graphics==N64Graphics::Gles2 &&
                c.graphicsHle && c.audioHle && c.cpuMode==CpuMode::Automatic,"recognized profile not applied");
            require(c.profileCore && c.workers==1 && c.hardware==&host,"independent options preserved");
            // CPU fallback retry must not reenable GPU through the auto profile.
            c.graphics=N64Graphics::Software; c.hardware=nullptr; c.automaticProfile=false;
            applyN64Profile(ids[i],sizes[i],c);
            require(c.graphics==N64Graphics::Software,"fallback must stay on CPU");
        }
        std::puts("PASS: N64 profiles, revisions, manual overrides, hardware availability and fallback");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
