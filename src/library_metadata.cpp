#include "library_metadata.h"
#include "file_ops.h"
#include "http.h"
#include "frontend/system_detector.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace r2n64 {
namespace {
using Bytes = std::vector<uint8_t>;
constexpr size_t databaseLimit = 32 * 1024 * 1024;
constexpr size_t imageLimit = 4 * 1024 * 1024;
constexpr size_t metadataLimit = 16384;
constexpr unsigned recordLimit = 250000;
bool fail(std::string& error, const std::string& message) { error = message; return false; }
bool cancelled(const std::atomic<bool>& cancel, std::string& error) {
    if (!cancel) return false;
    error = "Descarga cancelada";
    return true;
}
struct File {
    int fd;
    explicit File(int value = -1) : fd(value) {}
    ~File() { if (fd >= 0) ::close(fd); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
};
uint32_t big32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
uint64_t big64(const uint8_t* p) { return (uint64_t(big32(p)) << 32) | big32(p + 4); }
uint32_t rotate(uint32_t value, unsigned bits) { return (value << bits) | (value >> (32 - bits)); }

// Streaming SHA-1 identifies existing database records; it is not used as a
// security signature. No ROM bytes are transmitted to the remote server.
class Sha1 {
    std::array<uint32_t, 5> state{{0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0}};
    std::array<uint8_t, 64> pending{};
    uint64_t total = 0;
    size_t used = 0;
    void block(const uint8_t* data) {
        uint32_t words[80];
        for (unsigned i = 0; i < 16; ++i) words[i] = big32(data + i * 4);
        for (unsigned i = 16; i < 80; ++i) words[i] = rotate(words[i-3] ^ words[i-8] ^ words[i-14] ^ words[i-16], 1);
        uint32_t a=state[0], b=state[1], c=state[2], d=state[3], e=state[4];
        for (unsigned i = 0; i < 80; ++i) {
            const uint32_t f = i < 20 ? (b & c) | (~b & d) : i < 40 ? b ^ c ^ d :
                               i < 60 ? (b & c) | (b & d) | (c & d) : b ^ c ^ d;
            const uint32_t k = i < 20 ? 0x5a827999 : i < 40 ? 0x6ed9eba1 : i < 60 ? 0x8f1bbcdc : 0xca62c1d6;
            const uint32_t next = rotate(a,5) + f + e + k + words[i];
            e=d; d=c; c=rotate(b,30); b=a; a=next;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d; state[4]+=e;
    }
public:
    void update(const uint8_t* data, size_t length) {
        total += length;
        while (length) {
            const size_t count = std::min(length, pending.size() - used);
            std::memcpy(pending.data() + used, data, count);
            used += count; data += count; length -= count;
            if (used == pending.size()) { block(pending.data()); used = 0; }
        }
    }
    std::string finish() {
        const uint64_t bits = total * 8;
        uint8_t suffix[72]{};
        suffix[0] = 0x80;
        const size_t padding = used < 56 ? 56 - used : 120 - used;
        for (unsigned i = 0; i < 8; ++i) suffix[padding + i] = static_cast<uint8_t>(bits >> ((7-i)*8));
        update(suffix, padding + 8);
        std::string result;
        static constexpr char hex[] = "0123456789abcdef";
        for (uint32_t word : state) for (int shift = 28; shift >= 0; shift -= 4) result += hex[(word >> shift) & 15];
        return result;
    }
};
uint32_t crcUpdate(uint32_t crc, const uint8_t* data, size_t size) {
    static const auto table = [] {
        std::array<uint32_t,256> result{};
        for (unsigned i=0;i<256;++i) {
            uint32_t value=i;
            for (unsigned bit=0;bit<8;++bit) value=(value>>1)^((value&1)?0xedb88320u:0u);
            result[i]=value;
        }
        return result;
    }();
    for (size_t i=0;i<size;++i) crc=table[(crc^data[i])&255]^(crc>>8);
    return crc;
}
std::string hexBytes(const uint8_t* bytes, size_t size) {
    static constexpr char hex[]="0123456789abcdef";
    std::string result;
    for (size_t i=0;i<size;++i) { result+=hex[bytes[i]>>4]; result+=hex[bytes[i]&15]; }
    return result;
}
std::string sha(const Bytes& bytes) { Sha1 hash; hash.update(bytes.data(),bytes.size()); return hash.finish(); }
const char* databaseName(SystemType system) {
    switch (system) {
    case SystemType::GameBoy: return "Nintendo - Game Boy";
    case SystemType::GameBoyColor: return "Nintendo - Game Boy Color";
    case SystemType::GameBoyAdvance: return "Nintendo - Game Boy Advance";
    case SystemType::Nintendo64: return "Nintendo - Nintendo 64";
    case SystemType::NintendoEntertainmentSystem: return "Nintendo - Nintendo Entertainment System";
    case SystemType::SuperNintendo: return "Nintendo - Super Nintendo Entertainment System";
    default: return nullptr;
    }
}
bool safeId(const std::string& id) {
    return !id.empty() && id.size() <= 80 && std::all_of(id.begin(),id.end(),[](unsigned char c) {
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';
    });
}
bool validText(const std::string& text, size_t maximum = 1024) {
    if (text.size() > maximum) return false;
    for (size_t i=0;i<text.size();) {
        const uint8_t first=static_cast<uint8_t>(text[i++]);
        if (first<32 || first==127) return false;
        if (first<128) continue;
        unsigned extra, point, minimum;
        if (first>=0xc2&&first<=0xdf) {extra=1;point=first&31;minimum=0x80;}
        else if (first>=0xe0&&first<=0xef) {extra=2;point=first&15;minimum=0x800;}
        else if (first>=0xf0&&first<=0xf4) {extra=3;point=first&7;minimum=0x10000;}
        else return false;
        for (unsigned n=0;n<extra;++n) {
            if (i==text.size()) return false;
            const uint8_t byte=static_cast<uint8_t>(text[i++]);
            if ((byte&0xc0)!=0x80) return false;
            point=(point<<6)|(byte&63);
        }
        if (point<minimum||point>0x10ffff||(point>=0xd800&&point<=0xdfff)) return false;
    }
    return true;
}
std::string encoded(const std::string& value) {
    static constexpr char hex[]="0123456789ABCDEF";
    std::string out;
    for (unsigned char c:value) {
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') out+=char(c);
        else { out+='%';out+=hex[c>>4];out+=hex[c&15]; }
    }
    return out;
}
std::string thumbnailName(std::string title) {
    for (char& c:title) if (std::strchr("&*/:`<>?\\|\"",c)) c='_';
    return encoded(title + ".png");
}

// Local paths are generated exclusively from fixed names, systemId and safeId.
// Desktop walks use no-follow descriptors; PS4 uses the checked path backend.
int directory(const std::string& path, bool create, std::string& openedPath, std::string& error) {
    if (path.empty()||path.find('\0')!=std::string::npos) {fail(error,"Ruta de cache invalida");return -1;}
    std::vector<std::string> parts;
    for (size_t start=0;start<path.size();) {
        const auto slash=path.find('/',start);
        const auto end=slash==std::string::npos?path.size():slash;
        auto part=path.substr(start,end-start);start=end+1;
        if (part=="..") {fail(error,"Ruta de cache fuera de limite");return -1;}
        if (!part.empty()&&part!=".") parts.push_back(std::move(part));
    }
    File current(::open(path.front()=='/'?"/":".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    if (current.fd<0) {fail(error,"No se pudo abrir la cache");return -1;}
    std::string currentPath=path.front()=='/'?"/":".";
    for (const auto& part:parts) {
        int next=fileops::openAt(current.fd,currentPath,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if (next<0&&errno==ENOENT&&create) {
            if (fileops::mkdirAt(current.fd,currentPath,part.c_str(),0700)<0&&errno!=EEXIST) {fail(error,"No se pudo crear la cache");return -1;}
            next=fileops::openAt(current.fd,currentPath,part.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        }
        if (next<0) {if(errno!=ENOENT||create) fail(error,"Cache inaccesible o directorio enlazado");return -1;}
        ::close(current.fd);current.fd=next;
        if(currentPath!="/")currentPath+='/';
        currentPath+=part;
    }
    openedPath=std::move(currentPath);
    const int result=current.fd;current.fd=-1;return result;
}
bool readFile(const std::string& path, size_t limit, Bytes& bytes, std::string& error) {
    bytes.clear();
    const auto slash=path.find_last_of('/');
    std::string parentPath;
    File parent(directory(slash==std::string::npos?".":slash==0?"/":path.substr(0,slash),false,parentPath,error));
    if(parent.fd<0)return false;
    File file(fileops::openAt(parent.fd,parentPath,path.substr(slash==std::string::npos?0:slash+1).c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));
    if(file.fd<0) {if(errno!=ENOENT)fail(error,"Archivo de cache inaccesible o enlazado");return false;}
    struct stat info{};
    if(::fstat(file.fd,&info)<0||!S_ISREG(info.st_mode)||info.st_nlink!=1||info.st_size<=0||uint64_t(info.st_size)>limit)
        return fail(error,"Archivo de cache invalido o demasiado grande");
    bytes.resize(static_cast<size_t>(info.st_size));
    size_t done=0;
    while(done<bytes.size()) {
        const auto count=::read(file.fd,bytes.data()+done,bytes.size()-done);
        if(count<0&&errno==EINTR)continue;
        if(count<=0)return fail(error,"Cache truncada durante lectura");
        done+=static_cast<size_t>(count);
    }
    uint8_t extra;
    if(::read(file.fd,&extra,1)!=0)return fail(error,"Cache cambio durante lectura");
    return true;
}
bool regularTarget(int parent,const std::string& parentPath,const std::string& name,std::string& error) {
    File target(fileops::openAt(parent,parentPath,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));
    if(target.fd<0)return errno==ENOENT?true:fail(error,"Destino de cache inaccesible o enlazado");
    struct stat info{};
    return (::fstat(target.fd,&info)==0&&S_ISREG(info.st_mode)&&info.st_nlink==1)||fail(error,"Destino de cache no regular");
}
bool atomicWrite(const std::string& path,const Bytes& bytes,const std::atomic<bool>& cancel,std::string& error) {
    if(cancelled(cancel,error))return false;
    const auto slash=path.find_last_of('/');
    std::string parentPath;
    File parent(directory(slash==std::string::npos?".":slash==0?"/":path.substr(0,slash),true,parentPath,error));
    if(parent.fd<0)return false;
    const auto name=path.substr(slash+1);
    if(!regularTarget(parent.fd,parentPath,name,error))return false;
    static std::atomic<unsigned> sequence{0};
    File file;
    std::string temporary;
    for(unsigned retry=0;retry<32;++retry) {
        temporary=".catalog-"+std::to_string(::getpid())+"-"+std::to_string(sequence++);
        file.fd=fileops::openAt(parent.fd,parentPath,temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(file.fd>=0||errno!=EEXIST)break;
    }
    if(file.fd<0)return fail(error,"No se pudo crear temporal de cache");
    size_t written=0;
    bool ok=true;
    while(written<bytes.size()) {
        if(cancelled(cancel,error)){ok=false;break;}
        const auto count=::write(file.fd,bytes.data()+written,bytes.size()-written);
        if(count<0&&errno==EINTR)continue;
        if(count<=0){ok=false;break;}
        written+=static_cast<size_t>(count);
    }
    if(ok)ok=::fsync(file.fd)==0;
    if(::close(file.fd)<0)ok=false;
    file.fd=-1;
    if(ok)ok=!cancelled(cancel,error)&&regularTarget(parent.fd,parentPath,name,error);
    if(ok)ok=fileops::renameAt(parent.fd,parentPath,temporary.c_str(),parent.fd,parentPath,name.c_str())==0;
    if(!ok) {fileops::unlinkAt(parent.fd,parentPath,temporary.c_str());if(error.empty())error="No se pudo guardar cache; se conserva la anterior";}
    return ok;
}

struct Fingerprint { std::string sha1,view; uint32_t crc=0; uint64_t size=0; };
bool fingerprints(const Game& game,const std::atomic<bool>& cancel,std::vector<Fingerprint>& result,std::string& error) {
    result.clear();
    if(!databaseName(game.system)||!safeId(game.id))return fail(error,"Sistema o identificador de juego invalido");
    File file(::open(game.path.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC));
    struct stat info{};
    if(file.fd<0||::fstat(file.fd,&info)<0||!S_ISREG(info.st_mode)||info.st_size<=0||uint64_t(info.st_size)>maximumRomFileSize(game.system))
        return fail(error,"No se pudo leer una ROM regular dentro de los limites");
    const uint64_t size=static_cast<uint64_t>(info.st_size);
    std::array<uint8_t,16384> buffer{};
    size_t initial=std::min<uint64_t>(buffer.size(),size);
    size_t have=0;
    while(have<initial) {
        const auto count=::read(file.fd,buffer.data()+have,initial-have);
        if(count<0&&errno==EINTR)continue;
        if(count<=0)return fail(error,"ROM truncada durante identificacion");
        have+=static_cast<size_t>(count);
    }
    unsigned swap=0;
    uint64_t skip=0,nesSkip=0;
    if(game.system==SystemType::Nintendo64) {
        if(initial<4||size%4)return fail(error,"ROM N64 incompleta");
        const auto magic=big32(buffer.data());
        if(magic==0x37804012)swap=2;
        else if(magic==0x40123780)swap=4;
        else if(magic!=0x80371240)return fail(error,"Orden de bytes N64 invalido");
    }
    if(game.system==SystemType::SuperNintendo)skip=snesCopierHeaderSize(size);
    if(game.system==SystemType::NintendoEntertainmentSystem) {
        if(initial<16||std::memcmp(buffer.data(),"NES\x1a",4)!=0)return fail(error,"Cabecera NES invalida");
        nesSkip=16+((buffer[6]&4)?512:0);
        if(size<=nesSkip)return fail(error,"ROM NES incompleta");
    }
    Sha1 full,payload;
    uint32_t crc=0xffffffff,nesCrc=0xffffffff;
    uint64_t offset=0;
    for(;;) {
        if(cancelled(cancel,error))return false;
        if(swap==2)for(size_t i=0;i<initial;i+=2)std::swap(buffer[i],buffer[i+1]);
        if(swap==4)for(size_t i=0;i<initial;i+=4){std::swap(buffer[i],buffer[i+3]);std::swap(buffer[i+1],buffer[i+2]);}
        const size_t begin=offset>=skip?0:static_cast<size_t>(std::min<uint64_t>(initial,skip-offset));
        full.update(buffer.data()+begin,initial-begin);crc=crcUpdate(crc,buffer.data()+begin,initial-begin);
        if(nesSkip) {
            const size_t start=offset>=nesSkip?0:static_cast<size_t>(std::min<uint64_t>(initial,nesSkip-offset));
            payload.update(buffer.data()+start,initial-start);nesCrc=crcUpdate(nesCrc,buffer.data()+start,initial-start);
        }
        offset+=initial;
        if(offset==size)break;
        initial=static_cast<size_t>(std::min<uint64_t>(buffer.size(),size-offset));
        have=0;
        while(have<initial) {
            const auto count=::read(file.fd,buffer.data()+have,initial-have);
            if(count<0&&errno==EINTR)continue;
            if(count<=0)return fail(error,"ROM truncada durante identificacion");
            have+=static_cast<size_t>(count);
        }
    }
    struct stat after{};
    if(::fstat(file.fd,&after)<0||after.st_size!=info.st_size||after.st_mtime!=info.st_mtime)
        return fail(error,"ROM cambio durante identificacion");
    result.push_back({full.finish(),skip?"sin copier SNES":swap?"N64 big-endian":"contenido completo",crc^0xffffffff,size-skip});
    if(nesSkip)result.push_back({payload.finish(),"sin cabecera/trainer NES",nesCrc^0xffffffff,size-nesSkip});
    return true;
}

struct Value {
    enum class Type { Other, String, Binary, Unsigned, Map, Nil } type=Type::Other;
    std::string text;
    uint64_t number=0;
};
using Record=std::map<std::string,Value>;
class Msgpack {
    const Bytes& data;
    size_t limit;
    bool allowDuplicateSerial;
    bool number(size_t bytes,uint64_t& out) {
        if(bytes>limit-position)return false;
        out=0;for(size_t i=0;i<bytes;++i)out=(out<<8)|data[position++];return true;
    }
public:
    size_t position;
    Msgpack(const Bytes& bytes,size_t start,size_t end,bool databaseRecord=false)
        :data(bytes),limit(end),allowDuplicateSerial(databaseRecord),position(start){}
    bool read(Value& out,unsigned depth=0,Record* record=nullptr) {
        if(position>=limit||depth>8)return false;
        const uint8_t kind=data[position++];uint64_t count=0;
        if(kind<0x80){out.type=Value::Type::Unsigned;out.number=kind;return true;}
        if(kind>=0xe0)return true;
        if(kind==0xc0){out.type=Value::Type::Nil;return true;}
        if(kind==0xc2||kind==0xc3)return true;
        if(kind>=0xcc&&kind<=0xcf){out.type=Value::Type::Unsigned;return number(size_t(1)<<(kind-0xcc),out.number);}
        if(kind>=0xd0&&kind<=0xd3)return number(size_t(1)<<(kind-0xd0),out.number);
        if(kind==0xca||kind==0xcb)return number(kind==0xca?4:8,out.number);
        if((kind&0xe0)==0xa0)count=kind&31;
        else if(kind==0xd9||kind==0xda||kind==0xdb){if(!number(size_t(1)<<(kind-0xd9),count))return false;}
        else if(kind==0xc4||kind==0xc5||kind==0xc6){if(!number(size_t(1)<<(kind-0xc4),count))return false;out.type=Value::Type::Binary;}
        else if((kind&0xf0)==0x80||kind==0xde||kind==0xdf) {
            if((kind&0xf0)==0x80)count=kind&15;
            else if(!number(kind==0xde?2:4,count))return false;
            if(count>256)return false;
            out.type=Value::Type::Map;
            std::set<std::string> seen;
            for(uint64_t i=0;i<count;++i){
                Value key,value;
                if(!read(key,depth+1)||key.type!=Value::Type::String||key.text.empty()||key.text.size()>128)return false;
                // Official RDB records may contain both textual and binary
                // serial fields. We do not use either for content matching.
                if(!seen.insert(key.text).second&&!(allowDuplicateSerial&&key.text=="serial"))return false;
                if(!read(value,depth+1))return false;
                if(record)record->emplace(std::move(key.text),std::move(value));
            }
            return true;
        } else if((kind&0xf0)==0x90||kind==0xdc||kind==0xdd) {
            if((kind&0xf0)==0x90)count=kind&15;
            else if(!number(kind==0xdc?2:4,count))return false;
            if(count>1024)return false;
            for(uint64_t i=0;i<count;++i){Value ignored;if(!read(ignored,depth+1))return false;}
            return true;
        } else return false;
        if(count>65536||count>limit-position)return false;
        if(out.type!=Value::Type::Binary)out.type=Value::Type::String;
        out.text.assign(reinterpret_cast<const char*>(data.data()+position),static_cast<size_t>(count));
        position+=static_cast<size_t>(count);return true;
    }
};
std::string textField(const Record& record,const std::string& key) {
    const auto it=record.find(key);
    if(it==record.end())return {};
    if(it->second.type==Value::Type::Unsigned)return std::to_string(it->second.number);
    return it->second.type==Value::Type::String&&validText(it->second.text)?it->second.text:std::string();
}
std::string digestField(const Record& record,const std::string& key,size_t size) {
    const auto it=record.find(key);if(it==record.end())return {};
    if(it->second.type==Value::Type::Binary&&it->second.text.size()==size)
        return hexBytes(reinterpret_cast<const uint8_t*>(it->second.text.data()),size);
    return {};
}
LibraryMetadata fields(const Record& record) {
    LibraryMetadata result;
    result.title=textField(record,"name");result.region=textField(record,"region");
    result.developer=textField(record,"developer");result.publisher=textField(record,"publisher");
    result.genre=textField(record,"genre");result.year=textField(record,"releaseyear");result.players=textField(record,"users");
    if(result.players.empty())result.players=textField(record,"players");
    return result;
}
bool sameMatch(const LibraryMetadata& a,const LibraryMetadata& b) {
    return a.title==b.title&&a.region==b.region&&a.developer==b.developer&&a.publisher==b.publisher&&
           a.genre==b.genre&&a.year==b.year&&a.players==b.players;
}
// Walk records once, ignoring indexes entirely. Verify the nil sentinel and
// metadata count so truncated/corrupt databases cannot yield an early false match.
bool lookup(const Bytes& database,const std::vector<Fingerprint>& hashes,const std::atomic<bool>& cancel,
            LibraryMetadata& metadata,std::string& error,bool* validDatabase=nullptr) {
    if(validDatabase)*validDatabase=false;
    if(database.size()<18||database.size()>databaseLimit||std::memcmp(database.data(),"RARCHDB\0",8)!=0)
        return fail(error,"Cabecera RDB invalida");
    const uint64_t end=big64(database.data()+8);
    if(end<=16||end>=database.size())return fail(error,"Limites RDB invalidos");
    Msgpack parser(database,16,static_cast<size_t>(end),true);
    unsigned count=0;int best=0;bool ambiguous=false,sentinel=false;
    while(parser.position<end) {
        if(cancelled(cancel,error))return false;
        Value value;Record record;
        if(!parser.read(value,0,&record))return fail(error,"Registro RDB truncado o invalido");
        if(value.type==Value::Type::Nil){sentinel=true;break;}
        if(value.type!=Value::Type::Map||++count>recordLimit)return fail(error,"Demasiados registros RDB o tipo invalido");
        const auto sha1=digestField(record,"sha1",20),crc=digestField(record,"crc",4);
        int strength=0;std::string matched;
        for(const auto& hash:hashes) {
            const uint8_t crcBytes[]{uint8_t(hash.crc>>24),uint8_t(hash.crc>>16),uint8_t(hash.crc>>8),uint8_t(hash.crc)};
            const auto size=record.find("size");
            if(size!=record.end()&&size->second.type==Value::Type::Unsigned&&size->second.number!=hash.size)continue;
            const int current=!sha1.empty()&&sha1==hash.sha1?2:
                record.count("sha1")==0&&!crc.empty()&&crc==hexBytes(crcBytes,4)?1:0;
            if(current>strength){strength=current;matched=(current==2?"SHA-1: ":"CRC32 unico: ")+hash.view;}
        }
        if(strength==0)continue;
        auto candidate=fields(record);
        if(candidate.title.empty()||!validText(candidate.title,512))continue;
        candidate.match=matched;
        if(strength>best){metadata=std::move(candidate);best=strength;ambiguous=false;}
        else if(strength==best&&!sameMatch(candidate,metadata))ambiguous=true;
    }
    if(!sentinel||parser.position!=end)return fail(error,"RDB sin final de registros valido");
    Msgpack tail(database,static_cast<size_t>(end),database.size());
    Value value;Record info;
    if(!tail.read(value,0,&info)||value.type!=Value::Type::Map||info.count("count")!=1||
       info.at("count").type!=Value::Type::Unsigned||info.at("count").number!=count)
        return fail(error,"Conteo RDB invalido");
    if(validDatabase)*validDatabase=true;
    if(ambiguous)return fail(error,"Coincidencia ambigua en Libretro; no se asigno ficha");
    if(best==0)return fail(error,"Sin coincidencia de contenido en Libretro");
    return true;
}

bool validPng(const Bytes& data) {
    static constexpr uint8_t signature[]{137,80,78,71,13,10,26,10};
    if(data.size()<64||data.size()>imageLimit||std::memcmp(data.data(),signature,8)!=0)return false;
    bool header=false,palette=false,pixels=false;
    unsigned color=0,chunks=0;
    size_t offset=8;
    while(offset<data.size()) {
        if(++chunks>4096||data.size()-offset<12)return false;
        const uint32_t length=big32(data.data()+offset);
        if(length>data.size()-offset-12)return false;
        const auto* type=data.data()+offset+4;
        const auto* bytes=type+4;
        if((crcUpdate(0xffffffff,type,length+4)^0xffffffff)!=big32(bytes+length))return false;
        if(std::memcmp(type,"IHDR",4)==0) {
            if(header||offset!=8||length!=13)return false;
            const uint32_t width=big32(bytes),height=big32(bytes+4);
            if(width<8||height<8||width>2048||height>2048||uint64_t(width)*height>4*1024*1024)return false;
            color=bytes[9];const auto depth=bytes[8];
            const bool validDepth=color==0?(depth==1||depth==2||depth==4||depth==8||depth==16):
                color==3?(depth==1||depth==2||depth==4||depth==8):
                (color==2||color==4||color==6)&&(depth==8||depth==16);
            if(!validDepth||bytes[10]!=0||bytes[11]!=0||bytes[12]>1)return false;
            header=true;
        } else if(std::memcmp(type,"PLTE",4)==0) {
            if(!header||pixels||palette||length==0||length>768||length%3)return false;
            palette=true;
        } else if(std::memcmp(type,"IDAT",4)==0) {
            if(!header||(color==3&&!palette)||length==0)return false;
            pixels=true;
        } else if(std::memcmp(type,"IEND",4)==0) return header&&pixels&&length==0&&offset+12==data.size();
        else if((type[0]&32)==0)return false;
        offset+=size_t(length)+12;
    }
    return false;
}
void appendString(Bytes& output,const std::string& value) {
    if(value.size()<32)output.push_back(uint8_t(0xa0|value.size()));
    else {output.push_back(0xda);output.push_back(uint8_t(value.size()>>8));output.push_back(uint8_t(value.size()));}
    output.insert(output.end(),value.begin(),value.end());
}
std::string cachePath(const std::string& root,const Game& game) {
    return root+"/cache/library/"+systemId(game.system)+"/"+game.id+".meta";
}
std::string imageDirectory(const std::string& root,const Game& game) {
    return root+"/covers/"+systemId(game.system)+"/"+game.id+"/";
}
bool imageName(const std::string& name,const char* prefix) {
    const std::string start=std::string(prefix)+"-";
    return name.size()==start.size()+44&&name.compare(0,start.size(),start)==0&&name.compare(name.size()-4,4,".png")==0&&
        std::all_of(name.begin()+start.size(),name.end()-4,[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
Bytes cacheBytes(const Game& game,const std::string& hash,const LibraryMetadata& m,const std::array<std::string,3>& images) {
    const std::pair<const char*,std::string> entries[]{
        {"system",systemId(game.system)},{"id",game.id},{"sha1",hash},{"name",m.title},{"region",m.region},
        {"developer",m.developer},{"publisher",m.publisher},{"genre",m.genre},{"releaseyear",m.year},
        {"users",m.players},{"match",m.match},{"boxart",images[0]},{"snap",images[1]},{"title",images[2]}};
    Bytes bytes{'R','2','M','E','T','A','1',0,0x8e};
    for(const auto& entry:entries){appendString(bytes,entry.first);appendString(bytes,entry.second);}
    return bytes;
}
bool readCache(const std::string& root,const Game& game,const std::string& hash,LibraryMetadata& metadata,
               std::array<std::string,3>& images,std::string& error) {
    Bytes bytes;
    if(!readFile(cachePath(root,game),metadataLimit,bytes,error))return false;
    if(bytes.size()<9||std::memcmp(bytes.data(),"R2META1\0",8)!=0)return fail(error,"Ficha local invalida");
    Msgpack parser(bytes,8,bytes.size());Value value;Record record;
    if(!parser.read(value,0,&record)||value.type!=Value::Type::Map||parser.position!=bytes.size()||
       textField(record,"system")!=systemId(game.system)||textField(record,"id")!=game.id||textField(record,"sha1")!=hash)
        return fail(error,"La ficha local no corresponde al contenido actual");
    auto result=fields(record);result.match=textField(record,"match");
    if(result.title.empty()||result.match.empty()||!validText(result.title,512))return fail(error,"Ficha local sin identificacion");
    const char* names[]{"boxart","snap","title"};
    std::string* paths[]{&result.coverPath,&result.screenshotPath,&result.titleScreenPath};
    for(unsigned i=0;i<3;++i) {
        const auto name=textField(record,names[i]);
        if(name.empty())continue;
        if(!imageName(name,names[i]))return fail(error,"Ruta de imagen local invalida");
        Bytes png;std::string imageError;
        const auto path=imageDirectory(root,game)+name;
        if(readFile(path,imageLimit,png,imageError)&&validPng(png)&&name==std::string(names[i])+"-"+sha(png)+".png") {
            images[i]=name;*paths[i]=path;
        }
        // Missing/corrupt optional images do not invalidate an identified title.
    }
    metadata=std::move(result);return true;
}
}

bool loadCachedMetadata(const std::string& dataRoot,const Game& game,LibraryMetadata& metadata,std::string& error,
                        const std::atomic<bool>* cancel) {
    metadata={};error.clear();
    const std::atomic<bool> neverCancel{false};
    const std::atomic<bool>& cancellation=cancel?*cancel:neverCancel;
    if(cancelled(cancellation,error))return false;
    if(!databaseName(game.system)||!safeId(game.id))return fail(error,"Sistema o identificador de juego invalido");
    // Check the small cache before hashing a ROM when no metadata exists yet.
    Bytes cache;
    if(!readFile(cachePath(dataRoot,game),metadataLimit,cache,error))return false;
    std::vector<Fingerprint> hashes;
    if(!fingerprints(game,cancellation,hashes,error))return false;
    std::array<std::string,3> images{};
    LibraryMetadata result;
    if(!readCache(dataRoot,game,hashes[0].sha1,result,images,error))return false;
    if(cancelled(cancellation,error))return false;
    metadata=std::move(result);return true;
}

bool fetchLibraryMetadata(const std::string& dataRoot,const std::string& caFile,const Game& game,
                          const std::atomic<bool>& cancel,LibraryMetadata& metadata,std::string& error) {
    metadata={};error.clear();
    std::vector<Fingerprint> hashes;
    if(cancelled(cancel,error)||!fingerprints(game,cancel,hashes,error))return false;
    Bytes database;std::string cacheError;
    const std::string rdbPath=dataRoot+"/cache/libretro/"+systemId(game.system)+".rdb";
    bool downloaded=false;
    LibraryMetadata result;
    bool validDatabase=false;
    bool found=readFile(rdbPath,databaseLimit,database,cacheError)&&lookup(database,hashes,cancel,result,cacheError,&validDatabase);
    if(cancelled(cancel,error))return false;
    if(!found&&validDatabase)return fail(error,cacheError);
    if(!found) {
        const std::string url="https://raw.githubusercontent.com/libretro/libretro-database/master/rdb/"+
            encoded(std::string(databaseName(game.system))+".rdb");
        if(!httpGet(url,databaseLimit,caFile,cancel,database,error)||cancelled(cancel,error))return false;
        downloaded=true;
        if(!lookup(database,hashes,cancel,result,error))return false;
    }
    std::array<std::string,3> imageNames{};
    LibraryMetadata previous;std::string previousError;
    if(!readCache(dataRoot,game,hashes[0].sha1,previous,imageNames,previousError)||previous.title!=result.title)imageNames={};
    std::array<Bytes,3> images{};
    const char* localNames[]{"boxart","snap","title"};
    const char* folders[]{"Named_Boxarts","Named_Snaps","Named_Titles"};
    std::string warnings;
    // GitHub thumbnail repositories replace spaces with underscores in the
    // system name; thumbnail basenames follow the documented substitutions.
    std::string repository=databaseName(game.system);
    std::replace(repository.begin(),repository.end(),' ','_');
    for(unsigned i=0;i<3;++i) {
        if(cancelled(cancel,error))return false;
        const std::string url="https://raw.githubusercontent.com/libretro-thumbnails/"+encoded(repository)+
            "/master/"+folders[i]+"/"+thumbnailName(result.title);
        std::string imageError;
        const bool received=httpGet(url,imageLimit,caFile,cancel,images[i],imageError);
        if(received&&validPng(images[i]))
            imageNames[i]=std::string(localNames[i])+"-"+sha(images[i])+".png";
        else {
            images[i].clear();
            if(received)imageError="PNG invalido o dimensiones fuera de limite";
            else if(imageError.find("404")!=std::string::npos)imageError="No disponible en Libretro (HTTP 404)";
            if(imageError.empty())imageError="No se recibio una imagen valida";
            for(char& c:imageError)if(static_cast<unsigned char>(c)<32||c==127)c=' ';
            if(imageError.size()>256)imageError.resize(256);
            if(!warnings.empty())warnings+="; ";
            warnings+=std::string(localNames[i])+": "+imageError;
        }
        if(cancelled(cancel,error))return false;
    }
    if(cancelled(cancel,error))return false;
    // Immutable image names keep the old manifest valid even if a later commit
    // fails. Only the final manifest rename changes which images the UI sees.
    for(unsigned i=0;i<3;++i) if(!images[i].empty())
        if(!atomicWrite(imageDirectory(dataRoot,game)+imageNames[i],images[i],cancel,error))return false;
    if(downloaded&&!atomicWrite(rdbPath,database,cancel,error))return false;
    if(!atomicWrite(cachePath(dataRoot,game),cacheBytes(game,hashes[0].sha1,result,imageNames),cancel,error))return false;
    std::string* paths[]{&result.coverPath,&result.screenshotPath,&result.titleScreenPath};
    for(unsigned i=0;i<3;++i)if(!imageNames[i].empty())*paths[i]=imageDirectory(dataRoot,game)+imageNames[i];
    metadata=std::move(result);error=std::move(warnings);return true;
}
}
