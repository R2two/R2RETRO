#include "gpu_session.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>

namespace r2n64 {
namespace {
constexpr unsigned MaxWidth = 640, MaxHeight = 480;
constexpr unsigned MaxAttributes = 32;
// SDL's GLES2 renderer touches units 0,1,2 (RGB and YUV). Preserve these on
// both sides; higher units are not changed by SDL between core entries.
constexpr unsigned SdlTextureUnits = 3;
constexpr GLenum VertexArrayBinding = 0x85b5, PackedDepthStencil = 0x88f0;

#define SESSION_GL_FUNCTIONS(X) \
    X(const GLubyte*, GetString, (GLenum)) \
    X(GLenum, GetError, (void)) \
    X(void, GetIntegerv, (GLenum, GLint*)) \
    X(void, GetFloatv, (GLenum, GLfloat*)) \
    X(void, GetBooleanv, (GLenum, GLboolean*)) \
    X(GLboolean, IsEnabled, (GLenum)) \
    X(void, Enable, (GLenum)) \
    X(void, Disable, (GLenum)) \
    X(void, ActiveTexture, (GLenum)) \
    X(void, BindTexture, (GLenum, GLuint)) \
    X(void, BindBuffer, (GLenum, GLuint)) \
    X(void, GenBuffers, (GLsizei, GLuint*)) \
    X(void, BufferData, (GLenum, GLsizeiptr, const void*, GLenum)) \
    X(void, DeleteBuffers, (GLsizei, const GLuint*)) \
    X(void, BindFramebuffer, (GLenum, GLuint)) \
    X(void, GenFramebuffers, (GLsizei, GLuint*)) \
    X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(void, DeleteFramebuffers, (GLsizei, const GLuint*)) \
    X(GLenum, CheckFramebufferStatus, (GLenum)) \
    X(void, GenRenderbuffers, (GLsizei, GLuint*)) \
    X(void, BindRenderbuffer, (GLenum, GLuint)) \
    X(void, RenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei)) \
    X(void, FramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint)) \
    X(void, DeleteRenderbuffers, (GLsizei, const GLuint*)) \
    X(void, UseProgram, (GLuint)) \
    X(GLuint, CreateShader, (GLenum)) \
    X(void, ShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
    X(void, CompileShader, (GLuint)) \
    X(void, GetShaderiv, (GLuint, GLenum, GLint*)) \
    X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void, DeleteShader, (GLuint)) \
    X(GLuint, CreateProgram, (void)) \
    X(void, AttachShader, (GLuint, GLuint)) \
    X(void, BindAttribLocation, (GLuint, GLuint, const GLchar*)) \
    X(void, LinkProgram, (GLuint)) \
    X(void, GetProgramiv, (GLuint, GLenum, GLint*)) \
    X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void, DeleteProgram, (GLuint)) \
    X(void, EnableVertexAttribArray, (GLuint)) \
    X(void, DisableVertexAttribArray, (GLuint)) \
    X(void, GetVertexAttribiv, (GLuint, GLenum, GLint*)) \
    X(void, GetVertexAttribfv, (GLuint, GLenum, GLfloat*)) \
    X(void, GetVertexAttribPointerv, (GLuint, GLenum, void**)) \
    X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
    X(void, VertexAttrib4fv, (GLuint, const GLfloat*)) \
    X(void, Viewport, (GLint, GLint, GLsizei, GLsizei)) \
    X(void, Scissor, (GLint, GLint, GLsizei, GLsizei)) \
    X(void, BlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum)) \
    X(void, BlendEquationSeparate, (GLenum, GLenum)) \
    X(void, BlendColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, ColorMask, (GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, DepthMask, (GLboolean)) \
    X(void, DepthFunc, (GLenum)) \
    X(void, DepthRangef, (GLfloat, GLfloat)) \
    X(void, ClearDepthf, (GLfloat)) \
    X(void, ClearStencil, (GLint)) \
    X(void, StencilFuncSeparate, (GLenum, GLenum, GLint, GLuint)) \
    X(void, StencilOpSeparate, (GLenum, GLenum, GLenum, GLenum)) \
    X(void, StencilMaskSeparate, (GLenum, GLuint)) \
    X(void, CullFace, (GLenum)) \
    X(void, FrontFace, (GLenum)) \
    X(void, LineWidth, (GLfloat)) \
    X(void, PolygonOffset, (GLfloat, GLfloat)) \
    X(void, SampleCoverage, (GLfloat, GLboolean)) \
    X(void, PixelStorei, (GLenum, GLint)) \
    X(void, DrawArrays, (GLenum, GLint, GLsizei)) \
    X(void, Clear, (GLbitfield)) \
    X(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*))

struct GL {
#define DECLARE_GL(result, name, args) result (GL_APIENTRY *name) args = nullptr;
    SESSION_GL_FUNCTIONS(DECLARE_GL)
#undef DECLARE_GL
    void (GL_APIENTRY *BindVertexArray)(GLuint) = nullptr;
    bool packedDepthStencil = false;
    unsigned attributes = 0;
    bool load(std::string& error) {
#define LOAD_GL(result, name, args) \
        name = reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress("gl" #name)); \
        if (!name) { error = "GPU: falta gl" #name; return false; }
        SESSION_GL_FUNCTIONS(LOAD_GL)
#undef LOAD_GL
        const auto* extensions = reinterpret_cast<const char*>(GetString(GL_EXTENSIONS));
        const auto* version = reinterpret_cast<const char*>(GetString(GL_VERSION));
        auto extension = [extensions](const char* name) {
            if (!extensions) return false;
            const size_t length = std::strlen(name);
            for (const char* at = extensions; (at = std::strstr(at, name)); at += length)
                if ((at == extensions || at[-1] == ' ') && (!at[length] || at[length] == ' ')) return true;
            return false;
        };
        const bool es3 = version && std::strstr(version, "OpenGL ES 3.");
        if (es3 || extension("GL_OES_vertex_array_object")) {
            BindVertexArray = reinterpret_cast<decltype(BindVertexArray)>(
                SDL_GL_GetProcAddress(es3 ? "glBindVertexArray" : "glBindVertexArrayOES"));
            if (!BindVertexArray) { error = "GPU: no se puede preservar el VAO"; return false; }
        }
        packedDepthStencil = es3 || extension("GL_OES_packed_depth_stencil");
        GLint count = 0;
        GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &count);
        if (count < 3 || count > static_cast<GLint>(MaxAttributes)) {
            error = "GPU: numero de atributos GLES2 no compatible"; return false;
        }
        attributes = static_cast<unsigned>(count);
        return true;
    }
    bool okay(std::string& error, const char* operation) const {
        const auto result = GetError();
        if (result == GL_NO_ERROR) return true;
        char text[128];
        std::snprintf(text, sizeof(text), "GPU: %s (GL 0x%04x)", operation, unsigned(result));
        error = text;
        // Bound draining even on a broken/context-lost driver.
        for (unsigned i = 0; i < 16 && GetError() != GL_NO_ERROR; ++i) {}
        return false;
    }
};
#undef SESSION_GL_FUNCTIONS

constexpr GLenum capabilities[] = {GL_BLEND, GL_SCISSOR_TEST, GL_DEPTH_TEST, GL_STENCIL_TEST,
    GL_CULL_FACE, GL_POLYGON_OFFSET_FILL, GL_SAMPLE_ALPHA_TO_COVERAGE, GL_SAMPLE_COVERAGE, GL_DITHER};
struct State {
    GLint program{}, framebuffer{}, renderbuffer{}, arrayBuffer{}, elementBuffer{}, vertexArray{}, activeTexture{};
    GLint textures[SdlTextureUnits]{}, viewport[4]{}, scissor[4]{}, blend[6]{}, pack{}, unpack{};
    GLint depthFunc{}, cullFace{}, frontFace{}, clearStencil{}, stencil[2][7]{};
    GLfloat clearColor[4]{}, blendColor[4]{}, depthRange[2]{}, clearDepth{}, lineWidth{}, polygonFactor{}, polygonUnits{}, sampleCoverage{};
    GLboolean enabled[sizeof(capabilities)/sizeof(*capabilities)]{}, colorMask[4]{}, depthMask{}, sampleInvert{};
    struct Attribute { GLint enabled{}, size{}, type{}, normalized{}, stride{}, buffer{}; void* pointer{}; GLfloat current[4]{}; };
    std::array<Attribute, MaxAttributes> attribute{};
    static State neutral(unsigned width, unsigned height, GLuint fbo) {
        State result;
        result.framebuffer=static_cast<GLint>(fbo); result.activeTexture=GL_TEXTURE0;
        result.viewport[2]=result.scissor[2]=static_cast<GLint>(width);
        result.viewport[3]=result.scissor[3]=static_cast<GLint>(height);
        result.blend[0]=result.blend[2]=GL_ONE;
        result.blend[1]=result.blend[3]=GL_ZERO;
        result.blend[4]=result.blend[5]=GL_FUNC_ADD;
        result.pack=result.unpack=4; result.depthFunc=GL_LESS;
        result.cullFace=GL_BACK; result.frontFace=GL_CCW;
        result.depthRange[1]=result.clearDepth=result.lineWidth=result.sampleCoverage=1;
        result.depthMask=GL_TRUE;
        for (auto& mask : result.colorMask) mask=GL_TRUE;
        for (auto& face : result.stencil) {
            face[0]=GL_ALWAYS; face[2]=face[3]=-1;
            face[4]=face[5]=face[6]=GL_KEEP;
        }
        for (auto& a : result.attribute) { a.size=4; a.type=GL_FLOAT; a.current[3]=1; }
        // Dithering is enabled in a fresh GLES2 context; other capabilities are disabled.
        result.enabled[8]=GL_TRUE;
        return result;
    }
    void capture(GL& gl) {
        gl.GetIntegerv(GL_CURRENT_PROGRAM,&program); gl.GetIntegerv(GL_FRAMEBUFFER_BINDING,&framebuffer);
        gl.GetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer); gl.GetIntegerv(GL_ARRAY_BUFFER_BINDING,&arrayBuffer);
        gl.GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&elementBuffer); gl.GetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);
        if (gl.BindVertexArray) gl.GetIntegerv(VertexArrayBinding,&vertexArray);
        for (unsigned i = 0; i < SdlTextureUnits; ++i) {
            gl.ActiveTexture(GL_TEXTURE0+i); gl.GetIntegerv(GL_TEXTURE_BINDING_2D,&textures[i]);
        }
        gl.ActiveTexture(activeTexture);
        gl.GetIntegerv(GL_VIEWPORT,viewport); gl.GetIntegerv(GL_SCISSOR_BOX,scissor);
        const GLenum blendNames[]{GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA};
        for (unsigned i = 0; i < 6; ++i) gl.GetIntegerv(blendNames[i],&blend[i]);
        gl.GetIntegerv(GL_PACK_ALIGNMENT,&pack); gl.GetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);
        gl.GetIntegerv(GL_DEPTH_FUNC,&depthFunc); gl.GetIntegerv(GL_CULL_FACE_MODE,&cullFace);
        gl.GetIntegerv(GL_FRONT_FACE,&frontFace); gl.GetIntegerv(GL_STENCIL_CLEAR_VALUE,&clearStencil);
        const GLenum stencilNames[2][7] = {
            {GL_STENCIL_FUNC,GL_STENCIL_REF,GL_STENCIL_VALUE_MASK,GL_STENCIL_WRITEMASK,GL_STENCIL_FAIL,GL_STENCIL_PASS_DEPTH_FAIL,GL_STENCIL_PASS_DEPTH_PASS},
            {GL_STENCIL_BACK_FUNC,GL_STENCIL_BACK_REF,GL_STENCIL_BACK_VALUE_MASK,GL_STENCIL_BACK_WRITEMASK,GL_STENCIL_BACK_FAIL,GL_STENCIL_BACK_PASS_DEPTH_FAIL,GL_STENCIL_BACK_PASS_DEPTH_PASS}};
        for (unsigned face = 0; face < 2; ++face) for (unsigned i = 0; i < 7; ++i)
            gl.GetIntegerv(stencilNames[face][i],&stencil[face][i]);
        gl.GetFloatv(GL_COLOR_CLEAR_VALUE,clearColor); gl.GetFloatv(GL_BLEND_COLOR,blendColor);
        gl.GetFloatv(GL_DEPTH_RANGE,depthRange); gl.GetFloatv(GL_DEPTH_CLEAR_VALUE,&clearDepth);
        gl.GetFloatv(GL_LINE_WIDTH,&lineWidth); gl.GetFloatv(GL_POLYGON_OFFSET_FACTOR,&polygonFactor);
        gl.GetFloatv(GL_POLYGON_OFFSET_UNITS,&polygonUnits); gl.GetFloatv(GL_SAMPLE_COVERAGE_VALUE,&sampleCoverage);
        gl.GetBooleanv(GL_SAMPLE_COVERAGE_INVERT,&sampleInvert); gl.GetBooleanv(GL_COLOR_WRITEMASK,colorMask);
        gl.GetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
        for (unsigned i = 0; i < sizeof(capabilities)/sizeof(*capabilities); ++i) enabled[i] = gl.IsEnabled(capabilities[i]);
        for (unsigned i = 0; i < gl.attributes; ++i) {
            auto& a = attribute[i];
            gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&a.enabled);
            gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&a.size);
            gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&a.type);
            gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&a.normalized);
            gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&a.stride);
            gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&a.buffer);
            gl.GetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&a.pointer);
            gl.GetVertexAttribfv(i,GL_CURRENT_VERTEX_ATTRIB,a.current);
        }
    }
    void restore(GL& gl) const {
        gl.BindFramebuffer(GL_FRAMEBUFFER,framebuffer); gl.BindRenderbuffer(GL_RENDERBUFFER,renderbuffer);
        gl.UseProgram(program);
        if (gl.BindVertexArray) gl.BindVertexArray(vertexArray);
        for (unsigned i = 0; i < gl.attributes; ++i) {
            const auto& a = attribute[i];
            gl.BindBuffer(GL_ARRAY_BUFFER,a.buffer);
            gl.VertexAttribPointer(i,a.size,a.type,a.normalized,a.stride,a.pointer);
            gl.VertexAttrib4fv(i,a.current);
            if (a.enabled) gl.EnableVertexAttribArray(i); else gl.DisableVertexAttribArray(i);
        }
        gl.BindBuffer(GL_ARRAY_BUFFER,arrayBuffer); gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER,elementBuffer);
        for (unsigned i = 0; i < SdlTextureUnits; ++i) {
            gl.ActiveTexture(GL_TEXTURE0+i); gl.BindTexture(GL_TEXTURE_2D,textures[i]);
        }
        gl.ActiveTexture(activeTexture);
        gl.Viewport(viewport[0],viewport[1],viewport[2],viewport[3]); gl.Scissor(scissor[0],scissor[1],scissor[2],scissor[3]);
        gl.BlendFuncSeparate(blend[0],blend[1],blend[2],blend[3]); gl.BlendEquationSeparate(blend[4],blend[5]);
        gl.BlendColor(blendColor[0],blendColor[1],blendColor[2],blendColor[3]);
        gl.ColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);
        gl.ClearColor(clearColor[0],clearColor[1],clearColor[2],clearColor[3]);
        gl.DepthMask(depthMask); gl.DepthFunc(depthFunc); gl.DepthRangef(depthRange[0],depthRange[1]); gl.ClearDepthf(clearDepth);
        gl.ClearStencil(clearStencil);
        for (unsigned i = 0; i < 2; ++i) {
            const auto* s = stencil[i]; const GLenum face = i ? GL_BACK : GL_FRONT;
            gl.StencilFuncSeparate(face,s[0],s[1],s[2]); gl.StencilMaskSeparate(face,s[3]);
            gl.StencilOpSeparate(face,s[4],s[5],s[6]);
        }
        gl.CullFace(cullFace); gl.FrontFace(frontFace); gl.LineWidth(lineWidth);
        gl.PolygonOffset(polygonFactor,polygonUnits); gl.SampleCoverage(sampleCoverage,sampleInvert);
        gl.PixelStorei(GL_PACK_ALIGNMENT,pack); gl.PixelStorei(GL_UNPACK_ALIGNMENT,unpack);
        for (unsigned i = 0; i < sizeof(capabilities)/sizeof(*capabilities); ++i)
            if (enabled[i]) gl.Enable(capabilities[i]); else gl.Disable(capabilities[i]);
    }
};

struct StateGuard {
    GL& gl; State state; bool restored=false;
    explicit StateGuard(GL& value) : gl(value) { state.capture(gl); }
    void restore() { if (!restored) { state.restore(gl); restored=true; } }
    ~StateGuard() { restore(); }
};

bool compile(GL& gl, GLuint shader, const char* source, std::string& error) {
    if (!shader) { error = "GPU: no se pudo crear el shader"; return false; }
    gl.ShaderSource(shader,1,&source,nullptr); gl.CompileShader(shader);
    GLint compiled = 0; gl.GetShaderiv(shader,GL_COMPILE_STATUS,&compiled);
    if (compiled) return true;
    std::array<char,513> message{};
    gl.GetShaderInfoLog(shader,512,nullptr,message.data());
    error = "GPU: compilador GLSL no disponible o shader rechazado: " + std::string(message.data());
    return false;
}

bool probeShader(GL& gl, std::string& error) {
    GLboolean compiler = GL_FALSE;
    gl.GetBooleanv(GL_SHADER_COMPILER,&compiler);
    if (!compiler) { error = "GPU: GLES2 no anuncia compilador GLSL"; return false; }
    struct Objects {
        GL& gl; GLuint vertex=0,fragment=0,program=0,buffer=0;
        ~Objects() { gl.UseProgram(0); gl.BindBuffer(GL_ARRAY_BUFFER,0);
            if (buffer) gl.DeleteBuffers(1,&buffer);
            if (program) gl.DeleteProgram(program);
            if (vertex) gl.DeleteShader(vertex);
            if (fragment) gl.DeleteShader(fragment); }
    } objects{gl};
    objects.vertex = gl.CreateShader(GL_VERTEX_SHADER);
    objects.fragment = gl.CreateShader(GL_FRAGMENT_SHADER);
    if (!compile(gl,objects.vertex,"attribute vec2 position; void main(){gl_Position=vec4(position,0.0,1.0);}",error) ||
        !compile(gl,objects.fragment,"precision mediump float; void main(){gl_FragColor=vec4(0.25,0.5,0.75,1.0);}",error)) return false;
    objects.program = gl.CreateProgram();
    if (!objects.program) { error = "GPU: no se pudo crear el programa"; return false; }
    gl.AttachShader(objects.program,objects.vertex); gl.AttachShader(objects.program,objects.fragment);
    gl.BindAttribLocation(objects.program,0,"position"); gl.LinkProgram(objects.program);
    GLint linked=0; gl.GetProgramiv(objects.program,GL_LINK_STATUS,&linked);
    if (!linked) {
        std::array<char,513> message{}; gl.GetProgramInfoLog(objects.program,512,nullptr,message.data());
        error = "GPU: enlace GLSL fallido: " + std::string(message.data()); return false;
    }
    for (auto capability : capabilities) gl.Disable(capability);
    gl.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    gl.Viewport(0,0,4,4); gl.UseProgram(objects.program);
    gl.GenBuffers(1,&objects.buffer); gl.BindBuffer(GL_ARRAY_BUFFER,objects.buffer);
    const GLfloat vertices[]{-1,-1,3,-1,-1,3};
    gl.BufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    gl.EnableVertexAttribArray(0); gl.VertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,nullptr);
    gl.DrawArrays(GL_TRIANGLES,0,3);
    gl.PixelStorei(GL_PACK_ALIGNMENT,1);
    std::array<GLubyte,4> pixel{};
    gl.ReadPixels(1,1,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel.data()); // Capability check only, never in draw/begin/end.
    if (!gl.okay(error,"prueba shader/FBO")) return false;
    if (std::abs(int(pixel[0])-64)>2 || std::abs(int(pixel[1])-128)>2 || std::abs(int(pixel[2])-191)>2 || pixel[3]!=255) {
        error = "GPU: la prueba shader/FBO devolvio colores incorrectos"; return false;
    }
    return true;
}
}

struct GpuSession::Impl {
    SDL_Window* window; SDL_Renderer* renderer; SDL_GLContext context = nullptr;
    SDL_threadID thread = SDL_ThreadID();
    GL gl; State frontend, core;
    SDL_Texture* texture = nullptr;
    GLuint fbo=0,depthBuffer=0,stencilBuffer=0;
    unsigned width=0,height=0;
    bool depth=false,stencil=false,prepared=false,active=false,functions=false,coreStateValid=false;
    Impl(SDL_Window* w, SDL_Renderer* r) : window(w),renderer(r) {}
    bool onThread(std::string& error) const {
        if (SDL_ThreadID()==thread) return true;
        error="GPU: contexto usado fuera del hilo de video"; return false;
    }
    bool current(std::string& error) const {
        if (!onThread(error)) return false;
        if (context && SDL_GL_GetCurrentContext()==context && SDL_GL_GetCurrentWindow()==window) return true;
        error="GPU: el contexto GLES2 de SDL ya no esta activo"; return false;
    }
    void release() {
        if (fbo) gl.DeleteFramebuffers(1,&fbo);
        if (depthBuffer) gl.DeleteRenderbuffers(1,&depthBuffer);
        if (stencilBuffer && stencilBuffer!=depthBuffer) gl.DeleteRenderbuffers(1,&stencilBuffer);
        fbo=depthBuffer=stencilBuffer=0;
        if (texture) SDL_DestroyTexture(texture);
        texture=nullptr; width=height=0; prepared=false; coreStateValid=false;
    }
};

GpuSession::GpuSession(SDL_Window* window, SDL_Renderer* renderer) : impl_(new Impl(window,renderer)) {}
GpuSession::~GpuSession() { reset(); }
bool GpuSession::ready() const { return impl_->prepared; }
uintptr_t GpuSession::framebuffer() const { return impl_->prepared ? impl_->fbo : 0; }
HardwareVideoHost::Procedure GpuSession::procedure(const char* name) {
    if (!name || std::strlen(name)>128 || SDL_ThreadID()!=impl_->thread ||
        !impl_->window || SDL_GL_GetCurrentWindow()!=impl_->window) return nullptr;
    return reinterpret_cast<Procedure>(SDL_GL_GetProcAddress(name));
}

bool GpuSession::prepare(unsigned width, unsigned height, bool depth, bool stencil, std::string& error) {
    error.clear(); auto& s=*impl_;
    if (!s.onThread(error)) return false;
    if (!width || !height || width>MaxWidth || height>MaxHeight || width<4 || height<4) {
        error="GPU: framebuffer fuera del limite 4x4 a 640x480"; return false;
    }
    if (s.active) { error="GPU: no se puede cambiar framebuffer durante una llamada del nucleo"; return false; }
    SDL_RendererInfo info{};
    if (!s.window || !s.renderer || SDL_GetRendererInfo(s.renderer,&info)<0 || !info.name ||
        std::strcmp(info.name,"opengles2") || !(info.flags&SDL_RENDERER_ACCELERATED) ||
        !(info.flags&SDL_RENDERER_TARGETTEXTURE)) {
        error="GPU: se necesita SDL GLES2 acelerado con texturas target"; return false;
    }
    if (SDL_RenderFlush(s.renderer)<0) { error=SDL_GetError(); return false; }
    if (!s.context) s.context=SDL_GL_GetCurrentContext();
    if (!s.current(error)) return false;
    if (!s.functions) { if (!s.gl.load(error)) return false; s.functions=true; }
    if (s.prepared && s.width==width && s.height==height && s.depth==depth && s.stencil==stencil) {
        // prepare starts a new core lifetime, including reset/reload. Old GL
        // programs/textures may have been deleted by context_destroy; only
        // our host target can be reused, never that core's saved bindings.
        s.coreStateValid=false;
        return true;
    }
    if (!s.gl.okay(error,"estado previo de SDL")) return false;
    // Destroying an old SDL texture also invalidates SDL's binding cache. Do
    // this before taking the snapshot, so it cannot rebind a deleted name.
    s.release();
    StateGuard guard(s.gl);
    auto setup = [&]() {
        if (depth && stencil && !s.gl.packedDepthStencil) {
            error="GPU: falta soporte depth/stencil combinado"; return false;
        }
        s.texture=SDL_CreateTexture(s.renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height);
        if (!s.texture) { error="GPU: textura target: "+std::string(SDL_GetError()); return false; }
        if (SDL_SetTextureBlendMode(s.texture,SDL_BLENDMODE_NONE)<0 ||
            SDL_SetTextureScaleMode(s.texture,SDL_ScaleModeNearest)<0) { error=SDL_GetError(); return false; }
        s.gl.ActiveTexture(GL_TEXTURE0);
        float textureWidth=0,textureHeight=0;
        if (SDL_GL_BindTexture(s.texture,&textureWidth,&textureHeight)<0) { error=SDL_GetError(); return false; }
        GLint textureName=0; s.gl.GetIntegerv(GL_TEXTURE_BINDING_2D,&textureName);
        SDL_GL_UnbindTexture(s.texture);
        if (!textureName || textureWidth!=1.0f || textureHeight!=1.0f) {
            error="GPU: textura SDL no compatible con GLES2 normalizado"; return false;
        }
        s.gl.GenFramebuffers(1,&s.fbo); s.gl.BindFramebuffer(GL_FRAMEBUFFER,s.fbo);
        s.gl.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,textureName,0);
        if (depth) {
            s.gl.GenRenderbuffers(1,&s.depthBuffer); s.gl.BindRenderbuffer(GL_RENDERBUFFER,s.depthBuffer);
            s.gl.RenderbufferStorage(GL_RENDERBUFFER,stencil?PackedDepthStencil:GL_DEPTH_COMPONENT16,width,height);
            s.gl.FramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,s.depthBuffer);
        }
        if (stencil) {
            if (depth) s.stencilBuffer=s.depthBuffer;
            else { s.gl.GenRenderbuffers(1,&s.stencilBuffer); s.gl.BindRenderbuffer(GL_RENDERBUFFER,s.stencilBuffer);
                s.gl.RenderbufferStorage(GL_RENDERBUFFER,GL_STENCIL_INDEX8,width,height); }
            s.gl.FramebufferRenderbuffer(GL_FRAMEBUFFER,GL_STENCIL_ATTACHMENT,GL_RENDERBUFFER,s.stencilBuffer);
        }
        if (!s.fbo || s.gl.CheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {
            error="GPU: framebuffer GLES2 incompleto"; return false;
        }
        if (!s.gl.okay(error,"creacion del framebuffer") || !probeShader(s.gl,error)) return false;
        s.gl.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); s.gl.DepthMask(GL_TRUE);
        s.gl.ClearColor(0,0,0,1); s.gl.ClearDepthf(1);
        s.gl.Clear(GL_COLOR_BUFFER_BIT | (depth?GL_DEPTH_BUFFER_BIT:0) | (stencil?GL_STENCIL_BUFFER_BIT:0));
        if (!s.gl.okay(error,"limpieza del framebuffer")) return false;
        s.width=width; s.height=height; s.depth=depth; s.stencil=stencil; s.prepared=true;
        return true;
    };
    const bool prepared=setup();
    if (!prepared) s.release();
    guard.restore();
    std::string restoreError;
    if (!s.gl.okay(restoreError,"restaurar SDL tras preparar GPU")) {
        if (!error.empty()) error += "; ";
        error += restoreError;
        if (prepared) s.release();
        return false;
    }
    return prepared;
}

bool GpuSession::begin(std::string& error) {
    error.clear(); auto& s=*impl_;
    if (!s.prepared || s.active) { error="GPU: sesion no preparada o entrada duplicada"; return false; }
    if (!s.current(error)) return false;
    if (SDL_RenderFlush(s.renderer)<0) { error=SDL_GetError(); return false; }
    s.frontend.capture(s.gl);
    if (!s.gl.okay(error,"guardar estado SDL")) { s.frontend.restore(s.gl); return false; }
    s.active=true;
    // GLSM restores selected values, not a whole context. In particular SDL's
    // enabled blend/attributes and VBO bindings must not leak into the core.
    if (!s.coreStateValid) s.core=State::neutral(s.width,s.height,s.fbo);
    s.core.restore(s.gl);
    s.gl.BindFramebuffer(GL_FRAMEBUFFER,s.fbo);
    if (!s.gl.okay(error,"activar framebuffer")) { s.frontend.restore(s.gl); s.active=false; return false; }
    return true;
}
bool GpuSession::end(std::string& error) {
    error.clear(); auto& s=*impl_;
    if (!s.active) { error="GPU: no hay llamada del nucleo activa"; return false; }
    if (!s.current(error)) return false;
    bool coreOkay=s.gl.okay(error,"llamada del nucleo");
    s.core.capture(s.gl);
    std::string captureError;
    s.coreStateValid=s.gl.okay(captureError,"guardar estado del nucleo");
    if (!s.coreStateValid) {
        if (!error.empty()) error += "; ";
        error += captureError; coreOkay=false;
    }
    s.frontend.restore(s.gl); s.active=false;
    std::string restoreError;
    if (!s.gl.okay(restoreError,"restaurar estado SDL")) {
        if (!error.empty()) error += "; ";
        error += restoreError; return false;
    }
    return coreOkay;
}
bool GpuSession::draw(unsigned width, unsigned height, bool bottomLeftOrigin,
                       const SDL_Rect& destination, std::string& error) {
    error.clear(); auto& s=*impl_;
    if (!s.prepared || s.active || !width || !height || width>s.width || height>s.height ||
        destination.w<=0 || destination.h<=0) { error="GPU: cuadro o destino invalido"; return false; }
    if (!s.current(error)) return false;
    const SDL_Rect source{0,0,static_cast<int>(width),static_cast<int>(height)};
    if (SDL_RenderCopyEx(s.renderer,s.texture,&source,&destination,0,nullptr,
                        bottomLeftOrigin?SDL_FLIP_VERTICAL:SDL_FLIP_NONE)<0) { error=SDL_GetError(); return false; }
    return true;
}
bool GpuSession::draw(unsigned width, unsigned height, bool bottomLeftOrigin, std::string& error) {
    return draw(width,height,bottomLeftOrigin,SDL_Rect{240,0,1440,1080},error);
}
void GpuSession::reset() {
    auto& s=*impl_; std::string error;
    if (!s.functions || !s.current(error)) return;
    if (s.active) end(error);
    SDL_RenderFlush(s.renderer);
    s.release();
}
}
