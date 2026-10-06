#include "renderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_opengles2.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace pokemon3d {
namespace {
constexpr float Pi = 3.14159265358979323846f;
using Vec = std::array<float, 3>;
using Mat = std::array<float, 16>;
Vec subtract(Vec a, Vec b) { return {a[0]-b[0], a[1]-b[1], a[2]-b[2]}; }
float dot(Vec a, Vec b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
Vec cross(Vec a, Vec b) { return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]}; }
Vec normalized(Vec v) {
    const float length = std::sqrt(dot(v, v));
    return {v[0]/length, v[1]/length, v[2]/length};
}
Mat multiply(const Mat& a, const Mat& b) {
    Mat out{};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k) out[column*4+row] += a[k*4+row]*b[column*4+k];
    return out;
}
Mat lookAt(Vec eye, Vec target) {
    const auto forward = normalized(subtract(target, eye));
    const auto side = normalized(cross(forward, {0.f, 1.f, 0.f}));
    const auto up = cross(side, forward);
    return {side[0], up[0], -forward[0], 0.f,
            side[1], up[1], -forward[1], 0.f,
            side[2], up[2], -forward[2], 0.f,
            -dot(side, eye), -dot(up, eye), dot(forward, eye), 1.f};
}
Mat perspective(float near, float far) {
    const float f = 1.f/std::tan(50.f*Pi/360.f);
    return {f/(float(Renderer::Width)/Renderer::Height),0.f,0.f,0.f,
            0.f,f,0.f,0.f, 0.f,0.f,(far+near)/(near-far),-1.f,
            0.f,0.f,2.f*far*near/(near-far),0.f};
}
std::vector<Vertex> makeBallMesh() {
    // Original low-poly presentation for the verified SPRITE_POKE_BALL identity.
    // One immutable mesh serves every visible object; the game controls its lifetime.
    std::vector<Vertex> mesh;
    constexpr float radius=.46f;
    const std::array<float,13> latitude{{-90,-72,-54,-36,-18,-7,7,18,36,54,72,84,90}};
    const Vec light=normalized({-.45f,.8f,.45f});
    auto vertex=[&](float latitudeDegrees,float longitude,Vec color) {
        const float lat=latitudeDegrees*Pi/180.f;
        const Vec normal{std::cos(lat)*std::sin(longitude),std::sin(lat),std::cos(lat)*std::cos(longitude)};
        const float shade=.66f+.34f*std::max(0.f,dot(normal,light));
        return Vertex{radius*normal[0],radius*(normal[1]+1.f)+.025f,radius*normal[2],
                      color[0]*shade,color[1]*shade,color[2]*shade};
    };
    for(std::size_t ring=0;ring+1<latitude.size();++ring) {
        const float middle=(latitude[ring]+latitude[ring+1])*.5f;
        const Vec color=middle>7.f?Vec{1.f,.08f,.12f}:middle< -7.f?Vec{.97f,.98f,1.f}:Vec{.055f,.065f,.09f};
        for(unsigned segment=0;segment<16;++segment) {
            const float a=segment*2.f*Pi/16,b=(segment+1)*2.f*Pi/16;
            const auto p0=vertex(latitude[ring],a,color),p1=vertex(latitude[ring],b,color);
            const auto p2=vertex(latitude[ring+1],b,color),p3=vertex(latitude[ring+1],a,color);
            mesh.insert(mesh.end(),{p0,p1,p2,p0,p2,p3});
        }
    }
    auto button=[&](float size,float z,Vec color) {
        for(unsigned i=0;i<16;++i) {
            const float a=i*2.f*Pi/16,b=(i+1)*2.f*Pi/16;
            for(const auto& point:{Vec{0,radius+.025f,z},Vec{size*std::cos(a),radius+.025f+size*std::sin(a),z},
                                  Vec{size*std::cos(b),radius+.025f+size*std::sin(b),z}})
                mesh.push_back({point[0],point[1],point[2],color[0],color[1],color[2]});
        }
    };
    button(.15f,radius+.008f,{.04f,.05f,.075f});
    button(.104f,radius+.014f,{.92f,.95f,1.f});
    button(.056f,radius+.018f,{.72f,.81f,.9f});
    return mesh;
}

#define LAB_GL_FUNCTIONS(X) \
    X(const GLubyte*, GetString, (GLenum)) \
    X(void, GetIntegerv, (GLenum, GLint*)) \
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
    X(GLint, GetUniformLocation, (GLuint, const GLchar*)) \
    X(void, UniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*)) \
    X(void, Uniform1f, (GLint, GLfloat)) \
    X(void, Uniform2f, (GLint, GLfloat, GLfloat)) \
    X(void, Uniform3f, (GLint, GLfloat, GLfloat, GLfloat)) \
    X(void, GenBuffers, (GLsizei, GLuint*)) \
    X(void, BindBuffer, (GLenum, GLuint)) \
    X(void, BufferData, (GLenum, GLsizeiptr, const void*, GLenum)) \
    X(void, GenTextures, (GLsizei, GLuint*)) \
    X(void, BindTexture, (GLenum, GLuint)) \
    X(void, TexParameteri, (GLenum, GLenum, GLint)) \
    X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
    X(void, TexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)) \
    X(void, DeleteTextures, (GLsizei, const GLuint*)) \
    X(void, ActiveTexture, (GLenum)) \
    X(void, Uniform1i, (GLint, GLint)) \
    X(void, DeleteBuffers, (GLsizei, const GLuint*)) \
    X(void, EnableVertexAttribArray, (GLuint)) \
    X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
    X(void, DrawArrays, (GLenum, GLint, GLsizei)) \
    X(void, Viewport, (GLint, GLint, GLsizei, GLsizei)) \
    X(void, Enable, (GLenum)) \
    X(void, Disable, (GLenum)) \
    X(void, DepthFunc, (GLenum)) \
    X(void, DepthMask, (GLboolean)) \
    X(void, BlendFunc, (GLenum, GLenum)) \
    X(void, ClearDepthf, (GLfloat)) \
    X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, Clear, (GLbitfield)) \
    X(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)) \
    X(void, PixelStorei, (GLenum, GLint))
struct GL {
#define DECLARE_GL(type, name, arguments) type (GL_APIENTRY *name) arguments = nullptr;
    LAB_GL_FUNCTIONS(DECLARE_GL)
#undef DECLARE_GL
    bool load(std::string& error) {
#define LOAD_GL(type, name, arguments) \
        name = reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress("gl" #name)); \
        if (!name) { error = "Missing GLES2 function gl" #name; return false; }
        LAB_GL_FUNCTIONS(LOAD_GL)
#undef LOAD_GL
        return true;
    }
};
#undef LAB_GL_FUNCTIONS
}
struct Renderer::Impl {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    bool sdl = false;
    bool frameRendered = false;
    GL gl;
    GLuint program = 0, buffer = 0, actors = 0, frameProgram = 0, frameBuffer = 0, frameTexture = 0;
    GLuint sceneTexture = 0, spriteTexture = 0, whiteTexture = 0, shadowTexture = 0, ballBuffer = 0;
    GLsizei ballCount = 0;
    unsigned spriteWidth = 0, spriteHeight = 0;
    unsigned frameWidth = 0, frameHeight = 0;
    std::vector<std::uint8_t> frameRgba;
    Mat lastTransform{};
    GLint transform = -1, imageUniform = -1, opacity = -1, cutaway = -1;
    GLint cutawayTarget = -1, cutawayDirection = -1, frameImage = -1;
    GLint translation = -1, spriteMode = -1, spriteTexel = -1, alphaCutoff = -1;
    GLsizei count = 0;
    unsigned depth = 0;
    unsigned maximumTextureSize = 0;
    Vec center{};
    float radius = 1.f;
    Vec minimum{}, maximum{}, cameraRight{1.f,0.f,0.f};
    struct Support {
        Vec a,b,c;
        float minX,maxX,minZ,maxZ,denominator;
    };
    std::vector<Support> supports;
    std::string device;
    ~Impl() {
        if (context && window) {
            SDL_GL_MakeCurrent(window, context);
            if (buffer && gl.DeleteBuffers) gl.DeleteBuffers(1, &buffer);
            if (actors && gl.DeleteBuffers) gl.DeleteBuffers(1, &actors);
            if (frameBuffer && gl.DeleteBuffers) gl.DeleteBuffers(1, &frameBuffer);
            if (frameTexture && gl.DeleteTextures) gl.DeleteTextures(1, &frameTexture);
            if (sceneTexture && gl.DeleteTextures) gl.DeleteTextures(1, &sceneTexture);
            if (spriteTexture && gl.DeleteTextures) gl.DeleteTextures(1, &spriteTexture);
            if (whiteTexture && gl.DeleteTextures) gl.DeleteTextures(1, &whiteTexture);
            if (shadowTexture && gl.DeleteTextures) gl.DeleteTextures(1, &shadowTexture);
            if (ballBuffer && gl.DeleteBuffers) gl.DeleteBuffers(1,&ballBuffer);
            if (frameProgram && gl.DeleteProgram) gl.DeleteProgram(frameProgram);
            if (program && gl.DeleteProgram) gl.DeleteProgram(program);
            SDL_GL_DeleteContext(context);
        }
        if (window) SDL_DestroyWindow(window);
        if (sdl) SDL_Quit();
    }
    GLuint compile(GLenum kind, const char* source, std::string& error) {
        const GLuint shader = gl.CreateShader(kind);
        if (!shader) { error = "Cannot allocate GLES2 shader"; return 0; }
        gl.ShaderSource(shader, 1, &source, nullptr);
        gl.CompileShader(shader);
        GLint success = 0;
        gl.GetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            std::array<char, 4096> log{};
            gl.GetShaderInfoLog(shader, log.size(), nullptr, log.data());
            error = "Shader compilation failed: " + std::string(log.data());
            gl.DeleteShader(shader);
            return 0;
        }
        return shader;
    }
    void bindVertices(GLuint vbo) {
        gl.BindBuffer(GL_ARRAY_BUFFER,vbo);
        gl.EnableVertexAttribArray(0); gl.EnableVertexAttribArray(1); gl.EnableVertexAttribArray(2);
        gl.VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
        gl.VertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,r)));
        gl.VertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,u)));
    }
    void bindTexture(GLuint texture) {
        gl.ActiveTexture(GL_TEXTURE0); gl.BindTexture(GL_TEXTURE_2D,texture);
    }
    bool uploadTexture(GLuint& texture,unsigned width,unsigned height,const void* rgba,std::string& error) {
        if(!width||!height||width>maximumTextureSize||height>maximumTextureSize) {
            error="Texture exceeds active GLES2 device limit";return false;
        }
        if(!texture) gl.GenTextures(1,&texture);
        if(!texture) {error="Texture allocation failed";return false;}
        bindTexture(texture);
        gl.PixelStorei(GL_UNPACK_ALIGNMENT,1);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        gl.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
        if(gl.GetError()!=GL_NO_ERROR) {error="Texture upload failed";return false;}
        return true;
    }
    void drawDynamic(const std::vector<Vertex>& vertices,GLuint texture,float alpha,bool character=false) {
        if(vertices.empty()) return;
        if(!actors) gl.GenBuffers(1,&actors);
        gl.UseProgram(program);
        gl.UniformMatrix4fv(transform,1,GL_FALSE,lastTransform.data());
        gl.Uniform1i(imageUniform,0); gl.Uniform1f(opacity,alpha); gl.Uniform1f(cutaway,0.f);
        gl.Uniform3f(translation,0.f,0.f,0.f);gl.Uniform1f(spriteMode,character?1.f:0.f);
        gl.Uniform2f(spriteTexel,spriteWidth?1.f/spriteWidth:1.f,spriteHeight?1.f/spriteHeight:1.f);
        gl.Uniform1f(alphaCutoff,texture==shadowTexture?.001f:.5f);
        bindTexture(texture); bindVertices(actors);
        gl.BufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);
        gl.DrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()));
    }
    void drawBall(float x,float y,float z) {
        gl.UseProgram(program);gl.UniformMatrix4fv(transform,1,GL_FALSE,lastTransform.data());
        gl.Uniform1i(imageUniform,0);gl.Uniform1f(opacity,1.f);gl.Uniform1f(cutaway,0.f);
        gl.Uniform1f(spriteMode,0.f);gl.Uniform1f(alphaCutoff,.5f);
        gl.Uniform3f(translation,x,y,z);bindTexture(whiteTexture);bindVertices(ballBuffer);
        gl.DrawArrays(GL_TRIANGLES,0,ballCount);
    }
    float supportHeight(float x,float z) const {
        float height=0.f;
        for(const auto& t:supports) {
            if(x<t.minX||x>t.maxX||z<t.minZ||z>t.maxZ) continue;
            const float a=((t.b[2]-t.c[2])*(x-t.c[0])+(t.c[0]-t.b[0])*(z-t.c[2]))/t.denominator;
            const float b=((t.c[2]-t.a[2])*(x-t.c[0])+(t.a[0]-t.c[0])*(z-t.c[2]))/t.denominator;
            const float c=1.f-a-b;
            if(a>=-.001f&&b>=-.001f&&c>=-.001f) height=std::max(height,a*t.a[1]+b*t.b[1]+c*t.c[1]);
        }
        return height;
    }
};
Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}
Renderer::~Renderer() = default;
bool Renderer::initialize(bool visible, std::string& error, bool synchronize) {
    auto& p = *impl_;
    if (p.sdl) { error = "Renderer already initialized"; return false; }
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { error = SDL_GetError(); return false; }
    p.sdl = true;
    const std::array<std::pair<SDL_GLattr, int>, 8> attributes{{
        {SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES},
        {SDL_GL_CONTEXT_MAJOR_VERSION, 2}, {SDL_GL_CONTEXT_MINOR_VERSION, 0},
        {SDL_GL_DEPTH_SIZE, 24}, {SDL_GL_RED_SIZE, 8}, {SDL_GL_GREEN_SIZE, 8},
        {SDL_GL_BLUE_SIZE, 8}, {SDL_GL_DOUBLEBUFFER, 1}
    }};
    for (const auto& setting : attributes) {
        if (SDL_GL_SetAttribute(setting.first, setting.second) < 0) { error = SDL_GetError(); return false; }
    }
    p.window = SDL_CreateWindow("R2N64 - Pokemon Red 3D research lab",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Width, Height,
        SDL_WINDOW_OPENGL | (visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN));
    if (!p.window) { error = SDL_GetError(); return false; }
    p.context = SDL_GL_CreateContext(p.window);
    if (!p.context) { error = SDL_GetError(); return false; }
    if (!p.gl.load(error)) return false;
    SDL_GL_SetSwapInterval(visible&&synchronize ? 1 : 0);
    GLint depth = 0;
    p.gl.GetIntegerv(GL_DEPTH_BITS, &depth);
    if (depth < 16) { error = "A depth buffer of at least 16 bits is required"; return false; }
    p.depth = unsigned(depth);
    GLint textureLimit=0;p.gl.GetIntegerv(GL_MAX_TEXTURE_SIZE,&textureLimit);
    if(textureLimit<1) {error="GLES2 texture limit unavailable";return false;}
    p.maximumTextureSize=unsigned(textureLimit);
    const auto* version = p.gl.GetString(GL_VERSION);
    const auto* renderer = p.gl.GetString(GL_RENDERER);
    p.device = std::string(version ? reinterpret_cast<const char*>(version) : "unknown") + " | " +
        (renderer ? reinterpret_cast<const char*>(renderer) : "unknown");
    static constexpr char VertexShader[] =
        "#version 100\nattribute vec3 aPosition;\nattribute vec3 aColor;\n"
        "attribute vec2 aUv;uniform mat4 uTransform;uniform vec3 uTranslation;\nvarying mediump vec3 vColor;\n"
        "varying mediump vec2 vUv;varying mediump vec3 vWorld;\n"
        "void main(){vColor=aColor;vUv=aUv;vWorld=aPosition+uTranslation;gl_Position=uTransform*vec4(vWorld,1.0);}\n";
    static constexpr char FragmentShader[] =
        "#version 100\nprecision mediump float;\nvarying mediump vec3 vColor;\n"
        "varying mediump vec2 vUv;varying mediump vec3 vWorld;uniform sampler2D image;\n"
        "uniform float uOpacity;uniform float uCutaway;uniform vec3 uTarget;uniform vec3 uDirection;\n"
        "uniform float uSpriteMode;uniform vec2 uSpriteTexel;uniform float uAlphaCutoff;\n"
        "void main(){vec4 tex=texture2D(image,vUv);if(tex.a<uAlphaCutoff)discard;\n"
        // A local dither aperture retains depth ordering and avoids sorting transparent buildings.
        // Ground stays intact. Only elevated geometry on the player's view corridor is affected.
        "vec2 delta=vWorld.xz-uTarget.xz;float along=dot(delta,uDirection.xz);\n"
        "float side=abs(delta.x*uDirection.z-delta.y*uDirection.x);\n"
        "if(uCutaway>0.5&&vWorld.y>1.35&&along>0.35&&along<9.0&&side<1.45\n"
        "&&mod(floor(gl_FragCoord.x)+floor(gl_FragCoord.y),4.0)>0.5)discard;\n"
        // Subtle bevel stays within the original opaque pixel silhouette. No added border pixels.
        "float shade=1.0;if(uSpriteMode>0.5){vec2 cell=mod(floor(vUv/uSpriteTexel),16.0);\n"
        "float right=cell.x<14.5?texture2D(image,vUv+vec2(uSpriteTexel.x,0.0)).a:0.0;\n"
        "float below=cell.y<14.5?texture2D(image,vUv+vec2(0.0,uSpriteTexel.y)).a:0.0;\n"
        "shade-=0.08*(1.0-right)+0.04*(1.0-below);}\n"
        "gl_FragColor=vec4(tex.rgb*vColor*shade,tex.a*uOpacity);}\n";
    const GLuint vertex = p.compile(GL_VERTEX_SHADER, VertexShader, error);
    if (!vertex) return false;
    const GLuint fragment = p.compile(GL_FRAGMENT_SHADER, FragmentShader, error);
    if (!fragment) { p.gl.DeleteShader(vertex); return false; }
    p.program = p.gl.CreateProgram();
    p.gl.AttachShader(p.program, vertex);
    p.gl.AttachShader(p.program, fragment);
    p.gl.BindAttribLocation(p.program, 0, "aPosition");
    p.gl.BindAttribLocation(p.program, 1, "aColor");
    p.gl.BindAttribLocation(p.program, 2, "aUv");
    p.gl.LinkProgram(p.program);
    p.gl.DeleteShader(vertex);
    p.gl.DeleteShader(fragment);
    GLint linked = 0;
    p.gl.GetProgramiv(p.program, GL_LINK_STATUS, &linked);
    if (!linked) {
        std::array<char, 4096> log{};
        p.gl.GetProgramInfoLog(p.program, log.size(), nullptr, log.data());
        error = "Shader link failed: " + std::string(log.data()); return false;
    }
    p.transform = p.gl.GetUniformLocation(p.program, "uTransform");
    p.imageUniform=p.gl.GetUniformLocation(p.program,"image");
    p.opacity=p.gl.GetUniformLocation(p.program,"uOpacity");
    p.cutaway=p.gl.GetUniformLocation(p.program,"uCutaway");
    p.cutawayTarget=p.gl.GetUniformLocation(p.program,"uTarget");
    p.cutawayDirection=p.gl.GetUniformLocation(p.program,"uDirection");
    p.translation=p.gl.GetUniformLocation(p.program,"uTranslation");
    p.spriteMode=p.gl.GetUniformLocation(p.program,"uSpriteMode");
    p.spriteTexel=p.gl.GetUniformLocation(p.program,"uSpriteTexel");
    p.alphaCutoff=p.gl.GetUniformLocation(p.program,"uAlphaCutoff");
    if (p.transform < 0) { error = "Transform uniform unavailable"; return false; }
    p.gl.GenBuffers(1, &p.buffer);
    if (!p.buffer) { error = "Cannot allocate scene VBO"; return false; }
    const std::array<std::uint8_t,4> white{{255,255,255,255}};
    if(!p.uploadTexture(p.whiteTexture,1,1,white.data(),error)) return false;
    std::array<std::uint8_t,32*32*4> shadow{};
    for(unsigned y=0;y<32;++y) for(unsigned x=0;x<32;++x) {
        const float dx=(x+.5f-16.f)/16.f,dy=(y+.5f-16.f)/16.f;
        const float falloff=std::max(0.f,1.f-dx*dx-dy*dy);
        const auto i=(y*32+x)*4;
        shadow[i]=shadow[i+1]=shadow[i+2]=255;
        shadow[i+3]=std::uint8_t(200.f*falloff*falloff);
    }
    if(!p.uploadTexture(p.shadowTexture,32,32,shadow.data(),error)) return false;
    p.gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    p.gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    const auto ball=makeBallMesh();p.ballCount=GLsizei(ball.size());
    p.gl.GenBuffers(1,&p.ballBuffer);
    if(!p.ballBuffer) {error="Object mesh allocation failed";return false;}
    p.gl.BindBuffer(GL_ARRAY_BUFFER,p.ballBuffer);
    p.gl.BufferData(GL_ARRAY_BUFFER,ball.size()*sizeof(Vertex),ball.data(),GL_STATIC_DRAW);
    p.gl.Viewport(0, 0, Width, Height);
    p.gl.Enable(GL_DEPTH_TEST);
    p.gl.DepthFunc(GL_LESS);
    p.gl.DepthMask(GL_TRUE);
    p.gl.ClearDepthf(1.f);
    p.gl.Disable(GL_BLEND);
    p.gl.Disable(GL_CULL_FACE);
    p.gl.Disable(GL_DITHER);
    p.gl.ClearColor(0.035f, 0.043f, 0.057f, 1.f);
    if (p.gl.GetError() != GL_NO_ERROR) { error = "GLES2 initialization failed"; return false; }
    error.clear();
    return true;
}
bool Renderer::upload(const Scene& scene, std::string& error) {
    auto& p = *impl_;
    if (!p.buffer || scene.vertices.empty() || scene.vertices.size() > MaxVertices || scene.vertices.size() % 3) {
        error = "Renderer or scene is not ready"; return false;
    }
    Vec half{};
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(scene.minimum[i]) || !std::isfinite(scene.maximum[i]) || scene.minimum[i] > scene.maximum[i]) {
            error = "Invalid scene bounds"; return false;
        }
        p.minimum[i]=scene.minimum[i]; p.maximum[i]=scene.maximum[i];
        p.center[i] = (scene.minimum[i]+scene.maximum[i])*0.5f;
        half[i] = (scene.maximum[i]-scene.minimum[i])*0.5f;
    }
    p.radius = std::max(0.01f, std::sqrt(dot(half, half)));
    p.gl.BindBuffer(GL_ARRAY_BUFFER, p.buffer);
    static_assert(sizeof(Vertex) == 32, "Packed eight-float vertex required");
    p.gl.BufferData(GL_ARRAY_BUFFER, scene.vertices.size()*sizeof(Vertex), scene.vertices.data(), GL_STATIC_DRAW);
    if (p.gl.GetError() != GL_NO_ERROR) { error = "Scene VBO upload failed"; return false; }
    if(!scene.textureRgba.empty()) {
        if(!scene.textureWidth||!scene.textureHeight||scene.textureWidth>4096||scene.textureHeight>4096||
           scene.textureRgba.size()!=std::size_t(scene.textureWidth)*scene.textureHeight*4) {
            error="Invalid scene texture";return false;
        }
        if(!p.uploadTexture(p.sceneTexture,scene.textureWidth,scene.textureHeight,scene.textureRgba.data(),error)) return false;
    } else if(p.sceneTexture) {p.gl.DeleteTextures(1,&p.sceneTexture);p.sceneTexture=0;}
    // Cache only low supporting surfaces. This changes sprite presentation,
    // never the emulated game's authoritative position, collision or movement.
    p.supports.clear();
    for(std::size_t i=0;i<scene.vertices.size();i+=3) {
        const auto& a=scene.vertices[i];const auto& b=scene.vertices[i+1];const auto& c=scene.vertices[i+2];
        if(std::min({a.y,b.y,c.y})<0.f||std::max({a.y,b.y,c.y})>1.5f) continue;
        const float denominator=(b.z-c.z)*(a.x-c.x)+(c.x-b.x)*(a.z-c.z);
        if(std::abs(denominator)<.00001f) continue;
        p.supports.push_back({{a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z},
            std::min({a.x,b.x,c.x}),std::max({a.x,b.x,c.x}),std::min({a.z,b.z,c.z}),std::max({a.z,b.z,c.z}),denominator});
    }
    p.count = GLsizei(scene.vertices.size());
    error.clear();
    return true;
}
bool Renderer::uploadSprites(const SpriteAtlas& atlas,std::string& error) {
    auto& p=*impl_;
    if(!p.program||atlas.width!=96||!atlas.height||atlas.height%16||atlas.height>4096||
       atlas.rgba.size()!=std::size_t(atlas.width)*atlas.height*4) {
        error="Invalid character atlas";return false;
    }
    if(!p.uploadTexture(p.spriteTexture,atlas.width,atlas.height,atlas.rgba.data(),error)) return false;
    p.spriteWidth=atlas.width;p.spriteHeight=atlas.height;error.clear();return true;
}
bool Renderer::draw(const Camera& camera, std::string& error) {
    auto& p = *impl_;
    if (!p.count) { error = "No uploaded scene"; return false; }
    if (!std::isfinite(camera.yaw) || !std::isfinite(camera.pitch) || !std::isfinite(camera.zoom) || camera.zoom < 0.25f || camera.zoom > 4.f ||
        (camera.follow&&(!std::isfinite(camera.targetX)||!std::isfinite(camera.targetZ)||std::abs(camera.targetX)>100000||std::abs(camera.targetZ)>100000))) {
        error = "Invalid camera"; return false;
    }
    const float yaw = std::fmod(camera.yaw, 360.f)*Pi/180.f;
    const float pitch = std::clamp(camera.pitch, -85.f, 85.f)*Pi/180.f;
    Vec target=p.center;
    float distance=p.radius*3.f/camera.zoom;
    if(camera.follow) {
        const bool room=p.maximum[0]-p.minimum[0]<=20.f&&p.maximum[2]-p.minimum[2]<=20.f;
        // Small rooms retain their composition; the target follows within a safe central region.
        target={camera.targetX,.8f,camera.targetZ};
        if(room) {
            target[0]=std::clamp(target[0],p.center[0]-1.5f,p.center[0]+1.5f);
            target[2]=std::clamp(target[2],p.center[2]-1.5f,p.center[2]+1.5f);
        }
        distance=(room?27.f:26.f)/camera.zoom;
    }
    Vec eye{target[0]+distance*std::sin(yaw)*std::cos(pitch),
            target[1]+distance*std::sin(pitch),target[2]+distance*std::cos(yaw)*std::cos(pitch)};
    p.cameraRight={std::cos(yaw),0.f,-std::sin(yaw)};
    const auto matrix = multiply(perspective(std::max(.02f,p.radius*.005f),std::max(p.radius*20.f,80.f)),lookAt(eye,target));
    p.lastTransform=matrix;
    p.gl.Enable(GL_DEPTH_TEST);
    p.gl.Disable(GL_BLEND);p.gl.DepthMask(GL_TRUE);
    p.gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    p.gl.UseProgram(p.program);
    p.gl.UniformMatrix4fv(p.transform, 1, GL_FALSE, matrix.data());
    p.gl.Uniform1i(p.imageUniform,0);p.gl.Uniform1f(p.opacity,1.f);
    p.gl.Uniform3f(p.translation,0.f,0.f,0.f);p.gl.Uniform1f(p.spriteMode,0.f);p.gl.Uniform1f(p.alphaCutoff,.5f);
    p.gl.Uniform1f(p.cutaway,camera.follow&&camera.cutaway?1.f:0.f);
    p.gl.Uniform3f(p.cutawayTarget,camera.targetX,0.f,camera.targetZ);
    p.gl.Uniform3f(p.cutawayDirection,std::sin(yaw),0.f,std::cos(yaw));
    p.bindTexture(p.sceneTexture?p.sceneTexture:p.whiteTexture);
    p.bindVertices(p.buffer);
    p.gl.DrawArrays(GL_TRIANGLES, 0, p.count);
    if (p.gl.GetError() != GL_NO_ERROR) { error = "GLES2 draw failed"; return false; }
    p.frameRendered=true;
    error.clear();
    return true;
}
bool Renderer::drawActors(const std::vector<Actor>& actors, std::string& error) {
    auto& p=*impl_;
    if(!p.count || actors.size()>16) {error="Invalid actor draw";return false;}
    std::vector<Vertex> vertices,sprites,shadows;
    std::vector<Vec> balls;
    auto face=[&](Vec a,Vec b,Vec c,Vec d,Vec color) {
        for(auto v:{a,b,c,a,c,d}) vertices.push_back({v[0],v[1],v[2],color[0],color[1],color[2]});
    };
    for(const auto& actor:actors) {
        if(actor.x>=128 || actor.y>=128 || actor.slot>15 || actor.frame>=6) {error="Actor outside supported map";return false;}
        const float x=std::isnan(actor.worldX)?actor.x*2.f+1.f:actor.worldX;
        const float z=std::isnan(actor.worldZ)?actor.y*2.f+1.f:actor.worldZ;
        if(!std::isfinite(x)||!std::isfinite(z)||std::abs(x)>256.f||std::abs(z)>256.f) {error="Invalid actor position";return false;}
        const float r=.38f,y=1.6f;
        const float base=p.supportHeight(x,z);
        const bool isBall=actor.picture==61; // Verified SPRITE_POKE_BALL, no heuristic by color.
        // One small soft contact ellipse. The alpha texture is generated/uploaded once.
        const float shadowX=isBall?.43f:.62f,shadowZ=isBall?.3f:.34f;
        const Vertex sa{x-shadowX,base+.025f,z-shadowZ,.025f,.04f,.06f,0,0};
        const Vertex sb{x+shadowX,base+.025f,z-shadowZ,.025f,.04f,.06f,1,0};
        const Vertex sc{x+shadowX,base+.025f,z+shadowZ,.025f,.04f,.06f,1,1};
        const Vertex sd{x-shadowX,base+.025f,z+shadowZ,.025f,.04f,.06f,0,1};
        shadows.insert(shadows.end(),{sa,sb,sc,sa,sc,sd});
        if(isBall) {balls.push_back({x,base,z});continue;}
        if(p.spriteTexture&&actor.picture>0&&actor.picture<=p.spriteHeight/16) {
            const float u0=(actor.frame*16.f+.01f)/p.spriteWidth;
            const float u1=(actor.frame*16.f+15.99f)/p.spriteWidth;
            const float v0=((actor.picture-1)*16.f+.01f)/p.spriteHeight;
            const float v1=((actor.picture-1)*16.f+15.99f)/p.spriteHeight;
            const float left=actor.flipX?u1:u0,right=actor.flipX?u0:u1;
            const float dx=p.cameraRight[0],dz=p.cameraRight[2];
            const Vertex a{x-dx,base+.035f,z-dz,1,1,1,left,v1};
            const Vertex b{x+dx,base+.035f,z+dz,1,1,1,right,v1};
            const Vertex c{x+dx,base+2.035f,z+dz,1,1,1,right,v0};
            const Vertex d{x-dx,base+2.035f,z-dz,1,1,1,left,v0};
            sprites.insert(sprites.end(),{a,b,c,a,c,d});
            continue;
        }
        const Vec color=actor.slot==0?Vec{.12f,.45f,1.f}:Vec{1.f,.62f,.12f};
        const Vec dark{color[0]*.7f,color[1]*.7f,color[2]*.7f};
        face({x-r,0,z-r},{x-r,y,z-r},{x+r,y,z-r},{x+r,0,z-r},dark);
        face({x-r,0,z+r},{x-r,y,z+r},{x+r,y,z+r},{x+r,0,z+r},color);
        face({x-r,0,z-r},{x-r,y,z-r},{x-r,y,z+r},{x-r,0,z+r},dark);
        face({x+r,0,z-r},{x+r,y,z-r},{x+r,y,z+r},{x+r,0,z+r},color);
        face({x-r,y,z-r},{x-r,y,z+r},{x+r,y,z+r},{x+r,y,z-r},color);
        // White cap identifies the player independently of scene colors.
        if(actor.slot==0) face({x-.22f,y+.01f,z-.22f},{x-.22f,y+.01f,z+.22f},
                             {x+.22f,y+.01f,z+.22f},{x+.22f,y+.01f,z-.22f},{1,1,1});
    }
    p.gl.Enable(GL_DEPTH_TEST);p.gl.Enable(GL_BLEND);
    p.gl.BlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);p.gl.DepthMask(GL_FALSE);
    p.drawDynamic(shadows,p.shadowTexture,.42f);
    p.gl.Disable(GL_BLEND);p.gl.DepthMask(GL_TRUE);
    p.drawDynamic(vertices,p.whiteTexture,1.f);
    for(const auto& ball:balls) p.drawBall(ball[0],ball[1],ball[2]);
    p.drawDynamic(sprites,p.spriteTexture,1.f,true);
    if(p.gl.GetError()!=GL_NO_ERROR) {error="Actor draw failed";return false;}
    return true;
}
bool Renderer::drawGameFrame(const std::uint32_t* pixels,unsigned width,unsigned height,bool inset,std::string& error) {
    return drawImage(pixels,width,height,inset?1:0,error);
}
bool Renderer::drawGameOverlay(const std::uint32_t* pixels,unsigned width,unsigned height,std::string& error) {
    return drawImage(pixels,width,height,2,error);
}
bool Renderer::drawImage(const std::uint32_t* pixels,unsigned width,unsigned height,int mode,std::string& error) {
    auto& p=*impl_;
    if(!p.context||!p.program||!pixels||!width||!height||width>240||height>160) {error="Invalid portable image or renderer";return false;}
    if(!p.frameProgram) {
        const GLuint vs=p.compile(GL_VERTEX_SHADER,
            "#version 100\nattribute vec2 aPosition;attribute vec2 aUv;varying mediump vec2 uv;"
            "void main(){uv=aUv;gl_Position=vec4(aPosition,0.,1.);}",error);
        if(!vs) return false;
        const GLuint fs=p.compile(GL_FRAGMENT_SHADER,
            "#version 100\nprecision mediump float;varying mediump vec2 uv;uniform sampler2D image;"
            "void main(){gl_FragColor=texture2D(image,uv);}",error);
        if(!fs){p.gl.DeleteShader(vs);return false;}
        p.frameProgram=p.gl.CreateProgram();p.gl.AttachShader(p.frameProgram,vs);p.gl.AttachShader(p.frameProgram,fs);
        p.gl.BindAttribLocation(p.frameProgram,0,"aPosition");p.gl.BindAttribLocation(p.frameProgram,1,"aUv");
        p.gl.LinkProgram(p.frameProgram);p.gl.DeleteShader(vs);p.gl.DeleteShader(fs);
        GLint linked=0;p.gl.GetProgramiv(p.frameProgram,GL_LINK_STATUS,&linked);
        if(!linked){p.gl.DeleteProgram(p.frameProgram);p.frameProgram=0;error="Image shader link failed";return false;}
        p.frameImage=p.gl.GetUniformLocation(p.frameProgram,"image");
        p.gl.GenBuffers(1,&p.frameBuffer);p.gl.GenTextures(1,&p.frameTexture);
        if(!p.frameBuffer||!p.frameTexture){error="Image resources unavailable";return false;}
    }
    p.frameRgba.resize(width*height*4);
    for(std::size_t i=0;i<std::size_t(width)*height;++i) {
        p.frameRgba[i*4]=std::uint8_t(pixels[i]>>16);p.frameRgba[i*4+1]=std::uint8_t(pixels[i]>>8);
        p.frameRgba[i*4+2]=std::uint8_t(pixels[i]);p.frameRgba[i*4+3]=255;
    }
    p.gl.ActiveTexture(GL_TEXTURE0);p.gl.BindTexture(GL_TEXTURE_2D,p.frameTexture);
    if(width!=p.frameWidth||height!=p.frameHeight) {
        p.gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        p.gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        p.gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        p.gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        p.gl.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,p.frameRgba.data());
        p.frameWidth=width;p.frameHeight=height;
    } else p.gl.TexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,p.frameRgba.data());
    const bool dialog=mode==2&&width>=128&&height<=64;
    const float scale=mode==1?2.f:dialog?float(std::min({6,(Width-64)/int(width),(Height/3)/int(height)})):
        mode==2?float(std::min({5,(Width/2-32)/int(width),(Height-64)/int(height)})):
        float(std::min(Width/int(width),Height/int(height)));
    const float w=width*scale,h=height*scale;
    const float x=mode&&!dialog?Width-w-16:(Width-w)*.5f;
    const float y=dialog?Height-h-24:mode==1?Height-h-16:(Height-h)*.5f;
    const float left=x/Width*2-1,right=(x+w)/Width*2-1,top=1-y/Height*2,bottom=1-(y+h)/Height*2;
    const float quad[]={left,top,0,0,left,bottom,0,1,right,bottom,1,1,left,top,0,0,right,bottom,1,1,right,top,1,0};
    if(!mode) p.gl.Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    p.gl.Disable(GL_DEPTH_TEST);p.gl.Disable(GL_BLEND);p.gl.UseProgram(p.frameProgram);
    p.gl.Uniform1i(p.frameImage,0);
    p.gl.BindBuffer(GL_ARRAY_BUFFER,p.frameBuffer);p.gl.BufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STREAM_DRAW);
    p.gl.EnableVertexAttribArray(0);p.gl.EnableVertexAttribArray(1);
    p.gl.VertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);
    p.gl.VertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));
    p.gl.DrawArrays(GL_TRIANGLES,0,6);
    if(p.gl.GetError()!=GL_NO_ERROR) {error="Portable image draw failed";return false;}
    p.frameRendered=true;
    return true;
}
bool Renderer::readPixels(std::vector<std::uint8_t>& rgba, std::string& error) {
    auto& p = *impl_;
    if (!p.context||!p.frameRendered) { error = "No rendered frame"; return false; }
    rgba.resize(std::size_t(Width)*Height*4);
    p.gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
    p.gl.ReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    if (p.gl.GetError() != GL_NO_ERROR) { error = "GLES2 capture failed"; return false; }
    const std::size_t stride = std::size_t(Width)*4;
    for (int y = 0; y < Height/2; ++y)
        std::swap_ranges(rgba.begin()+y*stride, rgba.begin()+(y+1)*stride, rgba.begin()+(Height-1-y)*stride);
    error.clear();
    return true;
}
bool Renderer::savePng(const std::string& path, std::string& error) {
    std::vector<std::uint8_t> rgba;
    if (!readPixels(rgba, error)) return false;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(rgba.data(), Width, Height, 32, Width*4, SDL_PIXELFORMAT_RGBA32);
    if (!surface) { error = SDL_GetError(); return false; }
    const int result = IMG_SavePNG(surface, path.c_str());
    SDL_FreeSurface(surface);
    if (result != 0) { error = IMG_GetError(); return false; }
    return true;
}
void Renderer::present() { SDL_GL_SwapWindow(impl_->window); }
void Renderer::setPresentationEnabled(bool enabled) {
    if(impl_->window) SDL_SetWindowTitle(impl_->window,enabled?
        "R2N64 \xC2\xB7 Vista 3D activada \xC2\xB7 F1: cambiar":
        "R2N64 \xC2\xB7 Vista 3D desactivada \xC2\xB7 F1: cambiar");
}
unsigned Renderer::depthBits() const { return impl_->depth; }
const std::string& Renderer::deviceInfo() const { return impl_->device; }
}
