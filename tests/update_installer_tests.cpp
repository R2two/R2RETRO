#include "update.h"
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
namespace {
int failStage=0, registrations=0, starts=0, unregisters=0;
constexpr int32_t failure=static_cast<int32_t>(0x80990088);
}
extern "C" {
uint32_t sceSysmoduleLoadModuleInternal(OrbisSysModuleInternal) { return 0; }
int32_t sceAppInstUtilInitialize(){return 0;}
int32_t sceAppInstUtilGetTitleIdFromPkg(const char*,char* title,int32_t* app) {
    std::strcpy(title,failStage==1?"OTHER0000":r2n64::UpdateTitleId); *app=1; return 0;
}
int32_t sceAppInstUtilAppIsInUpdating(const char*,int32_t* value){ *value=failStage==2?1:0;return 0; }
int32_t sceAppInstUtilGetPrimaryAppSlot(const char*,int32_t* value){*value=2;return failStage==3?failure:0;}
int32_t sceUserServiceGetForegroundUser(int32_t* value){*value=7;return failStage==4?failure:0;}
int32_t sceBgftServiceIntInit(OrbisBgftInitParams* p){assert(p->heap&&p->heapSize==1024*1024);return 0;}
int32_t sceBgftServiceIntDownloadRegisterTaskByStorageEx(OrbisBgftDownloadParamEx* p,OrbisBgftTaskId* id) {
    ++registrations;
    assert(p->slot==2 && p->params.userId==7 && p->params.entitlementType==5);
    assert(p->params.option==ORBIS_BGFT_TASK_OPT_FORCE_UPDATE);
    assert(std::strcmp(p->params.id,r2n64::UpdateContentId)==0);
    assert(std::strcmp(p->params.packageType,"PS4GD")==0);
    uint64_t size=0; std::memcpy(&size,&p->params.packageSize,sizeof(size)); assert(size==4096);
    *id=42;return failStage==5?failure:0;
}
int32_t sceBgftServiceDownloadStartTask(OrbisBgftTaskId id){ assert(id==42);++starts;return failStage>=6?failure:0; }
int32_t sceBgftServiceIntDownloadUnregisterTask(OrbisBgftTaskId id){assert(id==42);++unregisters;return failStage==7?failure:0;}
// Deliberately no uninstall function stub: adding an uninstall dependency must fail the link.
}
int main() {
    char path[]="/tmp/r2retro-install-XXXXXX";
    int fd=::mkstemp(path); assert(fd>=0); assert(::ftruncate(fd,4096)==0); ::close(fd);
    r2n64::UpdateRelease r; r.version="0.5.4"; r.size=4096; std::string error;
    for(failStage=1;failStage<=7;++failStage) {
        int old=starts;
        assert(!r2n64::queueUpdateInstall(path,r,error)); assert(!error.empty());
        if(failStage<=5) assert(starts==old);
    }
    assert(unregisters==2);
    failStage=0; assert(r2n64::queueUpdateInstall(path,r,error)); assert(error.empty());
    assert(registrations==4 && starts==3);
    ::unlink(path);
    assert(!r2n64::queueUpdateInstall(path,r,error));
    std::cout<<"PASS: PS4 installer adapter (mock ABI, identity, slot/user, conflict, start failure, cleanup, successful handoff; no hardware)\n";
}
