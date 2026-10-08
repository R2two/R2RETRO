#include "update.h"
#ifdef R2N64_PS4
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
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
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT);
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
    int32_t slot=0,user=-1;
    rc=sceAppInstUtilGetPrimaryAppSlot(UpdateTitleId,&slot);
    if(rc || slot<0) return failed("slot instalado",rc?rc:-1);
    rc=sceUserServiceGetForegroundUser(&user);
    if(rc || user<0) return failed("usuario activo",rc?rc:-1);
    static OrbisBgftInitParams init{};
    static bool ready=false;
    if(!ready) {
        init.heapSize=1024*1024; init.heap=std::calloc(1,init.heapSize);
        if(!init.heap) return failed("memoria BGFT",-1);
        rc=sceBgftServiceIntInit(&init);
        if(rc) { std::free(init.heap); init={}; return failed("iniciar BGFT",rc); }
        ready=true;
    }
    // OpenOrbis declares a 32-bit size followed by padding; the ABI uses 64.
    struct Params {
        int32_t userId,entitlementType;
        const char *id,*url,*extra,*name,*icon,*sku;
        OrbisBgftTaskOpt option;
        const char *scenario,*date,*type,*subtype;
        uint64_t size;
    };
    struct Extended { Params params; uint32_t slot; } p{};
    static_assert(sizeof(Params)==sizeof(OrbisBgftDownloadParam),"BGFT ABI");
    static_assert(offsetof(Params,size)==offsetof(OrbisBgftDownloadParam,packageSize),"BGFT size ABI");
    static_assert(sizeof(Extended)==sizeof(OrbisBgftDownloadParamEx),"BGFT storage ABI");
    static_assert(offsetof(Extended,slot)==offsetof(OrbisBgftDownloadParamEx,slot),"BGFT slot ABI");
    const auto name="R2RETRO "+release.version;
    p.params.userId=user; p.params.entitlementType=5;
    p.params.id=UpdateContentId; p.params.url=systemPath.c_str(); p.params.name=name.c_str();
    // BGFT reads these C-string fields; never pass NULL where the ABI expects a
    // string. contentExUrl/skuId/releaseDate were previously left unset (NULL).
    p.params.extra=""; p.params.icon=""; p.params.sku="";
    // FORCE_UPDATE forces a same-version reinstall and made BGFT report
    // "content already exists" (0x80990088) during a normal version upgrade.
    // Register a normal update: the higher SFO version selects the update path.
    p.params.option=ORBIS_BGFT_TASK_OPT_NONE;
    p.params.scenario="0"; p.params.date=""; p.params.type="PS4GD"; p.params.subtype="";
    p.params.size=release.size; p.slot=uint32_t(slot);
    OrbisBgftTaskId task=-1;
    rc=sceBgftServiceIntDownloadRegisterTaskByStorageEx(reinterpret_cast<OrbisBgftDownloadParamEx*>(&p),&task);
    // Never copy upstream's uninstall/retry path: self-update must preserve data.
    if(rc || task<0) {
        const int32_t code=rc?rc:-1;
        // 0x80990088 / 0x80990015 = content already exists (installed app or a
        // stale BGFT task). Surface an actionable message instead of a bare code.
        if(code==static_cast<int32_t>(0x80990088) || code==static_cast<int32_t>(0x80990015)) {
            error="Instalador PS4: conflicto con contenido existente (0x80990088). Revisa Notificaciones → Descargas o cancela una actualización previa de R2RETRO. PKG conservado; no se desinstaló R2RETRO.";
            return false;
        }
        return failed("registrar reemplazo",code);
    }
    rc=sceBgftServiceDownloadStartTask(task);
    if(rc) {
        const int32_t cleanup=sceBgftServiceIntDownloadUnregisterTask(task);
        if(cleanup) return failed("inicio fallido; revisar tarea en Descargas",rc);
        return failed("iniciar instalación",rc);
    }
    error.clear(); return true;
#else
    (void)path; (void)release;
    error="PKG verificado. La instalación solo está disponible en PS4.";
    return false;
#endif
}
}
