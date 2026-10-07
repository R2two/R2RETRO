#include "update.h"
#include "update_sha256.h"
#include "http.h"
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <vector>
using namespace r2n64;
namespace {
std::vector<uint8_t> package;
std::string manifest;
bool networkFailure=false;
bool missingManifest=false;
std::atomic<bool> stallQuery{false}, enteredQuery{false};
std::atomic<unsigned> installs{0};
void check(bool ok,const std::string& reason) { if(!ok) throw std::runtime_error(reason); }
void be(std::vector<uint8_t>& b,size_t p,uint32_t v) { for(unsigned i=0;i<4;++i) b[p+i]=uint8_t(v>>(24-i*8)); }
void le(std::vector<uint8_t>& b,size_t p,uint32_t v) { for(unsigned i=0;i<4;++i) b[p+i]=uint8_t(v>>(i*8)); }
std::string hash(const std::vector<uint8_t>& b) { UpdateSha256 h; h.add(b.data(),b.size()); return h.finish(); }
std::vector<uint8_t> fixture() {
    std::vector<uint8_t> b(4096,0); std::memcpy(b.data(),"\x7f" "CNT",4);
    std::memcpy(b.data()+0x40,UpdateContentId,36);
    be(b,0x10,1); be(b,0x18,0x800); be(b,0x74,0x1a); be(b,0x434,b.size());
    be(b,0x800,0x1000); be(b,0x810,0x900); be(b,0x814,512);
    std::vector<uint8_t> s(512,0); std::memcpy(s.data(),"\0PSF",4);
    le(s,4,0x101); le(s,8,100); le(s,12,200); le(s,16,4);
    const std::pair<std::string,std::string> fields[]={{"TITLE_ID",UpdateTitleId},{"CONTENT_ID",UpdateContentId},{"APP_VER","99.00"},{"CATEGORY","gd"}};
    size_t k=100,d=200;
    for(size_t i=0;i<4;++i) {
        const auto& f=fields[i]; auto p=20+i*16;
        le(s,p,uint32_t(k-100)|0x02040000); le(s,p+4,f.second.size()+1);
        le(s,p+8,f.second.size()+1); le(s,p+12,d-200);
        std::memcpy(s.data()+k,f.first.c_str(),f.first.size()+1); k+=f.first.size()+1;
        std::memcpy(s.data()+d,f.second.c_str(),f.second.size()+1); d+=f.second.size()+1;
    }
    std::copy(s.begin(),s.end(),b.begin()+0x900); return b;
}
UpdateResult wait(UpdateService& s) {
    UpdateResult result;
    for(unsigned i=0;i<500;++i) { if(s.poll(result)) return result; std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
    throw std::runtime_error("worker timeout");
}
}
namespace r2n64 {
bool httpGet(const std::string& url,size_t,const std::string&,const std::atomic<bool>& cancel,std::vector<uint8_t>& body,std::string& error,const HttpOptions& options) {
    options.report(HttpStage::DnsConnect); enteredQuery=true;
    while(stallQuery && !cancel) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if(cancel) { error="Descarga cancelada."; return false; }
    if(missingManifest) { error="Archivo no encontrado (HTTP 404)."; return false; }
    check(url==updateManifestUrl(true),"wrong update channel"); body.assign(manifest.begin(),manifest.end()); return true;
}
bool httpDownload(const std::string&,uint64_t,const std::string&,const std::atomic<bool>& cancel,int fd,std::atomic<uint64_t>& received,std::string& error,const HttpOptions& options) {
    options.report(HttpStage::Receiving);
    if(cancel) { error="cancelada"; return false; }
    const auto n=networkFailure?100:package.size();
    check(::write(fd,package.data(),n)==ssize_t(n),"fixture write"); received=n;
    if(networkFailure) { error="network failure"; return false; } return true;
}
bool queueUpdateInstall(const std::string&,const UpdateRelease&,std::string&) { ++installs; return true; }
}
int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--manifest") {
            std::ifstream file(argv[2]); std::string text((std::istreambuf_iterator<char>(file)),{}),error;
            UpdateRelease release; check(parseUpdateManifest(text,release,error),error);
            std::cout<<"PASS: production manifest "<<release.version<<'\n'; return 0;
        }
        if(argc==5) {
            UpdateRelease r; r.version="0.5.2"; r.sfo=argv[3]; r.sha256=argv[4];
            r.size=std::filesystem::file_size(argv[2]); std::atomic<bool> cancel{false}; std::string error;
            const int fd=::open(argv[2],O_RDONLY);
            const bool ok=verifyUpdatePackage(fd,r,cancel,error); ::close(fd); check(ok,error);
            std::cout<<"PASS: real PKG SHA-256, identity and embedded SFO\n"; return 0;
        }
        UpdateSha256 h;
        check(h.finish()=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","empty SHA");
        h=UpdateSha256{}; for(char c:std::string("abc")) h.add(reinterpret_cast<uint8_t*>(&c),1);
        check(h.finish()=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","abc SHA");
        h=UpdateSha256{}; std::array<uint8_t,1000> a; a.fill('a');
        for(unsigned i=0;i<1000;++i) h.add(a.data(),a.size());
        check(h.finish()=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","million a SHA");
        package=fixture();
        manifest="R2RETRO-UPDATE-1\nversion=99.0.0\nsfo=99.00\nchannel=experimental\nurl=https://github.com/R2two/R2RETRO/releases/download/v99.0.0/R2RETRO-test.pkg\nsha256="+hash(package)+
            "\nsize=4096\nnotes=Prueba sintética, no instalable.\ntitle_id="+UpdateTitleId+"\ncontent_id="+UpdateContentId+"\n";
        std::string error; UpdateRelease r;
        check(parseUpdateManifest(manifest,r,error),error);
        check(newerUpdate(r,"0.5.3","00.53") && !newerUpdate(r,"99.0.0","99.00") && !newerUpdate(r,"99.1.0","99.01"),"version ordering");
        for(const auto& change:std::vector<std::pair<std::string,std::string>>{
            {"size=4096","size=536870913"},{"size=4096","size=-1"},{"sfo=99.00","sfo=0.54"},
            {"version=99.0.0","version=99.0.0."},{"version=99.0.0","version=099.0.0"},
            {"github.com/","github.com.evil/"},{"R2RETRO-test.pkg","../bad.pkg"},
            {"R2RETRO-test.pkg","test.pkg?evil"},{"RNTD00064","OTHER0000"},{"experimental","unknown"}}) {
            auto bad=manifest; bad.replace(bad.find(change.first),change.first.size(),change.second);
            UpdateRelease rejected; check(!parseUpdateManifest(bad,rejected,error),"malformed manifest accepted: "+change.first);
        }
        UpdateRelease rejected;
        check(!parseUpdateManifest(manifest+"version=99.0.0\n",rejected,error),"duplicate field");
        check(!parseUpdateManifest(manifest+std::string(4096,'x'),rejected,error),"oversize manifest");
        char temp[]="/tmp/r2retro-update-XXXXXX"; check(::mkdtemp(temp)!=nullptr,"tmpdir");
        const std::string root=temp;
        struct Cleanup { std::string root; ~Cleanup(){std::filesystem::remove_all(root);} } cleanup{root};
        std::filesystem::create_directory(root+"/configs");
        check(saveUpdatePreferences(root,{false,false},error),error);
        auto p=loadUpdatePreferences(root); check(!p.automatic&&!p.experimental,"preferences persistence");
        { std::ofstream old(root+"/configs/updates.conf"); old<<"2010\n"; }
        p=loadUpdatePreferences(root); check(!p.automatic && p.experimental,"v2 migration changed preferences");
        check(saveUpdatePreferences(root,p,error),error);
        { std::ifstream saved(root+"/configs/updates.conf"); std::string line; std::getline(saved,line);
          check(line=="2011","legacy system DNS byte not retired"); }
        { std::ofstream old(root+"/configs/updates.conf"); old<<"100\n"; }
        p=loadUpdatePreferences(root); check(!p.automatic&&!p.experimental,"v1 migration changed preferences");
        { std::ofstream bad(root+"/configs/updates.conf"); bad<<"200x\n"; }
        p=loadUpdatePreferences(root); check(p.automatic&&p.experimental,"invalid v2 not rejected");
        std::atomic<bool> cancel{false}; std::atomic<uint64_t> received{0}; std::string path;
        check(downloadUpdate(root,"ca",r,cancel,received,path,error),error);
        check(received==4096 && std::filesystem::exists(path),"download missing");
        int fd=::open(path.c_str(),O_RDONLY); check(verifyUpdatePackage(fd,r,cancel,error),error); ::close(fd);
        for(const auto& mutation:std::vector<std::pair<size_t,uint8_t>>{{0,0},{0x40,'X'},{0x436,0},
            {0x18,255},{0x10,255},{0x814,255},{0x910,255},{0x974,255}}) {
            auto bad=package; bad[mutation.first]=mutation.second;
            // Change hash too: structural verification must reject even correctly hashed bad files.
            UpdateRelease badRelease=r; badRelease.sha256=hash(bad);
            std::ofstream f(root+"/bad.pkg",std::ios::binary); f.write(reinterpret_cast<char*>(bad.data()),bad.size()); f.close();
            fd=::open((root+"/bad.pkg").c_str(),O_RDONLY);
            check(!verifyUpdatePackage(fd,badRelease,cancel,error),"malformed PKG accepted at "+std::to_string(mutation.first)); ::close(fd);
        }
        fd=::open(path.c_str(),O_RDONLY); auto wrong=r; wrong.sha256=std::string(64,'0');
        check(!verifyUpdatePackage(fd,wrong,cancel,error),"hash mismatch accepted");
        wrong=r; wrong.sfo="00.56"; check(!verifyUpdatePackage(fd,wrong,cancel,error),"SFO mismatch accepted");
        cancel=true; check(!verifyUpdatePackage(fd,r,cancel,error),"cancel ignored"); cancel=false; ::close(fd);
        UpdateService service;
        check(service.start(UpdateJob::Check,root,"ca",true),"start check");
        check(!service.start(UpdateJob::Check,root,"ca",true),"concurrent worker");
        check(wait(service).ok,"check result");
        missingManifest=true;
        check(service.start(UpdateJob::Check,root,"ca",true),"check start");
        auto result=wait(service);
        check(!result.ok && result.error.find("manifiesto publicado")!=std::string::npos,"404 not distinguished from connection failure");
        missingManifest=false; stallQuery=true; enteredQuery=false;
        check(service.start(UpdateJob::Check,root,"ca",true),"stalled check start");
        for(unsigned i=0;i<500 && !enteredQuery;++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        check(enteredQuery && service.stage()==HttpStage::DnsConnect,"worker stage not exposed");
        service.cancel(); result=wait(service); stallQuery=false;
        check(result.cancelled&&!result.ok&&!service.busy(),"cancelled worker not joined");
        check(service.start(UpdateJob::Check,root,"ca",true),"retry after cancel");
        check(wait(service).ok,"retry failed");
        check(service.start(UpdateJob::Install,root,"ca",true,r),"install start");
        check(wait(service).ok&&installs==1,"verified handoff");
        wrong=r; wrong.version="0.5.3"; wrong.sfo="00.53";
        check(service.start(UpdateJob::Install,root,"ca",true,wrong),"old start");
        check(!wait(service).ok&&installs==1,"downgrade installed");
        std::filesystem::remove(path); networkFailure=true;
        check(!downloadUpdate(root,"ca",r,cancel,received,path,error),"failed network promoted");
        check(std::filesystem::is_empty(root+"/updates"),"partial file left after failure");
        std::cout<<"PASS: SHA vectors, manifest, downgrade, bounded PKG/SFO, corruption, cancel, worker, preferences and failed transfer cleanup\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
