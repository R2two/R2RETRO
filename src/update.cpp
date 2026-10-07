#include "update.h"
#include "update_sha256.h"
#include "http.h"
#include "file_ops.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace r2n64 {
namespace {
bool fail(std::string& error, const char* message) { error=message; return false; }
bool number(const std::string& s, uint64_t& n, uint64_t max) {
    n=0;
    if(s.empty() || s.size()>12) return false;
    for(char c:s) { if(c<'0'||c>'9'||n>(max-uint64_t(c-'0'))/10) return false; n=n*10+unsigned(c-'0'); }
    return n<=max;
}
bool version(const std::string& s, std::array<uint64_t,3>& v) {
    std::istringstream stream(s); std::string part;
    for(auto& n:v) {
        if(!std::getline(stream,part,'.') || (part.size()>1 && part[0]=='0') || !number(part,n,999)) return false;
    }
    return stream.eof();
}
bool sfoVersion(const std::string& s) {
    return s.size()==5 && s[2]=='.' && s[0]>='0'&&s[0]<='9'&&s[1]>='0'&&s[1]<='9'&&
        s[3]>='0'&&s[3]<='9'&&s[4]>='0'&&s[4]<='9';
}
uint32_t be(const uint8_t* p) { return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3]; }
uint32_t le(const uint8_t* p) { return uint32_t(p[3])<<24|uint32_t(p[2])<<16|uint32_t(p[1])<<8|p[0]; }
bool readAt(int fd, uint64_t offset, uint8_t* p, size_t n) {
    if(::lseek(fd,off_t(offset),SEEK_SET)<0) return false;
    while(n) {
        const auto got=::read(fd,p,n);
        if(got<0 && errno==EINTR) continue;
        if(got<=0) return false;
        p+=got; n-=size_t(got);
    }
    return true;
}
struct File { int fd; ~File() { if(fd>=0) ::close(fd); } };
bool sfoIdentity(const std::vector<uint8_t>& b, const UpdateRelease& release) {
    if(b.size()<20 || std::memcmp(b.data(),"\0PSF",4)) return false;
    const uint64_t keys=le(b.data()+8), data=le(b.data()+12), count=le(b.data()+16);
    if(count>128 || 20+count*16>keys || keys>=data || data>=b.size()) return false;
    std::map<std::string,std::string> values;
    for(size_t i=0;i<count;++i) {
        const auto* e=b.data()+20+i*16;
        const uint64_t key=keys+unsigned(e[0])+unsigned(e[1])*256;
        const uint64_t size=le(e+4), capacity=le(e+8), off=data+le(e+12);
        if(key>=data || off>b.size() || size>capacity || capacity>b.size()-off) return false;
        auto end=std::find(b.begin()+key,b.begin()+data,0);
        if(end==b.begin()+data) return false;
        std::string name(b.begin()+key,end);
        if(values.count(name)) return false;
        if(e[2]==4 && e[3]==2) {
            if(!size || b[off+size-1]!=0 || std::find(b.begin()+off,b.begin()+off+size-1,0)!=b.begin()+off+size-1) return false;
            values[name]=std::string(b.begin()+off,b.begin()+off+size-1);
        } else values[name]="";
    }
    return values["TITLE_ID"]==UpdateTitleId && values["CONTENT_ID"]==UpdateContentId &&
        values["APP_VER"]==release.sfo && (values["CATEGORY"]=="gd" || values["CATEGORY"]=="gde");
}
}
bool parseUpdateManifest(const std::string& text, UpdateRelease& out, std::string& error) {
    out={}; error.clear();
    if(text.empty() || text.size()>4096) return fail(error,"Manifiesto de actualización fuera de límites.");
    for(unsigned char c:text) if((c<32 && c!='\n') || c==127) return fail(error,"Manifiesto de actualización inválido.");
    std::istringstream stream(text); std::string line;
    if(!std::getline(stream,line) || line!="R2RETRO-UPDATE-1") return fail(error,"Formato de actualización no admitido.");
    std::map<std::string,std::string> fields;
    while(std::getline(stream,line)) {
        auto equal=line.find('=');
        if(equal==std::string::npos || !fields.emplace(line.substr(0,equal),line.substr(equal+1)).second)
            return fail(error,"Campos de actualización inválidos o duplicados.");
    }
    const char* names[]={"version","sfo","channel","url","sha256","size","notes","title_id","content_id"};
    if(fields.size()!=9) return fail(error,"Faltan campos de actualización.");
    for(auto name:names) if(!fields.count(name)) return fail(error,"Campo de actualización desconocido.");
    UpdateRelease r; r.version=fields["version"]; r.sfo=fields["sfo"]; r.channel=fields["channel"];
    r.url=fields["url"]; r.sha256=fields["sha256"]; r.notes=fields["notes"];
    std::array<uint64_t,3> parts{};
    if(!version(r.version,parts) || !sfoVersion(r.sfo) ||
        (r.channel!="stable" && r.channel!="experimental") || fields["title_id"]!=UpdateTitleId ||
        fields["content_id"]!=UpdateContentId || !number(fields["size"],r.size,UpdateMaxBytes) || r.size<4096 ||
        r.sha256.size()!=64 || r.sha256.find_first_not_of("0123456789abcdef")!=std::string::npos || r.notes.size()>240)
        return fail(error,"Identidad, versión o tamaño de actualización inválidos.");
    const auto prefix="https://github.com/R2two/R2RETRO/releases/download/v"+r.version+"/";
    if(r.url.compare(0,prefix.size(),prefix)!=0) return fail(error,"La actualización no pertenece al repositorio R2RETRO.");
    const auto name=r.url.substr(prefix.size());
    if(name.size()<5 || name.size()>160 || name.substr(name.size()-4)!=".pkg" || name.find("..")!=std::string::npos ||
        name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.")!=std::string::npos)
        return fail(error,"Nombre de PKG no admitido.");
    out=std::move(r); return true;
}
bool newerUpdate(const UpdateRelease& r, const std::string& current, const std::string& sfo) {
    std::array<uint64_t,3> a{},b{};
    return version(r.version,a) && version(current,b) && sfoVersion(r.sfo) && sfoVersion(sfo) && a>b && r.sfo>sfo;
}
std::string updateManifestUrl(bool experimental) {
    return std::string("https://raw.githubusercontent.com/R2two/R2RETRO/main/updates/")+
        (experimental?"experimental.txt":"stable.txt");
}
std::string updatePackageName(const UpdateRelease& r) { return "R2RETRO-"+r.sha256+".pkg"; }

UpdatePreferences loadUpdatePreferences(const std::string& root) {
    UpdatePreferences p;
    const auto dir=root+"/configs";
    File parent{::open(dir.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC)};
    File file{fileops::openAt(parent.fd,dir,"updates.conf",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC)};
    std::array<uint8_t,5> bytes{}; struct stat st{};
    if(file.fd>=0 && !::fstat(file.fd,&st) && S_ISREG(st.st_mode) && (st.st_size==4 || st.st_size==5) &&
        readAt(file.fd,0,bytes.data(),size_t(st.st_size)) &&
        ((st.st_size==4 && bytes[0]=='1' && bytes[3]=='\n') ||
         (st.st_size==5 && bytes[0]=='2' && bytes[4]=='\n' && (bytes[3]=='0'||bytes[3]=='1'))) &&
        (bytes[1]=='0'||bytes[1]=='1') && (bytes[2]=='0'||bytes[2]=='1')) {
        p.automatic=bytes[1]=='1'; p.experimental=bytes[2]=='1';
        // Read old DNS byte only for format validation. It cannot disable DoH.
    }
    return p;
}
bool saveUpdatePreferences(const std::string& root, const UpdatePreferences& p, std::string& error) {
    const auto dir=root+"/configs";
    File parent{::open(dir.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC)};
    if(parent.fd<0) return fail(error,"No se pudo guardar la preferencia de actualizaciones.");
    fileops::unlinkAt(parent.fd,dir,"updates.conf.tmp");
    File file{fileops::openAt(parent.fd,dir,"updates.conf.tmp",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600)};
    const char bytes[]={'2',p.automatic?'1':'0',p.experimental?'1':'0','1','\n'};
    bool ok=file.fd>=0;
    size_t written=0;
    while(ok && written<sizeof(bytes)) {
        auto n=::write(file.fd,bytes+written,sizeof(bytes)-written);
        if(n<0 && errno==EINTR) continue;
        if(n<=0) ok=false; else written+=size_t(n);
    }
    ok=ok && ::fsync(file.fd)==0 && fileops::renameAt(parent.fd,dir,"updates.conf.tmp",parent.fd,dir,"updates.conf")==0;
    if(!ok) { fileops::unlinkAt(parent.fd,dir,"updates.conf.tmp"); return fail(error,"No se pudo guardar la preferencia de actualizaciones."); }
    error.clear(); return true;
}

bool verifyUpdatePackage(int fd, const UpdateRelease& r, const std::atomic<bool>& cancel, std::string& error) {
    error.clear(); struct stat st{};
    if(r.size<4096 || r.size>UpdateMaxBytes || ::fstat(fd,&st) || !S_ISREG(st.st_mode) ||
        st.st_nlink!=1 || st.st_size<0 || uint64_t(st.st_size)!=r.size)
        return fail(error,"Tamaño o tipo de PKG incorrecto.");
    std::array<uint8_t,65536> chunk{}; UpdateSha256 sha;
    if(::lseek(fd,0,SEEK_SET)<0) return fail(error,"No se pudo leer el PKG.");
    for(uint64_t done=0;done<r.size;) {
        if(cancel) return fail(error,"Actualización cancelada.");
        const auto n=::read(fd,chunk.data(),size_t(std::min<uint64_t>(chunk.size(),r.size-done)));
        if(n<0 && errno==EINTR) continue;
        if(n<=0) return fail(error,"Lectura incompleta del PKG.");
        sha.add(chunk.data(),size_t(n)); done+=uint64_t(n);
    }
    if(sha.finish()!=r.sha256) return fail(error,"SHA-256 incorrecto. No se instalará el PKG.");
    if(!readAt(fd,0,chunk.data(),4096) || std::memcmp(chunk.data(),"\x7f" "CNT",4) ||
        std::memcmp(chunk.data()+0x40,UpdateContentId,36) || chunk[0x40+36]!=0 ||
        be(chunk.data()+0x74)!=0x1a || (be(chunk.data()+0x78)&0x60100000u) ||
        ((uint64_t(be(chunk.data()+0x430))<<32)|be(chunk.data()+0x434))!=r.size)
        return fail(error,"El PKG no es una aplicación completa de R2RETRO.");
    const uint64_t count=be(chunk.data()+0x10), table=be(chunk.data()+0x18);
    if(!count || count>4096 || table>r.size || count*32>r.size-table)
        return fail(error,"Tabla de PKG inválida.");
    std::vector<uint8_t> sfo; bool found=false;
    for(size_t i=0;i<count;++i) {
        if(cancel) return fail(error,"Actualización cancelada.");
        if(!readAt(fd,table+i*32,chunk.data(),32)) return fail(error,"Tabla de PKG incompleta.");
        if(be(chunk.data())!=0x1000) continue;
        const uint64_t off=be(chunk.data()+16), size=be(chunk.data()+20);
        if(found || size<20 || size>65536 || off>r.size || size>r.size-off)
            return fail(error,"SFO de actualización inválido.");
        found=true; sfo.resize(size_t(size));
        if(!readAt(fd,off,sfo.data(),sfo.size())) return fail(error,"SFO incompleto.");
    }
    if(!found || !sfoIdentity(sfo,r)) return fail(error,"El SFO no coincide con la versión e identidad anunciadas.");
    return !cancel || fail(error,"Actualización cancelada.");
}

bool downloadUpdate(const std::string& root, const std::string& ca, const UpdateRelease& r,
                    const std::atomic<bool>& cancel, std::atomic<uint64_t>& received,
                    std::string& path, std::string& error, const HttpOptions& options) {
    path.clear();
    options.report(HttpStage::Verify);
    File parent{::open(root.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC)};
    if(parent.fd<0 || (fileops::mkdirAt(parent.fd,root,"updates",0700)<0 && errno!=EEXIST))
        return fail(error,"No se pudo preparar la carpeta de actualizaciones.");
    const auto dir=root+"/updates";
    File folder{fileops::openAt(parent.fd,root,"updates",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC)};
    if(folder.fd<0) return fail(error,"Carpeta de actualizaciones no válida.");
    const auto name=updatePackageName(r), part=name+".part";
    File existing{fileops::openAt(folder.fd,dir,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC)};
    if(existing.fd>=0 && verifyUpdatePackage(existing.fd,r,cancel,error)) {
        received=r.size; path=dir+"/"+name; return true;
    }
    if(cancel) return fail(error,"Actualización cancelada.");
    if(fileops::unlinkAt(folder.fd,dir,part.c_str())<0 && errno!=ENOENT)
        return fail(error,"No se pudo limpiar una descarga parcial.");
    File file{fileops::openAt(folder.fd,dir,part.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600)};
    if(file.fd<0) return fail(error,"No se pudo crear el PKG temporal.");
    struct Cleanup { int fd; const std::string& dir; const std::string& name;
        ~Cleanup(){ fileops::unlinkAt(fd,dir,name.c_str()); } } cleanup{folder.fd,dir,part};
    if(!httpDownload(r.url,r.size,ca,cancel,file.fd,received,error,options)) return false;
    options.report(HttpStage::Verify);
    if(::fsync(file.fd)!=0) return fail(error,"No se pudo guardar el PKG completo.");
    if(!verifyUpdatePackage(file.fd,r,cancel,error)) return false;
    if(fileops::renameAt(folder.fd,dir,part.c_str(),folder.fd,dir,name.c_str())<0)
        return fail(error,"No se pudo finalizar la descarga verificada.");
    path=dir+"/"+name; return true;
}

UpdateService::~UpdateService(){ stop(); }
void UpdateService::stop(){ cancel_=true; if(worker_.joinable()) worker_.join(); }
bool UpdateService::start(UpdateJob job, const std::string& root, const std::string& ca,
                          bool experimental, const UpdateRelease& release) {
    if(busy()) return false;
    cancel_=false; ready_=false; received_=0; pending_={}; pending_.job=job; pending_.release=release;
    stage_=HttpStage::Idle;
    try {
        worker_=std::thread([this,job,root,ca,experimental,release] {
            HttpOptions options; options.stage=&stage_;
            try {
                if(job==UpdateJob::Check) {
                    std::vector<uint8_t> bytes;
                    pending_.ok=httpGet(updateManifestUrl(experimental),4096,ca,cancel_,bytes,pending_.error,options) &&
                        parseUpdateManifest(std::string(bytes.begin(),bytes.end()),pending_.release,pending_.error);
                    if(!pending_.ok && pending_.error=="Archivo no encontrado (HTTP 404).")
                        pending_.error="Este canal todavía no tiene un manifiesto publicado. Puedes seguir jugando y consultar más adelante.";
                    if(pending_.ok && pending_.release.channel!=(experimental?"experimental":"stable")) {
                        pending_.ok=false; pending_.error="El canal recibido no coincide con el seleccionado.";
                    }
                } else if(!newerUpdate(release,R2N64_VERSION,R2N64_SFO_VERSION)) {
                    pending_.error="La versión no es posterior a la instalada.";
                } else if(job==UpdateJob::Download) {
                    pending_.ok=downloadUpdate(root,ca,release,cancel_,received_,pending_.path,pending_.error,options);
                } else {
                    stage_=HttpStage::Verify;
                    pending_.path=root+"/updates/"+updatePackageName(release);
                    File file{::open(pending_.path.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC)};
                    pending_.ok=verifyUpdatePackage(file.fd,release,cancel_,pending_.error);
                    if(pending_.ok && !cancel_) {
                        stage_=HttpStage::Install;
                        pending_.ok=queueUpdateInstall(pending_.path,release,pending_.error);
                    }
                }
            } catch(...) { pending_.ok=false; pending_.error="No se pudo completar la actualización."; }
            // Once handed to BGFT, cancellation cannot undo the external task.
            pending_.cancelled=cancel_ && !(job==UpdateJob::Install && pending_.ok);
            stage_=HttpStage::Done;
            ready_=true;
        });
    } catch(...) { return false; }
    return true;
}
bool UpdateService::poll(UpdateResult& result) {
    if(!busy() || !ready_) return false;
    worker_.join(); result=std::move(pending_); ready_=false; return true;
}
}
