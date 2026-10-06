#include "red_state.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace pokemon3d;
namespace {
void check(bool test, const char* message) { if (!test) throw std::runtime_error(message); }
void set(WorkRam& ram,unsigned address,unsigned value) { ram[address-0xC000]=std::uint8_t(value); }
WorkRam fixture() {
    WorkRam ram{};
    const auto set=[&](unsigned a,unsigned v) { ram[a-0xC000]=std::uint8_t(v); };
    set(0xD369,10); set(0xD368,9); set(0xD36A,0xFD); set(0xD36B,0x42); set(0xD4E1,3);
    set(0xD362,5); set(0xD361,6); set(0xC100,1); set(0xC101,0);
    set(0xCFCB,1);
    set(0xD5CE,1);set(0xD5CF,0);set(0xD5D0,255);
    set(0xC110,3); set(0xC111,1); set(0xC214,9); set(0xC215,12);
    return ram;
}
WorkRam house(unsigned map) {
    auto ram=fixture();
    set(ram,0xD35E,map); set(ram,0xD369,4); set(ram,0xD368,4);
    set(ram,0xD367,map==37?1:4); set(ram,0xD36A,map==37?9:16); set(ram,0xD36B,map==37?0x42:0x40);
    set(ram,0xD4E1,map==37?1:0); set(ram,0xC110,51); set(ram,0xC214,8); set(ram,0xC215,9);
    set(ram,0xD5CE,255);
    return ram;
}
WorkRam lab() {
    auto ram=fixture();
    set(ram,0xD35E,40);set(ram,0xD369,5);set(ram,0xD368,6);set(ram,0xD367,5);
    set(ram,0xD36A,0xC0);set(ram,0xD36B,0x41);set(ram,0xD4E1,11);set(ram,0xD362,4);
    for (unsigned i=0;i<8;++i) {set(ram,0xD5CE + i*2,i+1);set(ram,0xD5CF + i*2,42+i);}
    set(ram,0xD5DE,255);
    constexpr unsigned pictures[]={2,61,61,61,3,65,65,3,13,32,32};
    for (unsigned slot=1;slot<=11;++slot) {
        const unsigned a=0xC100+slot*16,b=0xC200+slot*16;
        set(ram,a,pictures[slot-1]);set(ram,a+1,1);set(ram,a+2,255);
        set(ram,b+4,5+slot/10);set(ram,b+5,4+slot%10);
    }
    return ram;
}
void box(WorkRam& ram,unsigned x,unsigned y,unsigned width,unsigned height) {
    const auto tile=[&](unsigned xx,unsigned yy,unsigned value) { set(ram,0xC3A0+yy*20+xx,value); };
    for(unsigned yy=y;yy<y+height;++yy) for(unsigned xx=x;xx<x+width;++xx) tile(xx,yy,0x7F);
    tile(x,y,0x79); tile(x+width-1,y,0x7B); tile(x,y+height-1,0x7D); tile(x+width-1,y+height-1,0x7E);
    for(unsigned xx=x+1;xx<x+width-1;++xx) { tile(xx,y,0x7A);tile(xx,y+height-1,0x7A); }
    for(unsigned yy=y+1;yy<y+height-1;++yy) { tile(x,yy,0x7C);tile(x+width-1,yy,0x7C); }
}
}
int main(int argc,char** argv) {
    try {
        check(sha1({})=="da39a3ee5e6b4b0d3255bfef95601890afd80709", "SHA1 empty vector");
        check(sha1({'a','b','c'})=="a9993e364706816aba3e25717850c26c9cd0d89d", "SHA1 abc vector");
        check(sha1(std::vector<std::uint8_t>(1000000,'a'))=="34aa973cd4c4daa4f61eeb2bdbad27316534016f", "SHA1 long vector");
        check(!supportedRed(std::vector<std::uint8_t>(1048576)), "Unknown cartridge accepted");
        const auto ram=fixture();
        auto state=decodeRed(ram,true);
        LiveGate gate;
        check(!gate.update(state).pallet && gate.update(state).pallet, "Map entry must settle");
        check(!gate.update(decodeRed({},true)).pallet && !gate.update(state).pallet && gate.update(state).pallet,
              "Map re-entry must settle after fallback");
        check(state.pallet && state.supported && !state.overlay && state.x==5 && state.y==6 && state.actors.size()==2, "Valid copied map");
        check(state.actors[0].worldX==11 && state.actors[0].worldZ==13, "Player world centre");
        check(state.actors[1].x==8 && state.actors[1].y==5, "NPC border coordinate conversion");
        check(!decodeRed(ram,false).pallet && !decodeRed({},true).pallet, "Wrong revision or boot accepted");
        for (const auto pair : {std::pair<unsigned,unsigned>{0xD35E,37},{0xD369,9},{0xD36A,0},
                               {0xD362,20},{0xD361,18},{0xD057,1},{0xC100,0},{0xC102,255},{0xCFCB,255},{0xD700,1}}) {
            auto changed=ram; changed[pair.first-0xC000]=std::uint8_t(pair.second);
            const auto hidden=decodeRed(changed,true);
            check(!hidden.pallet && !hidden.supported && hidden.actors.empty(), "Stale map or unavailable scene not rejected");
        }
        for (unsigned map : {37u,38u}) {
            const auto inside=decodeRed(house(map),true);
            check(inside.supported && !inside.pallet && inside.map==map, "Supported interior rejected");
            check(inside.actors.size()==(map==37?2:1), "Interior object count");
            check(!gate.update(inside).supported && gate.update(inside).supported, "Supported map switch must settle");
            auto stale=house(map); set(stale,0xD369,10);
            check(!decodeRed(stale,true).supported, "Mixed transition header accepted");
        }
        const auto laboratory=decodeRed(lab(),true);
        check(laboratory.supported && laboratory.map==40 && laboratory.actors.size()==12,"Oak lab header and initialized objects");
        check(!gate.update(laboratory).supported && gate.update(laboratory).supported,"Oak map entry must settle");
        auto chosen=lab();set(chosen,0xD5AB,1u<<3); // flag43 = chosen first ball
        const auto afterChoice=decodeRed(chosen,true);
        check(afterChoice.actors.size()==11 && std::none_of(afterChoice.actors.begin(),afterChoice.actors.end(),
              [](const Actor& actor){return actor.slot==2;}),"Chosen ball must disappear even when sprite data remains initialized");
        set(chosen,0xD5AB,(1u<<3)|(1u<<6));set(chosen,0xD5AC,1u<<1); // Oak1/2 hidden by script
        const auto absentOaks=decodeRed(chosen,true);
        check(absentOaks.actors.size()==9 && std::none_of(absentOaks.actors.begin(),absentOaks.actors.end(),
              [](const Actor& actor){return actor.picture==3;}),"Hidden Oak instances must not be invented from ROM defaults");
        for(const auto pair : {std::pair<unsigned,unsigned>{0xD369,6},{0xD367,1},{0xD36A,0},{0xD4E1,10},
                              {0xD5CE,2},{0xD5CF,41},{0xD5DE,0}}) {
            auto invalidLab=lab();set(invalidLab,pair.first,pair.second);
            check(!decodeRed(invalidLab,true).supported,"Mixed lab header/object list must fall back");
        }
        auto menu=ram; set(menu,0xCFC4,1); set(menu,0xC102,255);
        const auto overlay=decodeRed(menu,true);
        check(overlay.supported && overlay.pallet && overlay.overlay && overlay.actors[0].picture==1,
              "Text overlay must retain the validated world and player");
        check(overlay.ui.width==0 && overlay.ui.height==0,"Unrecognised UI must use the full original panel");
        auto dialogue=menu; box(dialogue,0,12,20,6);
        const auto dialogueUi=decodeRed(dialogue,true).ui;
        check(dialogueUi.x==0 && dialogueUi.y==96 && dialogueUi.width==160 && dialogueUi.height==48,
              "Bottom dialogue window bounds");
        auto startMenu=menu;box(startMenu,12,0,8,16);
        const auto startUi=decodeRed(startMenu,true).ui;
        check(startUi.x==96 && startUi.y==0 && startUi.width==64 && startUi.height==128,"Start menu window bounds");
        box(dialogue,12,1,8,8);
        const auto multiUi=decodeRed(dialogue,true).ui;
        check(multiUi.x==0 && multiUi.y==8 && multiUi.width==160 && multiUi.height==136,
              "Multiple windows must preserve the union");
        for (const auto address : {0xC3A0+12*20+1,0xC3A0+13*20,0xC3A0+17*20+19}) {
            auto damaged=dialogue;set(damaged,unsigned(address),0);
            check(decodeRed(damaged,true).ui.width==0,"Partial UI border must fall back instead of cropping text");
        }
        auto dangling=menu;set(dangling,0xC3A0+17*20+19,0x79);
        check(decodeRed(dangling,true).ui.width==0,"Edge corner must not read beyond the tile map");
        set(dialogue,0xCFC4,0);
        check(!decodeRed(dialogue,true).overlay && decodeRed(dialogue,true).ui.width==0,
              "Stale text tiles must not activate an overlay");
        set(menu,0xD057,1);
        check(!decodeRed(menu,true).supported, "Battle must override text overlay");
        // Game counters, including the different player/NPC coordinate update
        // points, provide deterministic sub-cell motion in both directions.
        for (unsigned remaining=1; remaining<=8; ++remaining) {
            auto moving=ram; set(moving,0xCFC5,remaining); set(moving,0xC105,1);
            check(decodeRed(moving,true).actors[0].worldX==11+(8-remaining)*0.25f, "Player step interpolation");
            set(moving,0xC105,255);
            check(decodeRed(moving,true).actors[0].worldX==11-(8-remaining)*0.25f, "Player negative step interpolation");
        }
        auto moving=ram; set(moving,0xC111,3); set(moving,0xC210,8); set(moving,0xC115,1);
        check(decodeRed(moving,true).actors[1].worldX==16, "NPC destination must subtract remaining walk pixels");
        set(moving,0xC115,255); set(moving,0xC113,1);
        check(decodeRed(moving,true).actors[1].worldX==18 && decodeRed(moving,true).actors[1].worldZ==10,
              "Signed NPC walk vectors");
        for (unsigned image=0; image<16; ++image) {
            auto animated=ram; set(animated,0xC102,image);
            const auto actor=decodeRed(animated,true).actors[0];
            const unsigned facing=image/4,phase=image%4;
            check(actor.frame==(facing<2?facing:2)+(phase%2?3:0), "Original sprite animation lookup");
            check(actor.flipX==(facing==3 || (facing<2 && phase==3)), "Original sprite horizontal flip");
        }
        std::array<std::uint8_t,64> planes{};
        planes[0]=0x80; planes[1]=0x40; planes[16]=0x80; planes[17]=0x80;
        planes[32+14]=0x01; planes[48+15]=0x01;
        const auto decoded=spritePixels(planes);
        check(decoded[0]==1 && decoded[1]==2 && decoded[8]==3 && decoded[15*16+7]==1 && decoded[255]==2,
              "Planar bits and TL/TR/BL/BR quadrants");
        check(decoded[7]==0 && decoded[16]==0, "Planar transparent background");
        std::array<std::uint8_t,256> materialPixels{};materialPixels.fill(1);
        materialPixels[11*16+7]=2;materialPixels[7*16+7]=3;materialPixels[0]=0;
        const auto redColour=colourSprite(1,0,materialPixels),blueColour=colourSprite(2,0,materialPixels);
        const auto oakColour=colourSprite(3,0,materialPixels),motherColour=colourSprite(51,0,materialPixels);
        const auto shirt=(11*16+7)*4,face=(6*16+7)*4,eye=(7*16+7)*4;
        check(redColour[shirt]>redColour[shirt+2] && blueColour[shirt+2]>blueColour[shirt],"Red and Blue clothing identity");
        check(oakColour[10*16*4+7*4]>220 && motherColour[shirt]!=blueColour[shirt],"Oak coat and Mom clothing materials");
        check(redColour[face]>redColour[face+2] && redColour[face]!=redColour[shirt],"Skin must be separate from clothing");
        check(redColour[eye]<40 && redColour[eye+1]<40 && redColour[eye+2]<40,"Face details must retain dark ink");
        for(unsigned picture : {1u,2u,3u,51u,61u}) for(unsigned frame=0;frame<6;++frame) {
            const auto colours=colourSprite(picture,frame,materialPixels);
            check(colours[3]==0,"Original transparent pixel must remain transparent");
            for(std::size_t p=1;p<materialPixels.size();++p) check(colours[p*4+3]==255,"Visible source pixel was removed");
            if(frame<3) check(colours==colourSprite(picture,frame+3,materialPixels),"Material regions must match standing/walking directions");
        }
        SpriteAtlas invalid{1,1,{255}}; std::string error;
        check(!loadSpriteAtlas({},invalid,error) && invalid.rgba.empty() && invalid.width==0 && !error.empty(),
              "Unknown ROM must not leave a stale sprite atlas");
        auto hidden=ram; hidden[0xC112-0xC000]=255;
        check(decodeRed(hidden,true).actors.size()==2, "Enabled off-screen NPC must retain its initialized WRAM position");
        set(hidden,0xD5A6,1);
        check(decodeRed(hidden,true).actors.size()==1, "Script-hidden NPC retained");
        hidden=ram;set(hidden,0xC111,0x81);
        check(decodeRed(hidden,true).actors.size()==2,"Face-player flag must not hide an initialized NPC");
        hidden=ram;set(hidden,0xC111,0);
        check(decodeRed(hidden,true).actors.size()==1,"Uninitialized NPC must not use cartridge default position");
        hidden=ram; hidden[0xC215-0xC000]=2;
        check(decodeRed(hidden,true).actors.size()==1, "NPC coordinate underflow");
        check(decodeRed(ram,true).pallet && !decodeRed({},true).pallet && decodeRed(ram,true).pallet,
              "Reset and restore must re-decode without cached actors");
        check(std::isnan(Actor{}.worldX) && std::isnan(Actor{}.worldZ), "Legacy actor coordinate sentinel");
        // Deliberately opt-in; CTest never reads a commercial cartridge.
        if (argc!=1) {
            check(argc==3 && std::string(argv[1])=="--rom", "Usage: pokemon3d_red_state_tests [--rom FILE]");
            std::ifstream input(argv[2],std::ios::binary);
            check(bool(input), "Cannot read opt-in ROM");
            const std::vector<std::uint8_t> rom((std::istreambuf_iterator<char>(input)),{});
            SpriteAtlas atlas;
            check(loadSpriteAtlas(rom,atlas,error),error.c_str());
            check(atlas.width==96 && atlas.height==1152 && atlas.rgba.size()==96*1152*4, "Sprite atlas dimensions");
            unsigned transparent=0,visible=0;
            for (std::size_t i=3;i<atlas.rgba.size();i+=4) {
                check(atlas.rgba[i]==0 || atlas.rgba[i]==255, "Sprite alpha must preserve binary transparency");
                atlas.rgba[i]?++visible:++transparent;
            }
            check(transparent>0 && visible>0, "Atlas must contain sprite pixels and transparency");
            bool changed=false;
            for (unsigned y=0;y<16;++y) for (unsigned x=0;x<16;++x) for (unsigned c=0;c<4;++c)
                changed |= atlas.rgba[(y*96+x)*4+c]!=atlas.rgba[(y*96+48+x)*4+c];
            check(changed,"Player walking and standing frames must differ");
            for(unsigned picture=1;picture<=72;++picture) {
                const auto entry=0x17B27+(picture-1)*4;
                const std::size_t offset=std::size_t(rom[entry+3])*0x4000+(rom[entry]|unsigned(rom[entry+1])<<8)-0x4000;
                for(unsigned frame=0;frame<6;++frame) {
                    std::array<std::uint8_t,64> tiles{};
                    std::copy_n(rom.begin()+offset+(rom[entry+2]==192?frame*64:0),64,tiles.begin());
                    const auto source=spritePixels(tiles);
                    for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
                        const auto dest=(((picture-1)*16+y)*96+frame*16+x)*4;
                        check(atlas.rgba[dest+3]==(source[y*16+x]?255:0),"Recolouring changed a ROM sprite silhouette");
                    }
                }
            }
            std::cout << "PASS: opt-in exact ROM atlas, 72 sprites, six frames, alpha and walking image\n";
        }
        std::cout << "PASS: ROM identity, four maps, WRAM visibility flags, sub-cell movement, material colours, UI and reset\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
