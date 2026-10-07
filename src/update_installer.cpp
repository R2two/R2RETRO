#include "update.h"
#ifdef R2N64_PS4
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#endif

namespace r2n64 {
bool queueUpdateInstall(const std::string& path, const UpdateRelease& release, std::string& error) {
#ifdef R2N64_PS4
    auto failed=[&](const char* stage,int32_t code) {
        char message[180];
        std::snprintf(message,sizeof(message),"Instalador PS4: %s (0x%08X). PKG conservado; no se desinstaló R2RETRO.",stage,unsigned(code));
        error=message; return false;
    };
    // System storage tasks use the global /user/data alias. Verify it describes
    // the very same file, never substitute an unrelated path or HTTP server.
    std::string systemPath=path;
    if(path.compare(0,6,"/data/")==0) systemPath="/user"+path;
    struct stat a{},b{};
    if(path.size()>1800 || ::lstat(path.c_str(),&a) || ::lstat(systemPath.c_str(),&b) ||
        !S_ISREG(a.st_mode) || !S_ISREG(b.st_mode) || a.st_dev!=b.st_dev || a.st_ino!=b.st_ino ||
        a.st_size<0 || uint64_t(a.st_size)!=release.size)
        return failed("ruta local",-1);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_APP_INST_UTIL);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_BGFT);
    static bool appReady=false;
    int32_t rc=0;
    if(!appReady) { rc=sceAppInstUtilInitialize(); if(rc) return failed("AppInstUtil",rc); appReady=true; }
    char title[32]{}; int32_t isApp=0;
    rc=sceAppInstUtilGetTitleIdFromPkg(systemPath.c_str(),title,&isApp);
    if(rc) return failed("identidad",rc);
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
    p.params.icon=""; p.params.option=ORBIS_BGFT_TASK_OPT_FORCE_UPDATE;
    p.params.scenario="0"; p.params.type="PS4GD"; p.params.subtype="";
    p.params.size=release.size; p.slot=uint32_t(slot);
    OrbisBgftTaskId task=-1;
    rc=sceBgftServiceIntDownloadRegisterTaskByStorageEx(reinterpret_cast<OrbisBgftDownloadParamEx*>(&p),&task);
    // Never copy upstream's uninstall/retry path: self-update must preserve data.
    if(rc || task<0) return failed("registrar reemplazo",rc?rc:-1);
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
