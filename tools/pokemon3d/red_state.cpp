#include "red_state.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
namespace pokemon3d {
namespace {
std::uint32_t rotate(std::uint32_t value, unsigned bits) { return (value << bits) | (value >> (32-bits)); }
using Colour = std::array<std::uint8_t,3>;
struct Materials {
    Colour hair, hairLight, clothing, clothingLight, trousers;
    bool coat = false, dress = false, creature = false;
};
Colour shadeColour(Colour colour,unsigned numerator,unsigned denominator) {
    for (auto& channel : colour) channel=std::uint8_t(unsigned(channel)*numerator/denominator);
    return colour;
}
Materials materials(unsigned picture) {
    // These are our own colours, not palettes extracted from another mod.
    // Named characters retain identifiable outfits in front/back/side views.
    if (picture==1) return {{66,43,37},{114,72,48},{202,48,45},{248,235,204},{43,83,129}};
    if (picture==2) return {{92,55,30},{171,109,46},{39,91,163},{126,178,229},{47,67,106}};
    if (picture==3) return {{140,142,133},{228,226,208},{197,212,211},{246,244,224},{85,83,66},true};
    if (picture==51) return {{70,40,31},{119,75,46},{163,77,137},{235,206,222},{115,49,94},false,true};
    if (picture==31 || picture==32 || picture==41)
        return {{59,52,50},{109,96,79},{185,208,207},{242,244,228},{54,77,98},true};
    if (picture==5 || picture==9 || picture==56 || picture==60 || picture==67)
        return {{48,71,69},{92,128,111},{74,130,106},{166,205,159},{51,83,81},false,false,true};
    if (picture==6 || picture==8 || picture==13 || picture==15 || picture==17 || picture==28 || picture==29)
        return {{74,48,36},{128,83,51},{171,79,79},{243,178,142},{104,50,58},false,true};
    if (picture==4 || picture==7 || picture==53 || picture==54 || picture==55)
        return {{69,47,35},{133,88,42},{199,158,63},{245,217,127},{55,98,129}};
    return {{62,49,39},{115,87,59},{79,125,123},{163,195,171},{63,73,89}};
}
UiRect findTextWindows(const WorkRam& ram) {
    // wTileMap is the game's 20x18 backing tile map. Recognise only complete
    // TextBoxBorder rectangles; do not guess bounds from text or framebuffer
    // colours. Missing or partly overwritten borders need full-frame fallback.
    const auto tile=[&](unsigned x,unsigned y) { return ram[0x3A0+y*20+x]; };
    unsigned left=20,top=18,right=0,bottom=0;
    bool any=false;
    for (unsigned y=0;y<18;++y) for (unsigned x=0;x<20;++x) {
        if (tile(x,y)!=0x79) continue;
        bool found=false;
        for (unsigned r=x+2;r<20 && !found;++r) {
            if (tile(r,y)!=0x7B) continue;
            bool topEdge=true;
            for (unsigned xx=x+1;xx<r;++xx) topEdge &= tile(xx,y)==0x7A;
            if (!topEdge) continue;
            for (unsigned b=y+2;b<18 && !found;++b) {
                if (tile(x,b)!=0x7D || tile(r,b)!=0x7E) continue;
                bool border=true;
                for (unsigned xx=x+1;xx<r;++xx) border &= tile(xx,b)==0x7A;
                for (unsigned yy=y+1;yy<b;++yy) border &= tile(x,yy)==0x7C && tile(r,yy)==0x7C;
                if (!border) continue;
                left=std::min(left,x); top=std::min(top,y);
                right=std::max(right,r+1); bottom=std::max(bottom,b+1);
                any=true; found=true;
            }
        }
        if (!found) return {};
    }
    return any ? UiRect{left*8,top*8,(right-left)*8,(bottom-top)*8} : UiRect{};
}
}
std::string sha1(const std::vector<std::uint8_t>& bytes) {
    auto data = bytes;
    const std::uint64_t bits = std::uint64_t(data.size()) * 8;
    data.push_back(0x80);
    while (data.size() % 64 != 56) data.push_back(0);
    for (int i = 7; i >= 0; --i) data.push_back(std::uint8_t(bits >> (i*8)));
    std::array<std::uint32_t, 5> hash{{0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0}};
    for (std::size_t block = 0; block < data.size(); block += 64) {
        std::array<std::uint32_t, 80> words{};
        for (unsigned i = 0; i < 16; ++i)
            for (unsigned j = 0; j < 4; ++j) words[i] = (words[i] << 8) | data[block+i*4+j];
        for (unsigned i = 16; i < 80; ++i) words[i] = rotate(words[i-3]^words[i-8]^words[i-14]^words[i-16], 1);
        auto a=hash[0], b=hash[1], c=hash[2], d=hash[3], e=hash[4];
        for (unsigned i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20) { f=(b&c)|(~b&d); k=0x5A827999; }
            else if (i < 40) { f=b^c^d; k=0x6ED9EBA1; }
            else if (i < 60) { f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDC; }
            else { f=b^c^d; k=0xCA62C1D6; }
            const auto next=rotate(a,5)+f+e+k+words[i];
            e=d; d=c; c=rotate(b,30); b=a; a=next;
        }
        hash[0]+=a; hash[1]+=b; hash[2]+=c; hash[3]+=d; hash[4]+=e;
    }
    std::ostringstream out;
    for (auto value : hash) out << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}
bool supportedRed(const std::vector<std::uint8_t>& rom) {
    return rom.size() == 1048576 && sha1(rom) == "ea9bcae617fdf159b045185467ae58b2e4a48b9a";
}
std::array<std::uint8_t, 256> spritePixels(const std::array<std::uint8_t, 64>& tiles) {
    std::array<std::uint8_t, 256> pixels{};
    for (unsigned tile=0; tile<4; ++tile)
        for (unsigned y=0; y<8; ++y)
            for (unsigned x=0; x<8; ++x) {
                const unsigned low=tiles[tile*16+y*2], high=tiles[tile*16+y*2+1];
                pixels[(tile/2*8+y)*16+tile%2*8+x]=std::uint8_t(((low>>(7-x))&1)|(((high>>(7-x))&1)<<1));
            }
    return pixels;
}
std::array<std::uint8_t, 1024> colourSprite(unsigned picture,unsigned frame,
    const std::array<std::uint8_t,256>& pixels) {
    std::array<std::uint8_t,1024> rgba{};
    if (!picture || picture>72 || frame>=6) return rgba;
    const Materials palette=materials(picture);
    const Colour ink{{24,25,31}},skin{{248,196,148}},skinShade{{192,132,94}};
    const unsigned direction=frame%3;
    const bool back=direction==1,side=direction==2;
    for (unsigned y=0;y<16;++y) for (unsigned x=0;x<16;++x) {
        const unsigned index=pixels[y*16+x];
        if (!index || index>3) continue;
        const bool silhouette=x==0 || y==0 || x==15 || y==15 ||
            !pixels[y*16+x-1] || !pixels[y*16+x+1] || !pixels[(y-1)*16+x] || !pixels[(y+1)*16+x];
        Colour light=palette.clothingLight,middle=palette.clothing,dark=shadeColour(middle,3,5);
        bool preserveInk=false;
        if (picture>=61) {
            // Static objects are not people. Preserve their patterns while
            // giving the ball/Pokedex a recognisable red and ivory material.
            light={{243,238,215}}; middle=picture==61||picture==65 ? Colour{{211,64,55}} : Colour{{157,145,113}};
            if (picture==61 && y>=8) middle={{180,191,196}};
            dark=ink;
        } else if (palette.creature) {
            dark=shadeColour(palette.clothing,2,5);
        } else {
            bool hair=false,face=false,hat=false;
            if (picture==1) {
                hat=y<=5;
                hair=back && y>=6 && y<=8;
                face=!back && y>=6 && y<=9;
            } else if (picture==3 || palette.coat) {
                hair=y<=3 || (back && y<=8 && x>=4 && x<=11) || (side && y<=7 && x>=8);
                face=!hair && y<=9;
            } else if (picture==51) {
                hair=y<=4 || (back && y<=10) || (!side && y<=10 && (x<=3 || x>=12)) ||
                    (side && y<=10 && x>=9);
                face=!hair && y<=9;
            } else {
                hair=y<=4 || (back && y<=8 && x>=4 && x<=11) || (side && y<=7 && x>=9);
                face=!hair && y<=9;
            }
            if (hat) {light=palette.clothingLight;middle=palette.clothing;dark=shadeColour(middle,3,5);}
            else if (hair) {light=palette.hairLight;middle=palette.hair;dark=shadeColour(middle,3,4);}
            else if (face) {light=skin;middle=skinShade;dark=ink;preserveInk=true;}
            else if (!palette.dress && y>=13) {
                light=palette.coat?palette.clothingLight:shadeColour(palette.trousers,6,5);
                middle=palette.trousers;dark=shadeColour(middle,3,5);
            }
            // Hands retain skin beside the garment; their location follows
            // the side/front pose. Back-facing central pixels stay clothing.
            const bool hand=y>=10 && y<=12 && ((!side && (x<=3 || x>=12)) || (side && x>=7 && x<=9));
            if (hand) {light=skin;middle=skinShade;dark=ink;preserveInk=true;}
        }
        const Colour colour=index==1?light:index==2?middle:(silhouette||preserveInk?ink:dark);
        const std::size_t destination=(y*16+x)*4;
        for (unsigned channel=0;channel<3;++channel) rgba[destination+channel]=colour[channel];
        rgba[destination+3]=255;
    }
    return rgba;
}
bool loadSpriteAtlas(const std::vector<std::uint8_t>& rom, SpriteAtlas& atlas, std::string& error) {
    atlas={}; error.clear();
    if (!supportedRed(rom)) { error="Sprite atlas requires the exact supported Pokemon Red revision"; return false; }
    constexpr unsigned count=72, table=0x17B27;
    SpriteAtlas decoded;
    decoded.width=96; decoded.height=count*16;
    decoded.rgba.resize(std::size_t(decoded.width)*decoded.height*4);
    for (unsigned picture=1; picture<=count; ++picture) {
        const auto entry=table+(picture-1)*4;
        const unsigned pointer=rom[entry]|unsigned(rom[entry+1])<<8;
        const unsigned bytes=rom[entry+2], bank=rom[entry+3];
        // The table describes the standing half. Moving sprites store the
        // second 192-byte half immediately after it; still objects use 64.
        const bool moving=bytes==192;
        const std::size_t offset=std::size_t(bank)*0x4000+pointer-0x4000;
        const unsigned total=moving?384:64;
        if (pointer<0x4000 || pointer+total>0x8000 || !bank ||
            (bytes!=192 && bytes!=64) || offset+total>rom.size()) {
            error="Sprite table range is invalid"; return false;
        }
        for (unsigned frame=0; frame<6; ++frame) {
            std::array<std::uint8_t,64> tiles{};
            std::copy_n(rom.begin()+offset+(moving?frame*64:0),64,tiles.begin());
            const auto rgba=colourSprite(picture,frame,spritePixels(tiles));
            for (unsigned y=0; y<16; ++y) for (unsigned x=0; x<16; ++x) {
                const std::size_t destination=((picture-1)*16+y)*decoded.width*4+(frame*16+x)*4;
                std::copy_n(rgba.begin()+(y*16+x)*4,4,decoded.rgba.begin()+destination);
            }
        }
    }
    atlas=std::move(decoded);
    return true;
}
RedState decodeRed(const WorkRam& ram, bool verifiedRom) {
    RedState result;
    if (!verifiedRom) { result.reason="unsupported ROM revision"; return result; }
    const auto read = [&](unsigned address) { return unsigned(ram[address-0xC000]); };
    result.map=read(0xD35E); result.x=read(0xD362); result.y=read(0xD361);
    result.width=read(0xD369); result.height=read(0xD368);
    struct Header { unsigned map,width,height,tileset,pointer,actors; const char* name; };
    constexpr Header headers[]={{0,10,9,0,0x42FD,3,"PalletTown"},
        {37,4,4,1,0x4209,1,"RedsHouse1F"},{38,4,4,4,0x4010,0,"RedsHouse2F"},
        {40,5,6,5,0x41C0,11,"OaksLab"}};
    const Header* header=nullptr;
    for (const auto& candidate : headers) if (candidate.map==result.map) header=&candidate;
    if (!header) { result.reason="map not supported"; return result; }
    if (result.width != header->width || result.height != header->height || read(0xD367) != header->tileset ||
        (read(0xD36A)|(read(0xD36B)<<8)) != header->pointer || read(0xD4E1) != header->actors) {
        result.reason="map header not ready"; return result;
    }
    if (result.x >= result.width*2 || result.y >= result.height*2) { result.reason="player outside map"; return result; }
    if (read(0xD057)) { result.reason="battle"; return result; }
    // The game's own toggle list separates off-screen sprites from objects
    // removed by scripts. Match the complete list for this exact map first;
    // during a transition it may still belong to the previous map.
    const unsigned toggleCount=result.map==40?8:result.map==0?1:0;
    const unsigned toggleBase=result.map==40?42:0;
    for (unsigned i=0;i<toggleCount;++i) {
        if (read(0xD5CE + i*2)!=i+1 || read(0xD5CF + i*2)!=toggleBase+i) {
            result.reason="object visibility not ready"; return result;
        }
    }
    if (read(0xD5CE + toggleCount*2)!=255) {
        result.reason="object visibility not ready"; return result;
    }
    const bool overlay=(read(0xCFC4)&1)!=0;
    // Cycling/surfing use a different player sheet, not the walking atlas.
    if (read(0xD700)) { result.reason="player travel mode not supported"; return result; }
    // Player movement does not use the NPC movement-status field: it normally
    // stays zero. Do not reject a real player using NPC readiness rules.
    if (read(0xC100)!=1 || (!overlay && read(0xC102)==255) || read(0xCFCB)!=1) {
        result.reason="player sprite not ready"; return result;
    }
    const auto signedStep=[&](unsigned address) {
        const unsigned value=read(address);
        return value==255 ? -1 : value==1 ? 1 : 0;
    };
    const auto makeActor=[&](unsigned slot,unsigned x,unsigned y) {
        const unsigned a=0xC100+slot*16,b=0xC200+slot*16;
        Actor actor{slot,read(a),x,y,read(a+9)};
        actor.worldX=float(x*2+1); actor.worldZ=float(y*2+1);
        // Player map coordinates change at the END of a 16-pixel step. NPC
        // coordinates change at its START. Counters are emulated game data,
        // not renderer time, so pause/save/load never extrapolate movement.
        const unsigned remaining=read(slot?b:0xCFC5);
        float movement=0;
        if (!slot && remaining>0 && remaining<=8) movement=float(8-remaining)*0.25f;
        if (slot && (read(a+1)&0x7F)==3 && remaining<=16) movement=-float(remaining)*0.125f;
        actor.worldX+=signedStep(a+5)*movement;
        actor.worldZ+=signedStep(a+3)*movement;
        // Low nibble is the game's own facing/animation-table index. Text
        // may hide the player under its panel; the preserved fields still
        // describe that walking sprite when the font overlay is active.
        const unsigned image=read(a+2)==255 ? ((read(a+9)&12)|(read(a+8)&3)) : read(a+2)&15;
        const unsigned direction=image/4,phase=image%4;
        if (actor.picture<61) {
            actor.frame=(direction<2?direction:2)+(phase%2?3:0);
            actor.flipX=direction==3 || (direction<2 && phase==3);
        }
        return actor;
    };
    result.supported=true; result.overlay=overlay; result.pallet=result.map==0; result.reason=header->name;
    if (overlay) result.ui=findTextWindows(ram);
    result.actors.reserve(header->actors+1);
    result.actors.push_back(makeActor(0,result.x,result.y));
    for (unsigned slot=1; slot<=header->actors; ++slot) {
        const auto a=0xC100+slot*16, b=0xC200+slot*16;
        const unsigned picture=read(a), x=read(b+5), y=read(b+4), status=read(a+1)&0x7F;
        if (slot<=toggleCount) {
            const unsigned flag=toggleBase+slot-1;
            if (read(0xD5A6+flag/8)&(1u<<(flag%8))) continue;
        }
        // An image index of FF also means outside the 160x144 viewport or
        // hidden underneath text. Once the toggle list proves the object is
        // enabled, render its initialized WRAM state even outside that view.
        // Never create an actor from the cartridge's default map coordinates.
        if (!picture || picture>72 || !status || status>3 || (read(a+9)&~12u) || read(a+8)>3 || x<4 || y<4 ||
            x>=result.width*2+4 || y>=result.height*2+4) continue;
        result.actors.push_back(makeActor(slot,x-4,y-4));
    }
    return result;
}
}
