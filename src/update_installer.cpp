#include "update.h"
#ifdef R2N64_PS4
#include <orbis/AppInstUtil.h>
#include <orbis/Sysmodule.h>
#include <cstddef>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace r2n64 {
bool queueUpdateInstall(const std::string& path, const UpdateRelease& release, std::string& error) {
#ifdef R2N64_PS4
    auto failed=[&](const char* stage,int32_t code) {
        char value[16];
        std::snprintf(value,sizeof(value),"0x%08X",unsigned(code));
        error=std::string("Instalador PS4: ")+stage+" ("+value+"). PKG conservado; no se desinstaló R2RETRO.";
        return false;
    };
    error.clear();
    // /data and /user/data can be different mount views. Device/inode equality
    // across them is not a content-integrity test. Validate the exact bytes at
    // every path we may hand off; never trust an alias just because it exists.
    struct File { int fd=-1; ~File(){if(fd>=0) ::close(fd);} } original, alias;
    auto pathFailure=[&](const std::string& reason,const std::string& file,int code=0) {
        error="Instalador PS4: "+reason;
        if(code) error+="; errno="+std::to_string(code)+" ("+std::strerror(code)+")";
        error+=" ["+file+"]. PKG conservado; no se desinstaló R2RETRO.";
        return false;
    };
    if(path.empty() || path.front()!='/' || path.size()>1800 || path.find('\0')!=std::string::npos)
        return pathFailure("ruta PKG inválida",path);
    const std::atomic<bool> notCancelled{false};
    auto validate=[&](int fd,const std::string& name) {
        struct stat info{};
        if(::fstat(fd,&info)) return pathFailure("fstat PKG",name,errno);
        if(!S_ISREG(info.st_mode)) return pathFailure("PKG no regular",name);
        if(info.st_size<0 || uint64_t(info.st_size)!=release.size)
            return pathFailure("tamaño PKG distinto del manifiesto",name);
        std::string check;
        if(!verifyUpdatePackage(fd,release,notCancelled,check))
            return pathFailure("verificación PKG: "+check,name);
        return true;
    };
    original.fd=::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(original.fd<0) return pathFailure("abrir PKG",path,errno);
    if(!validate(original.fd,path)) return false;
    std::string systemPath=path;
    if(path.compare(0,6,"/data/")==0) {
        const auto candidate="/user"+path;
        alias.fd=::open(candidate.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(alias.fd>=0) {
            if(!validate(alias.fd,candidate)) return false;
            systemPath=candidate;
        } else {
            const int code=errno;
            // If the global alias isn't exposed to this process, let the
            // system APIs evaluate the already verified original path. A
            // symlink (ELOOP) or I/O failure is not an absent alias.
            if(code!=ENOENT && code!=ENOTDIR && code!=EACCES)
                return pathFailure("abrir alias PKG",candidate,code);
        }
    }
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL);
    static bool appReady=false;
    int32_t rc=0;
    if(!appReady) { rc=sceAppInstUtilInitialize(); if(rc) return failed("AppInstUtil",rc); appReady=true; }
    char title[32]{}; int32_t isApp=0;
    rc=sceAppInstUtilGetTitleIdFromPkg(systemPath.c_str(),title,&isApp);
    if(rc) return failed(("identidad en "+systemPath).c_str(),rc);
    if(std::strncmp(title,UpdateTitleId,sizeof(title))!=0 || !isApp) return failed("Title ID",-1);
    int32_t updating=0;
    rc=sceAppInstUtilAppIsInUpdating(UpdateTitleId,&updating);
    if(rc || updating) return failed("actualización ya activa o consulta fallida",rc?rc:-1);
    // Self-update: direct AppInstUtil overwrite (flatz's primary method). BGFT
    // registration returned 0x80990088 no matter the id/option, so install the
    // PKG directly instead of queuing a background download task. This moves
    // the file to /user/app/<title id>/app.pkg without a second BGFT copy.
    rc=sceAppInstUtilAppPrepareOverwritePkg(systemPath.c_str());
    if(rc) return failed("preparar sobreescritura",rc);
    rc=sceAppInstUtilAppInstallPkg(systemPath.c_str(),nullptr);
    if(rc) return failed("instalar paquete",rc);
    error.clear(); return true;
#else
    (void)path; (void)release;
    error="PKG verificado. La instalación solo está disponible en PS4.";
    return false;
#endif
}
}
