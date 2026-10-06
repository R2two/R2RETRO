#include "renderer.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
using namespace pokemon3d;
void require(bool value, const std::string& reason) { if (!value) throw std::runtime_error(reason); }
void append32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<unsigned char>(value >> (i*8)));
}
std::vector<unsigned char> header(std::uint32_t count) {
    std::vector<unsigned char> bytes{'R','2','S','C','E','N','E','1'};
    append32(bytes, count);
    return bytes;
}
std::vector<unsigned char> encode(const Scene& scene) {
    auto bytes = header(std::uint32_t(scene.vertices.size()));
    for (const auto& v : scene.vertices) {
        for (float component : {v.x, v.y, v.z, v.r, v.g, v.b}) {
            std::uint32_t value;
            std::memcpy(&value, &component, 4);
            append32(bytes, value);
        }
    }
    return bytes;
}
std::vector<unsigned char> encodeTextured(const Scene& scene) {
    auto bytes=header(std::uint32_t(scene.vertices.size()));bytes[7]='2';
    append32(bytes,scene.textureWidth);append32(bytes,scene.textureHeight);
    for(const auto& v:scene.vertices) for(float f:{v.x,v.y,v.z,v.r,v.g,v.b,v.u,v.v}) {
        std::uint32_t value;std::memcpy(&value,&f,4);append32(bytes,value);
    }
    bytes.insert(bytes.end(),scene.textureRgba.begin(),scene.textureRgba.end());return bytes;
}
void write(const fs::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    require(bool(file), "Cannot create test fixture");
}
Scene triangle() {
    Scene scene;scene.vertices={{-1.f,-1.f,0.f,1.f,0.f,0.f},{1.f,-1.f,0.f,1.f,0.f,0.f},{0.f,1.f,0.f,1.f,0.f,0.f}};
    return scene;
}
void validationTests(const fs::path& directory) {
    const auto valid = encode(triangle());
    const auto path = directory / "fixture.scene";
    Scene loaded;
    std::string error;
    write(path, valid);
    require(loadScene(path.string(), loaded, error), "Valid scene rejected: " + error);
    require(loaded.vertices.size() == 3 && loaded.minimum[0] == -1.f && loaded.maximum[1] == 1.f,
            "Scene payload or bounds mismatch");
    auto reject = [&](const std::vector<unsigned char>& bytes, const std::string& name) {
        write(path, bytes);
        require(!loadScene(path.string(), loaded, error), name + " accepted");
        require(!error.empty() && loaded.vertices.empty(), name + " left stale scene or no error");
    };
    reject({}, "Empty file");
    reject(std::vector<unsigned char>(valid.begin(), valid.begin()+11), "Truncated header");
    auto bytes = valid; bytes[0] = 'X'; reject(bytes, "Invalid magic");
    bytes = valid; bytes.pop_back(); reject(bytes, "Truncated payload");
    bytes = valid; bytes.push_back(0); reject(bytes, "Trailing data");
    reject(header(0), "Empty geometry");
    reject(header(2), "Nontriangle vertex count");
    reject(header(std::uint32_t(MaxVertices+3)), "Resource limit");
    reject(header(std::numeric_limits<std::uint32_t>::max()), "Overflow vertex count");
    auto scene = triangle(); scene.vertices[0].x = std::numeric_limits<float>::quiet_NaN();
    reject(encode(scene), "NaN position");
    scene = triangle(); scene.vertices[0].r = std::numeric_limits<float>::infinity();
    reject(encode(scene), "Infinite color");
    scene = triangle(); scene.vertices[0].b = -0.001f; reject(encode(scene), "Negative color");
    scene = triangle(); scene.vertices[0].g = 1.001f; reject(encode(scene), "Excessive color");
    scene = triangle(); scene.vertices[0].z = 1000001.f; reject(encode(scene), "Unsafe coordinate");
    require(!loadScene((directory / "missing.scene").string(), loaded, error), "Missing scene accepted");
    scene=triangle();scene.textureWidth=2;scene.textureHeight=1;
    scene.textureRgba={255,0,0,255,0,0,255,255};scene.vertices[1].u=1.f;
    bytes=encodeTextured(scene);write(path,bytes);
    require(loadScene(path.string(),loaded,error),"Valid textured scene rejected: "+error);
    require(loaded.textureRgba==scene.textureRgba&&loaded.vertices[1].u==1.f,"Texture/UV payload mismatch");
    auto truncated=bytes;truncated.pop_back();reject(truncated,"Truncated atlas");
    truncated=bytes;truncated.push_back(0);reject(truncated,"Trailing atlas data");
    truncated=bytes;truncated.resize(18);reject(truncated,"Truncated texture dimensions");
    scene.textureWidth=0;reject(encodeTextured(scene),"Zero texture width");
    scene.textureWidth=2049;reject(encodeTextured(scene),"Excessive texture width");
    scene.textureWidth=2;scene.textureHeight=0;reject(encodeTextured(scene),"Zero texture height");
    scene.textureHeight=1;scene.vertices[0].u=-.1f;reject(encodeTextured(scene),"Negative UV");
    scene.vertices[0].u=std::numeric_limits<float>::quiet_NaN();reject(encodeTextured(scene),"NaN UV");
    scene.vertices[0].u=0.f;scene.vertices[0].v=1.1f;reject(encodeTextured(scene),"Excessive UV");
    write(path,valid);require(loadScene(path.string(),loaded,error),error);
    require(loaded.textureRgba.empty()&&loaded.vertices[0].u==0,"Legacy scene retained old atlas");
    fs::remove(path);
    std::cout << "PASS scene validation: valid LE float payload plus 15 malformed/missing cases\n";
}
void face(Scene& scene, std::array<float,3> a, std::array<float,3> b, std::array<float,3> c,
          std::array<float,3> d, std::array<float,3> color) {
    for (const auto& p : {a,b,c,a,c,d}) scene.vertices.push_back({p[0],p[1],p[2],color[0],color[1],color[2]});
}
Scene syntheticBuilding() {
    Scene scene;
    face(scene, {-2,0,-2},{2,0,-2},{2,0,2},{-2,0,2},{.28f,.45f,.32f});
    face(scene, {-1,0,1},{1,0,1},{1,1,1},{-1,1,1},{.62f,.28f,.12f});
    face(scene, {1,0,1},{1,0,-1},{1,1,-1},{1,1,1},{.38f,.16f,.07f});
    face(scene, {-1,0,-1},{-1,0,1},{-1,1,1},{-1,1,-1},{.45f,.23f,.11f});
    face(scene, {1,0,-1},{-1,0,-1},{-1,1,-1},{1,1,-1},{.5f,.3f,.2f});
    face(scene, {-1,1,-1},{-1,1,1},{1,1,1},{1,1,-1},{.84f,.53f,.23f});
    return scene;
}
void renderTests(const fs::path& directory) {
    Renderer renderer;
    std::string error;
    require(renderer.initialize(false,error), "GLES2 initialize: " + error);
    require(renderer.depthBits() >= 16, "No usable depth buffer");
    std::vector<std::uint8_t> earlyFrame;
    require(!renderer.readPixels(earlyFrame,error),"Unrendered frame capture accepted");
    std::vector<std::uint32_t> bootImage(160*144,0x0000FF00);
    require(renderer.drawGameFrame(bootImage.data(),160,144,false,error)&&renderer.readPixels(earlyFrame,error),
            "Original 2D frame cannot be captured before any 3D upload: "+error);
    require(earlyFrame[(480*Renderer::Width+640)*4+1]==255,"Early 2D image missing");
    Scene layers = triangle();
    for (auto& v : layers.vertices) v.z = .8f;
    auto back = triangle();
    for (auto& v : back.vertices) { v.r = 0.f; v.b = 1.f; }
    layers.vertices.insert(layers.vertices.end(), back.vertices.begin(), back.vertices.end());
    require(validateScene(layers,error), error);
    require(renderer.upload(layers,error) && renderer.draw({0.f,0.f,1.f},error), error);
    std::vector<std::uint8_t> first, second;
    require(renderer.readPixels(first,error), error);
    const auto center = (Renderer::Height/2*Renderer::Width+Renderer::Width/2)*4;
    require(first[center] > 245 && first[center+1] < 5 && first[center+2] < 5,
            "Front red triangle must occlude back blue triangle even when drawn first");
    require(renderer.savePng((directory/"depth-front-first.png").string(),error), error);
    std::rotate(layers.vertices.begin(), layers.vertices.begin()+3, layers.vertices.end());
    require(renderer.upload(layers,error) && renderer.draw({0.f,0.f,1.f},error), error);
    require(renderer.readPixels(second,error), error);
    require(first == second, "Depth output changed when primitive draw order reversed");
    require(renderer.savePng((directory/"depth-front-last.png").string(),error), error);
    auto building = syntheticBuilding();
    require(validateScene(building,error), error);
    write(directory/"original-building.scene",encode(building));
    require(renderer.upload(building,error) && renderer.draw({35.f,50.f,1.f},error),error);
    require(renderer.readPixels(first,error),error);
    require(renderer.savePng((directory/"original-building-35.png").string(),error),error);
    require(renderer.draw({145.f,35.f,1.f},error),error);
    require(renderer.readPixels(second,error),error);
    require(first != second, "Camera orbit did not change scene projection");
    std::size_t changed = 0;
    for (std::size_t i = 0; i < first.size(); i += 4)
        if (first[i] != second[i] || first[i+1] != second[i+1] || first[i+2] != second[i+2]) ++changed;
    require(changed > 10000, "Orbit has insufficient visible geometry");
    require(renderer.savePng((directory/"original-building-145.png").string(),error),error);
    require(!renderer.draw({std::numeric_limits<float>::infinity(),35.f,1.f},error), "Nonfinite camera accepted");
    require(!renderer.draw({0.f,35.f,0.f},error), "Zero zoom accepted");
    // Read back real textured output, checking channel order and orientation.
    std::vector<std::uint32_t> game(160*144,0x000000FF);
    std::fill(game.begin(),game.begin()+160*72,0x00FF0000);
    require(renderer.drawGameFrame(game.data(),160,144,false,error),error);
    std::vector<std::uint8_t> fallback;
    require(renderer.readPixels(fallback,error),error);
    auto at=[&](int x,int y,int channel){return fallback[(y*Renderer::Width+x)*4+channel];};
    require(at(640,100,0)==255 && at(640,100,2)==0 && at(640,800,2)==255 && at(640,800,0)==0,
            "Fallback image orientation or RGB channels incorrect");
    require(at(10,10,0)<20,"Fallback must clear old 3D image and letterbox");
    require(renderer.draw({145.f,35.f,1.f},error)&&renderer.readPixels(fallback,error),error);
    require(fallback==second,"2D fallback leaked GL state into map redraw");
    require(renderer.drawGameFrame(game.data(),160,144,true,error)&&renderer.readPixels(fallback,error),error);
    const auto insetPixel=(700*Renderer::Width+1000)*4;
    require(fallback[insetPixel]==255 && fallback[insetPixel+2]==0,"Inset image missing");
    require(std::equal(fallback.begin(),fallback.begin()+Renderer::Width*600*4,second.begin()),
            "Inset damaged pixels outside its rectangle");
    require(!renderer.drawGameFrame(nullptr,160,144,true,error),"Null game image accepted");
    require(!renderer.drawActors({Actor{0,1,128,0,0}},error),"Invalid actor position accepted");
    require(renderer.draw({35.f,50.f,1.f},error)&&renderer.drawActors({Actor{0,1,0,0,0}},error),error);
    require(renderer.readPixels(fallback,error),error);
    require(first!=fallback,"Live actor marker was not drawn");
    require(renderer.draw({35.f,50.f,1.f},error)&&renderer.drawActors({},error)&&renderer.readPixels(fallback,error),error);
    require(first==fallback,"Empty actor list retained old markers");
    require(renderer.drawGameOverlay(game.data(),160,144,error)&&renderer.readPixels(fallback,error),error);
    const auto panel=(300*Renderer::Width+900)*4;
    require(fallback[panel]==255&&fallback[panel+2]==0,"Large overlay does not show original image");
    for(int row=0;row<Renderer::Height;++row)
        require(std::equal(fallback.begin()+row*Renderer::Width*4,fallback.begin()+(row*Renderer::Width+700)*4,
                           first.begin()+row*Renderer::Width*4),"Overlay damaged world to its left");
    require(renderer.draw({35.f,50.f,1.f},error)&&renderer.readPixels(fallback,error)&&fallback==first,
            "Overlay leaked GL state");
    require(renderer.drawGameOverlay(game.data(),160,48,error)&&renderer.readPixels(fallback,error),error);
    require(std::equal(fallback.begin(),fallback.begin()+Renderer::Width*640*4,first.begin()),"Bottom dialog covers upper world");
    const auto dialogPixel=(800*Renderer::Width+640)*4;
    require(fallback[dialogPixel]==255&&fallback[dialogPixel+2]==0,"Cropped dialog was not placed at bottom");

    Scene textured;
    textured.vertices={{-2,-2,0,1,1,1,0,1},{2,-2,0,1,1,1,1,1},{2,2,0,1,1,1,1,0},
                       {-2,-2,0,1,1,1,0,1},{2,2,0,1,1,1,1,0},{-2,2,0,1,1,1,0,0}};
    textured.textureWidth=2;textured.textureHeight=2;
    textured.textureRgba={255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255};
    require(validateScene(textured,error)&&renderer.upload(textured,error)&&renderer.draw({0,0,1},error)&&
            renderer.readPixels(fallback,error),error);
    auto rgb=[&](int x,int y){const auto i=(y*Renderer::Width+x)*4;return std::array<unsigned,3>{fallback[i],fallback[i+1],fallback[i+2]};};
    require(rgb(550,390)==std::array<unsigned,3>{255,0,0}&&rgb(730,390)==std::array<unsigned,3>{0,255,0}&&
            rgb(550,570)==std::array<unsigned,3>{0,0,255}&&rgb(730,570)==std::array<unsigned,3>{255,255,0},
            "Scene atlas orientation, nearest sampling or RGB incorrect");
    require(renderer.upload(building,error)&&renderer.draw({35,50,1},error)&&renderer.readPixels(fallback,error)&&fallback==first,
            "Legacy upload retained scene atlas");

    Scene floor;
    face(floor,{-20,0,-20},{20,0,-20},{20,0,20},{-20,0,20},{.2f,.35f,.25f});
    require(validateScene(floor,error)&&renderer.upload(floor,error),error);
    Camera follow{0,50,1.25f};follow.follow=true;follow.targetX=0;follow.targetZ=0;
    SpriteAtlas atlas;atlas.width=96;atlas.height=16;atlas.rgba.resize(96*16*4);
    // First frame: opaque right half, red top and blue bottom; remaining frames transparent.
    for(unsigned y=0;y<16;++y) for(unsigned x=8;x<16;++x) {
        auto i=(y*96+x)*4;atlas.rgba[i+(y<8?0:2)]=255;atlas.rgba[i+3]=255;
    }
    require(renderer.uploadSprites(atlas,error),error);
    Actor actor;actor.picture=1;actor.worldX=0;actor.worldZ=0;
    require(renderer.draw(follow,error)&&renderer.drawActors({actor},error)&&renderer.readPixels(first,error),error);
    auto coloredCentroid=[&](const std::vector<std::uint8_t>& pixels,int channel) {
        std::array<double,3> out{};
        for(int y=0;y<Renderer::Height;++y) for(int x=0;x<Renderer::Width;++x) {
            auto i=(y*Renderer::Width+x)*4;
            if(pixels[i+channel]>245&&pixels[i+1]<5&&pixels[i+(channel==0?2:0)]<5) {out[0]+=x;out[1]+=y;++out[2];}
        }
        require(out[2]>30,"Expected opaque character pixels missing");out[0]/=out[2];out[1]/=out[2];return out;
    };
    auto red=coloredCentroid(first,0),blue=coloredCentroid(first,2);
    require(red[1]<blue[1],"Sprite atlas vertically inverted");
    std::size_t shadedContour=0;
    for(std::size_t i=0;i<first.size();i+=4)
        if(first[i]>=220&&first[i]<245&&first[i+1]==0&&first[i+2]==0) ++shadedContour;
    require(shadedContour>10,"Character edge lighting is missing or altered the palette channels");
    actor.flipX=true;
    require(renderer.draw(follow,error)&&renderer.drawActors({actor},error)&&renderer.readPixels(second,error),error);
    require(coloredCentroid(second,0)[0]<red[0]-5,"Sprite flip does not mirror horizontal pixels");
    actor.frame=1;
    require(renderer.draw(follow,error)&&renderer.drawActors({actor},error)&&renderer.readPixels(second,error),error);
    require(renderer.draw(follow,error)&&renderer.readPixels(fallback,error),error);
    for(int y=0;y<440;++y)
        require(std::equal(second.begin()+y*Renderer::Width*4,second.begin()+(y+1)*Renderer::Width*4,
                           fallback.begin()+y*Renderer::Width*4),"Transparent sprite wrote visible color/depth");
    actor.frame=0;actor.flipX=false;actor.worldX=6;follow.targetX=6;
    require(renderer.draw(follow,error)&&renderer.drawActors({actor},error)&&renderer.readPixels(second,error),error);
    auto moved=coloredCentroid(second,0);
    require(std::abs(moved[0]-red[0])<1&&std::abs(moved[1]-red[1])<1,"Follow camera did not keep player framed");
    follow.targetX=0;actor.worldX=0;
    face(floor,{-1,.6f,-1},{1,.6f,-1},{1,.6f,1},{-1,.6f,1},{.6f,.5f,.3f});
    require(validateScene(floor,error)&&renderer.upload(floor,error)&&renderer.draw(follow,error)&&
            renderer.drawActors({actor},error)&&renderer.readPixels(second,error),error);
    require(coloredCentroid(second,0)[1]<red[1]-10,"Character feet were not supported on low furniture");
    // Authoritative visible object ID 61 uses one cached original sphere mesh.
    Actor ball;ball.picture=61;ball.slot=2;ball.worldX=0;ball.worldZ=0;
    Camera objectView=follow;objectView.zoom=2.2f;objectView.cutaway=false;
    require(renderer.draw(objectView,error)&&renderer.readPixels(first,error),error);
    require(renderer.drawActors({ball},error)&&renderer.readPixels(second,error),error);
    std::size_t ballRed=0,ballWhite=0,ballBand=0;
    for(int y=250;y<700;++y) for(int x=400;x<880;++x) {
        const auto i=(y*Renderer::Width+x)*4;
        if(second[i]>130&&second[i+1]<45&&second[i+2]<55) ++ballRed;
        if(second[i]>145&&second[i+1]>145&&second[i+2]>145&&std::abs(int(second[i])-int(second[i+2]))<20) ++ballWhite;
        if(second[i]<32&&second[i+1]<32&&second[i+2]<40&&
           (second[i]!=first[i]||second[i+1]!=first[i+1]||second[i+2]!=first[i+2])) ++ballBand;
    }
    require(ballRed>150&&ballWhite>50&&ballBand>20,"Pokeball red/white shell or central band/button missing");
    require(renderer.savePng((directory/"pokeball-original-object.png").string(),error),error);
    require(renderer.draw(objectView,error)&&renderer.drawActors({},error)&&renderer.readPixels(fallback,error)&&fallback==first,
            "Hidden/removed object retained cached Pokeball geometry");
    auto objectOcclusion=floor;
    face(objectOcclusion,{-4,0,2},{4,0,2},{4,6,2},{-4,6,2},{.15f,.5f,.55f});
    require(validateScene(objectOcclusion,error)&&renderer.upload(objectOcclusion,error)&&renderer.draw(objectView,error)&&
            renderer.readPixels(first,error),error);
    require(renderer.drawActors({ball},error)&&renderer.readPixels(second,error)&&second==first,
            "Pokeball or its contact shadow ignored foreground depth");
    require(renderer.upload(floor,error)&&renderer.draw(objectView,error)&&renderer.drawActors({ball},error)&&
            renderer.readPixels(second,error),"Object cache did not survive scene transition: "+error);
    Scene occlusion;
    face(occlusion,{-20,0,-20},{20,0,-20},{20,0,20},{-20,0,20},{0,1,0});
    face(occlusion,{-5,0,3},{5,0,3},{5,5,3},{-5,5,3},{1,0,0});
    follow.cutaway=false;
    require(validateScene(occlusion,error)&&renderer.upload(occlusion,error)&&renderer.draw(follow,error)&&
            renderer.readPixels(first,error),error);
    follow.cutaway=true;
    require(renderer.draw(follow,error)&&renderer.readPixels(second,error),error);
    std::size_t cutPixels=0,retainedWall=0;
    for(std::size_t i=0;i<first.size();i+=4) {
        if(first[i+1]==255) require(first[i]==second[i]&&first[i+1]==second[i+1]&&first[i+2]==second[i+2],
                                  "Cutaway removed visible ground");
        if(first[i]==255&&first[i+1]==0) {if(second[i]!=255) ++cutPixels;else ++retainedWall;}
    }
    require(cutPixels>100&&retainedWall>cutPixels,"Cutaway did not remain local to player's view corridor");
    follow.targetX=std::numeric_limits<float>::infinity();require(!renderer.draw(follow,error),"Nonfinite follow target accepted");
    std::cout << "PASS depth order, foreground occlusion, orbit (" << changed << " changed pixels), camera validation; "
        << renderer.deviceInfo() << "; depth=" << renderer.depthBits() << "; live markers and 2D fallback PASS\n";
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("Usage: pokemon3d_tests --validation|--render EVIDENCE_DIR");
        const fs::path directory = argv[2];
        fs::create_directories(directory);
        if (std::string(argv[1]) == "--validation") validationTests(directory);
        else if (std::string(argv[1]) == "--render") renderTests(directory);
        else throw std::runtime_error("Unknown test mode");
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
