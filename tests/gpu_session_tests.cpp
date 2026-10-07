#include "gpu_session.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
bool failCompiler=false, failFramebuffer=false;
unsigned sessionReadbacks=0;
unsigned capabilityWrites=0;
void (GL_APIENTRY *realEnable)(GLenum)=nullptr;
void (GL_APIENTRY *realDisable)(GLenum)=nullptr;
void GL_APIENTRY testEnable(GLenum cap) { ++capabilityWrites; realEnable(cap); }
void GL_APIENTRY testDisable(GLenum cap) { ++capabilityWrites; realDisable(cap); }
void (GL_APIENTRY *realGetBooleanv)(GLenum,GLboolean*)=nullptr;
GLenum (GL_APIENTRY *realFramebufferStatus)(GLenum)=nullptr;
void (GL_APIENTRY *realReadPixels)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*)=nullptr;
void GL_APIENTRY testGetBooleanv(GLenum name, GLboolean* value) {
    if (name==GL_SHADER_COMPILER && failCompiler) { *value=GL_FALSE; return; }
    realGetBooleanv(name,value);
}
GLenum GL_APIENTRY testFramebufferStatus(GLenum target) {
    return failFramebuffer?GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:realFramebufferStatus(target);
}
void GL_APIENTRY testReadPixels(GLint x,GLint y,GLsizei w,GLsizei h,GLenum format,GLenum type,void* pixels) {
    ++sessionReadbacks; realReadPixels(x,y,w,h,format,type,pixels);
}
}
extern "C" void* __real_SDL_GL_GetProcAddress(const char* name);
extern "C" void* __wrap_SDL_GL_GetProcAddress(const char* name) {
    void* result=__real_SDL_GL_GetProcAddress(name);
    if (!std::strcmp(name,"glEnable")) {
        realEnable=reinterpret_cast<decltype(realEnable)>(result);
        return reinterpret_cast<void*>(testEnable);
    }
    if (!std::strcmp(name,"glDisable")) {
        realDisable=reinterpret_cast<decltype(realDisable)>(result);
        return reinterpret_cast<void*>(testDisable);
    }
    if (!std::strcmp(name,"glGetBooleanv")) {
        realGetBooleanv=reinterpret_cast<decltype(realGetBooleanv)>(result);
        return reinterpret_cast<void*>(testGetBooleanv);
    }
    if (!std::strcmp(name,"glCheckFramebufferStatus")) {
        realFramebufferStatus=reinterpret_cast<decltype(realFramebufferStatus)>(result);
        return reinterpret_cast<void*>(testFramebufferStatus);
    }
    if (!std::strcmp(name,"glReadPixels")) {
        realReadPixels=reinterpret_cast<decltype(realReadPixels)>(result);
        return reinterpret_cast<void*>(testReadPixels);
    }
    return result;
}

namespace {
using r2n64::GpuSession;
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message+": "+SDL_GetError());
}
template<class T> T procedure(GpuSession& gpu,const char* name) {
    const auto result=reinterpret_cast<T>(gpu.procedure(name));
    require(result!=nullptr,std::string("Missing ")+name); return result;
}
#define TEST_GL(X) \
 X(void,GetIntegerv,(GLenum,GLint*)) \
 X(void,GetFloatv,(GLenum,GLfloat*)) \
 X(void,GetBooleanv,(GLenum,GLboolean*)) \
 X(GLboolean,IsEnabled,(GLenum)) \
 X(void,ClearColor,(GLfloat,GLfloat,GLfloat,GLfloat)) \
 X(void,Clear,(GLbitfield)) \
 X(void,Scissor,(GLint,GLint,GLsizei,GLsizei)) \
 X(void,Viewport,(GLint,GLint,GLsizei,GLsizei)) \
 X(void,Enable,(GLenum)) \
 X(void,Disable,(GLenum)) \
 X(void,ColorMask,(GLboolean,GLboolean,GLboolean,GLboolean)) \
 X(void,DepthMask,(GLboolean)) \
 X(void,PixelStorei,(GLenum,GLint)) \
 X(void,UseProgram,(GLuint)) \
 X(void,GenBuffers,(GLsizei,GLuint*)) \
 X(void,BindBuffer,(GLenum,GLuint)) \
 X(void,BufferData,(GLenum,GLsizeiptr,const void*,GLenum)) \
 X(void,DeleteBuffers,(GLsizei,const GLuint*)) \
 X(void,GenTextures,(GLsizei,GLuint*)) \
 X(void,ActiveTexture,(GLenum)) \
 X(void,BindTexture,(GLenum,GLuint)) \
 X(void,DeleteTextures,(GLsizei,const GLuint*)) \
 X(void,BlendFuncSeparate,(GLenum,GLenum,GLenum,GLenum)) \
 X(void,BlendEquationSeparate,(GLenum,GLenum)) \
 X(void,DisableVertexAttribArray,(GLuint)) \
 X(void,VertexAttribPointer,(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*)) \
 X(void,GetVertexAttribiv,(GLuint,GLenum,GLint*)) \
 X(GLenum,GetError,(void))
struct GL {
#define MEMBER(ret,name,args) ret (GL_APIENTRY *name) args;
    TEST_GL(MEMBER)
#undef MEMBER
    explicit GL(GpuSession& gpu) {
#define LOAD(ret,name,args) name=procedure<decltype(name)>(gpu,"gl" #name);
        TEST_GL(LOAD)
#undef LOAD
    }
};
#undef TEST_GL
std::vector<uint32_t> pixels(SDL_Renderer* renderer) {
    std::vector<uint32_t> result(128*96);
    require(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,result.data(),128*4)==0,"capture SDL");
    return result;
}
void drawUi(SDL_Renderer* renderer,SDL_Texture* texture) {
    require(SDL_SetRenderDrawColor(renderer,0,210,0,255)==0 && SDL_RenderClear(renderer)==0,"clear UI");
    const SDL_Rect destination{48,32,32,32};
    require(SDL_RenderCopy(renderer,texture,nullptr,&destination)==0,"copy UI");
    const auto image=pixels(renderer);
    require((image[2*128+2]&0xffffff)==0x00d200 && (image[48*128+64]&0xffffff)==0xc80000,
            "SDL color/texture cache corrupted by shared core context");
}
std::vector<GLint> state(GL& gl) {
    std::vector<GLint> result;
    for (const auto name : {GL_CURRENT_PROGRAM,GL_FRAMEBUFFER_BINDING,GL_RENDERBUFFER_BINDING,
            GL_ARRAY_BUFFER_BINDING,GL_ELEMENT_ARRAY_BUFFER_BINDING,GL_ACTIVE_TEXTURE,
            GL_PACK_ALIGNMENT,GL_UNPACK_ALIGNMENT,GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,
            GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA}) {
        GLint value; gl.GetIntegerv(name,&value); result.push_back(value);
    }
    for (const auto name : {GL_VIEWPORT,GL_SCISSOR_BOX}) {
        GLint values[4]; gl.GetIntegerv(name,values); result.insert(result.end(),values,values+4);
    }
    for (const auto name : {GL_BLEND,GL_SCISSOR_TEST,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_DITHER})
        result.push_back(gl.IsEnabled(name));
    GLint active; gl.GetIntegerv(GL_ACTIVE_TEXTURE,&active);
    for (unsigned unit=0;unit<3;++unit) {
        gl.ActiveTexture(GL_TEXTURE0+unit);
        GLint texture; gl.GetIntegerv(GL_TEXTURE_BINDING_2D,&texture); result.push_back(texture);
    }
    gl.ActiveTexture(active);
    for (const auto name : {GL_DEPTH_WRITEMASK,GL_COLOR_WRITEMASK}) {
        GLboolean values[4]{}; gl.GetBooleanv(name,values);
        for (const auto value : values) result.push_back(value);
    }
    GLfloat clearColor[4]; gl.GetFloatv(GL_COLOR_CLEAR_VALUE,clearColor);
    for (auto value : clearColor) result.push_back(static_cast<GLint>(value*255));
    for (unsigned i=0;i<3;++i) {
        for (const auto name : {GL_VERTEX_ATTRIB_ARRAY_ENABLED,GL_VERTEX_ATTRIB_ARRAY_SIZE,
                GL_VERTEX_ATTRIB_ARRAY_TYPE,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
                GL_VERTEX_ATTRIB_ARRAY_STRIDE,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING}) {
            GLint value; gl.GetVertexAttribiv(i,name,&value); result.push_back(value);
        }
    }
    return result;
}
}

int main(int argc,char** argv) {
    const bool software=argc>1 && std::string(argv[1])=="--software";
    SDL_Window* window=nullptr; SDL_Renderer* renderer=nullptr; SDL_Texture* ui=nullptr;
    std::unique_ptr<GpuSession> gpu;
    try {
        std::string error;
        { GpuSession missing(nullptr,nullptr);
          require(!missing.prepare(64,48,true,false,error) && !error.empty() && !missing.ready(),"null renderer rejection"); }
        require(SDL_Init(SDL_INIT_VIDEO)==0,"SDL initialize");
        window=SDL_CreateWindow("Shared GLES2 core test",0,0,128,96,SDL_WINDOW_HIDDEN);
        require(window!=nullptr,"SDL window");
        renderer=SDL_CreateRenderer(window,-1,software?SDL_RENDERER_SOFTWARE:SDL_RENDERER_ACCELERATED);
        require(renderer!=nullptr,"SDL renderer");
        ui=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,2,2);
        require(ui!=nullptr,"UI texture");
        const uint32_t red[]{0xffc80000,0xffc80000,0xffc80000,0xffc80000};
        require(SDL_UpdateTexture(ui,nullptr,red,8)==0,"UI contents");
        drawUi(renderer,ui);
        gpu=std::make_unique<GpuSession>(window,renderer);
        const auto originalContext=SDL_GL_GetCurrentContext();
        if (software) {
            r2n64::DisplayShader effect(window, renderer);
            require(effect.select(0, error) && !effect.select(1, error) && effect.mode() == 0,
                    "Software renderer must keep display shader disabled");
            require(!gpu->prepare(64,48,true,false,error) && !error.empty() && !gpu->ready() && !gpu->framebuffer(),
                    "Software renderer must decline without touching UI");
            drawUi(renderer,ui);
        } else {
            failCompiler=true;
            require(!gpu->prepare(64,48,true,false,error) && error.find("GLSL")!=std::string::npos && !gpu->ready(),
                    "Missing shader compiler must reject GPU mode");
            failCompiler=false; drawUi(renderer,ui);
            failFramebuffer=true;
            require(!gpu->prepare(64,48,true,false,error) && error.find("framebuffer")!=std::string::npos && !gpu->ready(),
                    "Incomplete framebuffer must reject GPU mode");
            failFramebuffer=false; drawUi(renderer,ui);
            require(gpu->prepare(64,48,true,false,error),error);
            require(gpu->ready() && gpu->framebuffer()!=0,"GPU FBO missing");
            const unsigned preparationReads=sessionReadbacks;
            require(preparationReads==1,"Capability probe must read exactly one pixel after valid setup");
            const auto fbo=gpu->framebuffer();
            require(gpu->prepare(64,48,true,false,error) && gpu->framebuffer()==fbo && sessionReadbacks==preparationReads,
                    "Unchanged dimensions recreated or reprobed GPU resources");
            require(!gpu->prepare(641,480,true,false,error) && gpu->ready() && gpu->framebuffer()==fbo,
                    "Invalid dimensions lost existing GPU resources");
            GL gl(*gpu);
            const auto initialState=state(gl);
            capabilityWrites=0;
            require(gpu->begin(error) && gpu->end(error),error);
            require(state(gl)==initialState,"differential restore changed untouched frontend state");
            require(capabilityWrites<18,"unchanged GL capabilities must not be rewritten at every boundary");
            std::printf("Boundary capability writes: %u (full restore: 18)\n",capabilityWrites);
            require(!gpu->driverDescription().empty(),"GPU driver description missing");
            GLuint buffer=0,texture=0;
            std::vector<GLint> previousCore;
            for (unsigned frame=0;frame<6;++frame) {
                drawUi(renderer,ui);
                const auto before=state(gl);
                require(gpu->begin(error),error);
                if (frame) require(state(gl)==previousCore,"Core GL state did not restore after SDL presentation");
                else {
                    GLint program=-1,attribute=-1;
                    gl.GetIntegerv(GL_CURRENT_PROGRAM,&program);
                    gl.GetVertexAttribiv(0,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&attribute);
                    require(!program && !attribute && !gl.IsEnabled(GL_BLEND) && !gl.IsEnabled(GL_SCISSOR_TEST),
                            "Initial core state inherited SDL program, attributes or capabilities");
                }
                require(!gpu->begin(error),"Nested core entry accepted");
                GLint bound=0; gl.GetIntegerv(GL_FRAMEBUFFER_BINDING,&bound);
                require(uintptr_t(bound)==fbo,"Core was not given its framebuffer");
                gl.Disable(GL_SCISSOR_TEST); gl.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
                gl.ClearColor(0,0,1,1); gl.Clear(GL_COLOR_BUFFER_BIT);
                gl.Enable(GL_SCISSOR_TEST); gl.Scissor(0,0,64,24);
                gl.ClearColor(1,0,0,1); gl.Clear(GL_COLOR_BUFFER_BIT);
                if (!buffer) {
                    gl.GenBuffers(1,&buffer); gl.GenTextures(1,&texture);
                    gl.BindBuffer(GL_ARRAY_BUFFER,buffer);
                    const GLfloat value[8]{}; gl.BufferData(GL_ARRAY_BUFFER,sizeof(value),value,GL_STATIC_DRAW);
                }
                // Exercise the SDL cache restoration even when a core leaves
                // its own program, buffers, attribs, pixel modes and masks set.
                gl.UseProgram(0); gl.BindBuffer(GL_ARRAY_BUFFER,buffer); gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffer);
                for (unsigned i=0;i<3;++i) { gl.DisableVertexAttribArray(i); gl.VertexAttribPointer(i,2,GL_FLOAT,GL_FALSE,8,nullptr); }
                gl.ActiveTexture(GL_TEXTURE2); gl.BindTexture(GL_TEXTURE_2D,texture);
                gl.Viewport(1,2,3,4); gl.Scissor(1,1,2,2); gl.DepthMask(GL_FALSE);
                gl.ColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE); gl.PixelStorei(GL_PACK_ALIGNMENT,8); gl.PixelStorei(GL_UNPACK_ALIGNMENT,8);
                gl.Enable(GL_DEPTH_TEST); gl.Enable(GL_STENCIL_TEST); gl.Enable(GL_CULL_FACE); gl.Enable(GL_BLEND);
                gl.BlendFuncSeparate(GL_DST_COLOR,GL_ZERO,GL_ZERO,GL_ONE); gl.BlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT,GL_FUNC_ADD);
                gl.ClearColor(0.75f,0.25f,0.5f,0.125f);
                previousCore=state(gl);
                require(gpu->end(error),error);
                require(state(gl)==before,"Frontend GL state did not restore after core frame");
                require(SDL_GL_GetCurrentContext()==originalContext,"Shared context identity changed");
                require(gpu->draw(64,48,true,SDL_Rect{0,0,128,96},error),error);
                auto image=pixels(renderer);
                require((image[16*128+64]&0xffffff)==0x0000ff && (image[80*128+64]&0xffffff)==0xff0000,
                        "GPU texture colors or bottom-left orientation incorrect");
                require(gpu->draw(64,48,false,SDL_Rect{0,0,128,96},error),error);
                image=pixels(renderer);
                require((image[16*128+64]&0xffffff)==0xff0000 && (image[80*128+64]&0xffffff)==0x0000ff,
                        "GPU top-left orientation incorrect");
                require(sessionReadbacks==preparationReads,"Core frames or GPU presentation performed host readback");
                drawUi(renderer,ui);
            }
            require(gpu->begin(error),error); gl.DeleteBuffers(1,&buffer); gl.DeleteTextures(1,&texture);
            require(gpu->end(error),error);
            require(!gpu->draw(65,48,true,error) && !gpu->end(error),"Invalid frame size or unmatched end accepted");
            // The bridge allocates 640x480, but GLide commonly submits only
            // the lower-left 320x240 region. Do not sample the unused half.
            require(gpu->prepare(640,480,true,false,error),error);
            const unsigned largePreparationReads=sessionReadbacks;
            require(gpu->begin(error),error);
            gl.Disable(GL_SCISSOR_TEST); gl.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            gl.ClearColor(1,0,1,1); gl.Clear(GL_COLOR_BUFFER_BIT);
            gl.Viewport(0,0,320,240); gl.Enable(GL_SCISSOR_TEST); gl.Scissor(0,0,320,240);
            gl.ClearColor(0,0,1,1); gl.Clear(GL_COLOR_BUFFER_BIT);
            gl.Scissor(0,0,320,120); gl.ClearColor(1,0,0,1); gl.Clear(GL_COLOR_BUFFER_BIT);
            require(gpu->end(error),error);
            require(gpu->draw(320,240,true,SDL_Rect{0,0,128,96},error),error);
            const auto subregion=pixels(renderer);
            for (unsigned y=0;y<96;++y) for (unsigned x=0;x<128;++x)
                require((subregion[y*128+x]&0xffffff)==(y<48?0x0000ffU:0xff0000U),
                        "640x480 target did not crop/orient the lower-left 320x240 region");
            require(sessionReadbacks==largePreparationReads,"Subregion presentation performed host readback");
            // Reconfigure after drawing the old texture, then release. A saved
            // texture binding must never recreate a texture name just deleted.
            require(gpu->prepare(128,96,true,false,error),error);
            require(sessionReadbacks==largePreparationReads+1,"New dimensions did not perform a fresh capability check");
            drawUi(renderer,ui);
            gpu->reset(); require(!gpu->ready() && !gpu->framebuffer(),"GPU reset retained resources");
            require(gl.GetError()==GL_NO_ERROR,"GPU cleanup left a GL error");
            drawUi(renderer,ui);
            require(gpu->prepare(64,48,false,false,error),error);
            gpu->reset(); drawUi(renderer,ui);
            {
                r2n64::DisplayShader effect(window, renderer);
                require(SDL_RenderSetLogicalSize(renderer, 128, 96) == 0, "Shader logical canvas");
                const unsigned readsBefore = sessionReadbacks;
                for (unsigned mode : {1u, 2u, 0u, 1u, 0u}) {
                    require(SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255) == 0, "Shader test color");
                    require(SDL_RenderFillRect(renderer, nullptr) == 0 && SDL_RenderFlush(renderer) == 0, "Shader test canvas");
                    const auto before = state(gl);
                    require(effect.select(mode, error), error);
                    require(state(gl) == before, "Shader selection altered SDL state");
                    require(effect.draw(16, 12, SDL_Rect{16,12,96,72}, error), error);
                    require(state(gl) == before, "Display shader did not restore SDL state");
                    require(sessionReadbacks == readsBefore, "Display shader performed a readback");
                    const auto shaded = pixels(renderer);
                    unsigned darkened = 0;
                    for (unsigned y = 0; y < 96; ++y) for (unsigned x = 0; x < 128; ++x) {
                        const auto rgb = shaded[y*128+x] & 0xffffff;
                        if (!mode || x < 16 || x >= 112 || y < 12 || y >= 84)
                            require(rgb == 0xffffff, "Shader affected artwork/UI outside the game, or off changed output");
                        else darkened += rgb != 0xffffff;
                    }
                    require(!mode || darkened > 100, "Selected display shader has no visible effect");
                    drawUi(renderer, ui);
                }
                require(!effect.select(3, error) && effect.mode() == 0, "Invalid shader mode accepted");
            }
            drawUi(renderer, ui);
        }
        gpu.reset(); SDL_DestroyTexture(ui); ui=nullptr;
        SDL_DestroyRenderer(renderer); renderer=nullptr; SDL_DestroyWindow(window); window=nullptr; SDL_Quit();
        std::puts(software?"PASS: software fallback retains SDL UI":"PASS: shared GLES2 shader/FBO capability, failed setup recovery, GPU texture presentation/orientation, GL state restoration, reuse/reset and no per-frame readback");
        return 0;
    } catch (const std::exception& exception) {
        std::fprintf(stderr,"FAIL: %s\n",exception.what()); gpu.reset();
        if (ui) SDL_DestroyTexture(ui);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit(); return 1;
    }
}
