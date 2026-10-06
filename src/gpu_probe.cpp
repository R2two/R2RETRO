#include "gpu_probe.h"
#include "startup.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

namespace r2n64 {
namespace {
constexpr size_t MaxLog = 8192;
void stage(GpuProbeResult& result, const char* name, const std::string& detail = {}) {
    const auto bounded = detail.substr(0, 768);
    startupLog(name, bounded.empty() ? nullptr : bounded.c_str());
    const std::string line = std::string(name) + (bounded.empty() ? "" : ": " + bounded) + "\n";
    if (result.log.size() < MaxLog) result.log.append(line, 0, MaxLog - result.log.size());
}
std::string boundedString(const GLubyte* value) {
    if (!value) return {};
    const char* text = reinterpret_cast<const char*>(value);
    size_t size = 0;
    while (size < 256 && text[size]) ++size;
    return std::string(text, size);
}

#define PROBE_GL_FUNCTIONS(X) \
    X(const GLubyte*, GetString, (GLenum)) \
    X(void, GetBooleanv, (GLenum, GLboolean*)) \
    X(GLenum, GetError, (void)) \
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
    X(void, UseProgram, (GLuint)) \
    X(void, DeleteProgram, (GLuint)) \
    X(void, GenBuffers, (GLsizei, GLuint*)) \
    X(void, BindBuffer, (GLenum, GLuint)) \
    X(void, BufferData, (GLenum, GLsizeiptr, const void*, GLenum)) \
    X(void, DeleteBuffers, (GLsizei, const GLuint*)) \
    X(void, EnableVertexAttribArray, (GLuint)) \
    X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
    X(void, DrawArrays, (GLenum, GLint, GLsizei)) \
    X(void, GenTextures, (GLsizei, GLuint*)) \
    X(void, BindTexture, (GLenum, GLuint)) \
    X(void, TexParameteri, (GLenum, GLenum, GLint)) \
    X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
    X(void, DeleteTextures, (GLsizei, const GLuint*)) \
    X(void, GenFramebuffers, (GLsizei, GLuint*)) \
    X(void, BindFramebuffer, (GLenum, GLuint)) \
    X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(GLenum, CheckFramebufferStatus, (GLenum)) \
    X(void, DeleteFramebuffers, (GLsizei, const GLuint*)) \
    X(void, Viewport, (GLint, GLint, GLsizei, GLsizei)) \
    X(void, Disable, (GLenum)) \
    X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, Clear, (GLbitfield)) \
    X(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)) \
    X(void, PixelStorei, (GLenum, GLint)) \
    X(void, Finish, (void))

struct GL {
#define DECLARE_GL(returnType, name, arguments) returnType (GL_APIENTRY *name) arguments = nullptr;
    PROBE_GL_FUNCTIONS(DECLARE_GL)
#undef DECLARE_GL
    bool load(GpuProbeResult& result) {
#define LOAD_GL(returnType, name, arguments) \
        name = reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress("gl" #name)); \
        if (!name) { result.status = "Falta la función GLES2 gl" #name; return false; }
        PROBE_GL_FUNCTIONS(LOAD_GL)
#undef LOAD_GL
        return true;
    }
};
#undef PROBE_GL_FUNCTIONS

class TemporaryContext {
public:
    explicit TemporaryContext(SDL_Window* window) : window_(window),
        previousWindow_(SDL_GL_GetCurrentWindow()), previous_(SDL_GL_GetCurrentContext()),
        previousSwapInterval_(previous_ ? SDL_GL_GetSwapInterval() : 0) {}
    ~TemporaryContext() { restore(); }
    bool create(GpuProbeResult& result) {
        const std::array<int, 5> desired{{2, 0, SDL_GL_CONTEXT_PROFILE_ES, 0, 0}};
        for (size_t i = 0; i < attributes_.size(); ++i) {
            if (SDL_GL_GetAttribute(attributes_[i], &values_[i]) < 0) {
                result.status = "No se pudo consultar la configuración del contexto";
                stage(result, "GPU probe attributes failed", SDL_GetError());
                return false;
            }
        }
        for (size_t i = 0; i < attributes_.size(); ++i) {
            if (SDL_GL_SetAttribute(attributes_[i], desired[i]) < 0) {
                result.status = "El puerto SDL no permite un contexto GLES2 aislado";
                stage(result, "GPU probe attributes rejected", SDL_GetError());
                return false;
            }
            ++changed_;
        }
        stage(result, "GPU probe create isolated context begin");
        contextAttempted_ = true;
        current_ = SDL_GL_CreateContext(window_);
        if (!current_) {
            result.status = "No se pudo crear un segundo contexto GLES2";
            stage(result, "GPU probe create isolated context failed", SDL_GetError());
            return false;
        }
        if (current_ == previous_) {
            // Some console ports only expose one context. Never alter/delete it.
            current_ = nullptr;
            result.status = "SDL reutilizó el contexto del menú; prueba no disponible";
            stage(result, "GPU probe context alias; skipped");
            return false;
        }
        result.contextCreated = true;
        if (SDL_GL_MakeCurrent(window_, current_) < 0) {
            result.status = "No se pudo activar el contexto de prueba";
            stage(result, "GPU probe make current failed", SDL_GetError());
            return false;
        }
        stage(result, "GPU probe isolated context active");
        return true;
    }
    bool restore() noexcept {
        if (restored_) return restoreOk_;
        restored_ = true;
        if (current_) { SDL_GL_DeleteContext(current_); current_ = nullptr; }
        restoreOk_ = SDL_GL_MakeCurrent(previous_ ? previousWindow_ : window_, previous_) == 0;
        // SDL's EGL backend resets its cached interval when creating a context,
        // even when the existing window/surface remains the same.
        if (restoreOk_ && previous_ && contextAttempted_ &&
            SDL_GL_GetSwapInterval() != previousSwapInterval_ &&
            SDL_GL_SetSwapInterval(previousSwapInterval_) < 0) restoreOk_ = false;
        for (size_t i = 0; i < changed_; ++i)
            if (SDL_GL_SetAttribute(attributes_[i], values_[i]) < 0) restoreOk_ = false;
        return restoreOk_;
    }
private:
    SDL_Window* window_;
    SDL_Window* previousWindow_;
    SDL_GLContext previous_;
    int previousSwapInterval_;
    SDL_GLContext current_ = nullptr;
    std::array<SDL_GLattr, 5> attributes_{{SDL_GL_CONTEXT_MAJOR_VERSION,
        SDL_GL_CONTEXT_MINOR_VERSION, SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_FLAGS, SDL_GL_SHARE_WITH_CURRENT_CONTEXT}};
    std::array<int, 5> values_{};
    size_t changed_ = 0;
    bool contextAttempted_ = false, restored_ = false, restoreOk_ = true;
};

struct Resources {
    GL& gl;
    GLuint vertex = 0, fragment = 0, program = 0, buffer = 0, texture = 0, framebuffer = 0;
    ~Resources() {
        gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
        gl.UseProgram(0);
        gl.BindBuffer(GL_ARRAY_BUFFER, 0);
        gl.BindTexture(GL_TEXTURE_2D, 0);
        if (framebuffer) gl.DeleteFramebuffers(1, &framebuffer);
        if (texture) gl.DeleteTextures(1, &texture);
        if (buffer) gl.DeleteBuffers(1, &buffer);
        if (program) gl.DeleteProgram(program);
        if (fragment) gl.DeleteShader(fragment);
        if (vertex) gl.DeleteShader(vertex);
    }
};

bool glOkay(GL& gl, GpuProbeResult& result, const char* operation) {
    const GLenum error = gl.GetError();
    if (error == GL_NO_ERROR) return true;
    char message[96];
    std::snprintf(message, sizeof(message), "%s: GL error 0x%04x", operation, unsigned(error));
    stage(result, "GPU probe GL failure", message);
    result.status = "La prueba GPU devolvió un error GLES2";
    return false;
}
bool compile(GL& gl, GLuint shader, const char* source, GpuProbeResult& result, const char* name) {
    if (!shader) { result.status = "No se pudo reservar un shader"; return false; }
    stage(result, name, "compile begin");
    gl.ShaderSource(shader, 1, &source, nullptr);
    gl.CompileShader(shader);
    GLint compiled = GL_FALSE;
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    std::array<char, 769> message{};
    GLsizei size = 0;
    gl.GetShaderInfoLog(shader, GLsizei(message.size() - 1), &size, message.data());
    message.back() = 0;
    stage(result, name, std::string(compiled ? "compile OK" : "compile failed") +
        (message[0] ? "; " + std::string(message.data()) : ""));
    if (!compiled) result.status = "El compilador rechazó el shader de prueba";
    return compiled == GL_TRUE && glOkay(gl, result, "compile shader");
}

void probe(GpuProbeResult& result) {
    GL gl;
    stage(result, "GPU probe resolve GLES2 functions");
    if (!gl.load(result)) return;
    result.supported = true;
    result.vendor = boundedString(gl.GetString(GL_VENDOR));
    result.renderer = boundedString(gl.GetString(GL_RENDERER));
    result.version = boundedString(gl.GetString(GL_VERSION));
    result.shadingLanguage = boundedString(gl.GetString(GL_SHADING_LANGUAGE_VERSION));
    stage(result, "GPU probe GL version", result.version);
    stage(result, "GPU probe vendor", result.vendor);
    stage(result, "GPU probe renderer", result.renderer);
    stage(result, "GPU probe shading language", result.shadingLanguage);
    stage(result, "GPU probe shader compiler query begin");
    GLboolean compiler = GL_FALSE;
    gl.GetBooleanv(GL_SHADER_COMPILER, &compiler);
    result.shaderCompiler = compiler == GL_TRUE;
    if (!glOkay(gl, result, "query shader compiler")) return;
    if (!result.shaderCompiler) {
        result.status = "Piglet/GLES2 no anuncia compilador de shaders GLSL";
        stage(result, "GPU probe shader compiler unavailable");
        return;
    }
    Resources objects{gl};
    const char* vertex =
        "attribute vec2 position;\n"
        "void main() { gl_Position = vec4(position, 0.0, 1.0); }\n";
    const char* fragment =
        "precision mediump float;\n"
        "void main() { gl_FragColor = vec4(gl_FragCoord.x < 4.0 ? 1.0 : 0.0,"
        " gl_FragCoord.y < 4.0 ? 1.0 : 0.0, 0.25, 1.0); }\n";
    objects.vertex = gl.CreateShader(GL_VERTEX_SHADER);
    result.vertexCompiled = compile(gl, objects.vertex, vertex, result, "GPU probe vertex shader");
    if (!result.vertexCompiled) return;
    objects.fragment = gl.CreateShader(GL_FRAGMENT_SHADER);
    result.fragmentCompiled = compile(gl, objects.fragment, fragment, result, "GPU probe fragment shader");
    if (!result.fragmentCompiled) return;
    stage(result, "GPU probe program link begin");
    objects.program = gl.CreateProgram();
    if (!objects.program) { result.status = "No se pudo reservar el programa GPU"; return; }
    gl.AttachShader(objects.program, objects.vertex);
    gl.AttachShader(objects.program, objects.fragment);
    gl.BindAttribLocation(objects.program, 0, "position");
    gl.LinkProgram(objects.program);
    GLint linked = GL_FALSE;
    gl.GetProgramiv(objects.program, GL_LINK_STATUS, &linked);
    std::array<char, 769> message{};
    GLsizei messageSize = 0;
    gl.GetProgramInfoLog(objects.program, GLsizei(message.size() - 1), &messageSize, message.data());
    message.back() = 0;
    stage(result, "GPU probe program link result", std::string(linked ? "OK" : "failed") +
        (message[0] ? "; " + std::string(message.data()) : ""));
    result.programLinked = linked == GL_TRUE;
    if (!result.programLinked) { result.status = "No se pudo enlazar el programa GPU"; return; }
    if (!glOkay(gl, result, "link program")) return;
    stage(result, "GPU probe framebuffer setup begin");
    gl.GenTextures(1, &objects.texture);
    gl.BindTexture(GL_TEXTURE_2D, objects.texture);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gl.GenFramebuffers(1, &objects.framebuffer);
    gl.BindFramebuffer(GL_FRAMEBUFFER, objects.framebuffer);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, objects.texture, 0);
    result.framebufferComplete = objects.texture && objects.framebuffer &&
        gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!result.framebufferComplete) { result.status = "Framebuffer de prueba incompleto"; return; }
    if (!glOkay(gl, result, "create framebuffer")) return;
    stage(result, "GPU probe draw known pattern begin");
    gl.Viewport(0, 0, 8, 8);
    for (const GLenum capability : {GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_SCISSOR_TEST, GL_CULL_FACE, GL_DITHER})
        gl.Disable(capability);
    gl.ClearColor(1, 0, 1, 1);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    gl.UseProgram(objects.program);
    const GLfloat vertices[]{-1,-1, 1,-1, -1,1, 1,1};
    gl.GenBuffers(1, &objects.buffer);
    if (!objects.buffer) { result.status = "No se pudo reservar el buffer de vértices"; return; }
    gl.BindBuffer(GL_ARRAY_BUFFER, objects.buffer);
    gl.BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    gl.EnableVertexAttribArray(0);
    gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    gl.DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (!glOkay(gl, result, "draw")) return;
    stage(result, "GPU probe pixel readback begin");
    std::array<GLubyte, 8 * 8 * 4> pixels{};
    gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl.Finish();
    gl.ReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    if (!glOkay(gl, result, "read pixels")) return;
    result.pixelMatched = true;
    for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 8; ++x) {
        const std::array<int, 4> expected{{x < 4 ? 255 : 0, y < 4 ? 255 : 0, 64, 255}};
        for (unsigned channel = 0; channel < 4; ++channel)
            if (std::abs(int(pixels[(y * 8 + x) * 4 + channel]) - expected[channel]) > 2)
                result.pixelMatched = false;
    }
    stage(result, "GPU probe pixel result", result.pixelMatched ? "64 RGBA pixels match" : "pattern mismatch");
    result.status = result.pixelMatched ? "Shaders GLSL y framebuffer verificados" : "La GPU devolvió píxeles incorrectos";
    result.passed = result.pixelMatched;
}
}

GpuProbeResult runGpuProbe(SDL_Window* window, SDL_Renderer* renderer) {
    GpuProbeResult result;
    result.contextRestored = true; // No context touched on unsupported paths.
    stage(result, "GPU probe manual begin");
    SDL_RendererInfo info{};
    if (!window || !renderer || SDL_GetRendererInfo(renderer, &info) < 0 ||
        !(SDL_GetWindowFlags(window) & SDL_WINDOW_OPENGL) ||
        !(info.flags & SDL_RENDERER_ACCELERATED)) {
        result.status = "La prueba necesita el renderer SDL OpenGL acelerado";
        stage(result, "GPU probe unavailable", result.status);
        return result;
    }
    TemporaryContext context(window);
    try {
        stage(result, "GPU probe flush SDL begin");
        if (SDL_RenderFlush(renderer) < 0) {
            result.status = "No se pudo finalizar el dibujo del menú";
            stage(result, "GPU probe flush failed", SDL_GetError());
        } else if (context.create(result)) probe(result);
    } catch (const std::exception& error) {
        result.passed = false;
        result.status = "No se pudo completar el diagnóstico GPU";
        stage(result, "GPU probe exception", error.what());
    } catch (...) {
        result.passed = false;
        result.status = "Error durante el diagnóstico GPU";
    }
    stage(result, "GPU probe restore SDL context begin");
    result.contextRestored = context.restore();
    if (!result.contextRestored) {
        result.passed = false;
        result.status = "No se pudo restaurar el contexto del menú";
        stage(result, "GPU probe restore failed", SDL_GetError());
    } else stage(result, "GPU probe SDL context restored");
    stage(result, "GPU probe complete", result.status);
    return result;
}
}
