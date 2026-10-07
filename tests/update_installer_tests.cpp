#include "update.h"
#include <orbis/AppInstUtil.h>
#include <orbis/Bgft.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
namespace {
int failStage=0, registrations=0, starts=0, unregisters=0;
int aliasError=ENOENT, verifications=0;
std::string originalFile, aliasFile, expectedPath;
constexpr const char* localPath="/data/r2retro-installer-test.pkg";
constexpr const char* globalPath="/user/data/r2retro-installer-test.pkg";
constexpr int32_t failure=static_cast<int32_t>(0x80990088);
}
namespace r2n64 {
// Adapter unit double only: real SHA-256/SFO/identity parsing is covered by
// update_tests.cpp. Distinct files must both pass the verification contract.
bool verifyUpdatePackage(int fd,const UpdateRelease&,const std::atomic<bool>& cancel,std::string& error) {
    ++verifications;
    unsigned char marker=1;
    if(cancel || ::pread(fd,&marker,1,0)!=1 || marker!=0) {
        error="contenido de prueba alterado"; return false;
    }
    error.clear(); return true;
}
}
extern "C" {
int __real_open(const char*,int,...);
int __wrap_open(const char* path,int flags,...) {
    // Only the adapter's read-only opens are wrapped; mkstemp creates fixtures.
    assert(!(flags&O_CREAT));
    if(std::strcmp(path,localPath)==0) return __real_open(originalFile.c_str(),flags);
    if(std::strcmp(path,globalPath)==0) {
        if(aliasError) {errno=aliasError;return -1;}
        return __real_open(aliasFile.c_str(),flags);
    }
    return __real_open(path,flags);
}
uint32_t sceSysmoduleLoadModuleInternal(OrbisSysModuleInternal) { return 0; }
int32_t sceAppInstUtilInitialize(){return 0;}
int32_t sceAppInstUtilGetTitleIdFromPkg(const char* path,char* title,int32_t* app) {
    assert(path==expectedPath);
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
    expectedPath=path;
    r2n64::UpdateRelease r; r.version="99.0.0"; r.size=4096; std::string error;
    for(failStage=1;failStage<=7;++failStage) {
        int old=starts;
        assert(!r2n64::queueUpdateInstall(path,r,error)); assert(!error.empty());
        if(failStage<=5) assert(starts==old);
    }
    assert(unregisters==2);
    failStage=0; assert(r2n64::queueUpdateInstall(path,r,error)); assert(error.empty());
    assert(registrations==4 && starts==3);
    originalFile=path;
    expectedPath=localPath;
    for(int code : {ENOENT,ENOTDIR,EACCES}) {
        aliasError=code; const int before=verifications;
        assert(r2n64::queueUpdateInstall(localPath,r,error));
        assert(verifications==before+1 && error.empty());
    }
    char second[]="/tmp/r2retro-alias-XXXXXX";
    fd=::mkstemp(second); assert(fd>=0); assert(::ftruncate(fd,4096)==0);
    aliasFile=second; aliasError=0; expectedPath=globalPath;
    struct stat a{}, b{}; assert(::stat(path,&a)==0 && ::fstat(fd,&b)==0);
    assert(a.st_ino!=b.st_ino || a.st_dev!=b.st_dev);
    const int before=verifications;
    assert(r2n64::queueUpdateInstall(localPath,r,error));
    assert(verifications==before+2 && error.empty());
    const int queued=registrations;
    const unsigned char changed=1;
    assert(::pwrite(fd,&changed,1,0)==1);
    assert(!r2n64::queueUpdateInstall(localPath,r,error));
    assert(error.find("verificación PKG")!=std::string::npos);
    assert(registrations==queued);
    assert(::ftruncate(fd,17)==0);
    assert(!r2n64::queueUpdateInstall(localPath,r,error));
    assert(error.find("tamaño PKG")!=std::string::npos);
    ::close(fd); assert(::unlink(second)==0);
    assert(::symlink(path,second)==0);
    assert(!r2n64::queueUpdateInstall(localPath,r,error));
    assert(error.find("abrir alias PKG")!=std::string::npos && error.find("errno=")!=std::string::npos);
    assert(::unlink(second)==0);
    aliasError=EIO;
    assert(!r2n64::queueUpdateInstall(localPath,r,error));
    assert(error.find("abrir alias PKG")!=std::string::npos);
    aliasError=ENOENT;
    assert(!r2n64::queueUpdateInstall("relative.pkg",r,error));
    assert(error.find("ruta PKG inválida")!=std::string::npos);
    assert(!r2n64::queueUpdateInstall(std::string("/tmp/a\0b",8),r,error));
    assert(!r2n64::queueUpdateInstall("/tmp",r,error));
    assert(error.find("PKG no regular")!=std::string::npos);
    fd=__real_open(path,O_WRONLY); assert(fd>=0);
    assert(::pwrite(fd,&changed,1,0)==1); ::close(fd);
    assert(!r2n64::queueUpdateInstall(localPath,r,error));
    assert(error.find("verificación PKG")!=std::string::npos);
    assert(registrations==queued);
    ::unlink(path);
    assert(!r2n64::queueUpdateInstall(path,r,error));
    assert(error.find("abrir PKG")!=std::string::npos && error.find("errno=")!=std::string::npos);
    assert(error.find("0xFFFFFFFF")==std::string::npos);
    std::cout<<"PASS: PS4 installer adapter (mock verification/ABI, alias integrity, local errors, identity, slot/user, conflict, start failure, cleanup, handoff; no hardware)\n";
}
