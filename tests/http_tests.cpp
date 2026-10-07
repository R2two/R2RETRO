#include "http.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <fcntl.h>
#include <unistd.h>

namespace {
void require(bool condition, const std::string& error) {
    if (!condition) throw std::runtime_error(error);
}
}

int main(int argc, char** argv) {
    try {
        if((argc==6 || argc==7) && std::string(argv[1])=="--doh-smoke") {
            std::atomic<bool> cancel{false}; std::atomic<r2n64::HttpStage> stage{r2n64::HttpStage::Idle};
            r2n64::HttpOptions options; options.useSystemDns=false; options.dohEndpoint=argv[4]; options.stage=&stage;
            if(argc==7) options.dohBootstrap=argv[6];
            std::vector<uint8_t> bytes; std::string error;
            const std::string expected=argv[5];
            const auto started=std::chrono::steady_clock::now();
            std::thread cancellation;
            if(expected=="cancel") cancellation=std::thread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(200));cancel=true;});
            const bool ok=r2n64::httpGet(argv[2],4096,argv[3],cancel,bytes,error,options);
            if(cancellation.joinable()) cancellation.join();
            if(expected=="ok") require(ok && !bytes.empty(),error);
            else {
                require(!ok && bytes.empty() && !error.empty(),"DoH failure exposed body or succeeded");
                if(expected=="404") require(error.find("404")!=std::string::npos,error);
                if(expected=="cancel") require(error.find("cancelada")!=std::string::npos &&
                    std::chrono::steady_clock::now()-started<std::chrono::seconds(5),"DoH cancellation unbounded");
            }
            std::cout<<"PASS: DoH "<<expected<<"; "<<r2n64::httpStageName(stage)<<"; "<<error<<'\n';
            r2n64::httpShutdown(); return 0;
        }
        if (argc == 6 && std::string(argv[1]) == "--download-smoke") {
            std::string error; std::atomic<bool> cancel{false}; std::atomic<uint64_t> bytes{0};
            const int fd=::open(argv[5],O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
            require(fd>=0,"Download smoke destination must not already exist");
            const bool ok=r2n64::httpDownload(argv[2],std::stoull(argv[3]),argv[4],cancel,fd,bytes,error);
            ::close(fd); require(ok,error);
            std::cout<<"PASS: HTTPS stream received "<<bytes<<" bytes\n";
            r2n64::httpShutdown(); return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "--smoke") {
            std::vector<uint8_t> bytes; std::string error; std::atomic<bool> cancel{false};
            require(r2n64::httpGet(argv[2], 4 * 1024 * 1024, argv[3], cancel, bytes, error), error);
            require(!bytes.empty(), "Empty HTTPS smoke body");
            std::cout << "PASS: verified HTTPS; received " << bytes.size() << " bytes\n";
            r2n64::httpShutdown(); return 0;
        }
        require(argc == 5, "Usage: http_tests https://localhost:port test-ca.pem public-ca.pem ca-fixtures-directory");
        const std::string base = argv[1], testCa = argv[2], publicCa = argv[3];
        std::atomic<bool> cancel{false};
        std::vector<uint8_t> body;
        std::string error;
        unsigned checks = 0;
        auto get = [&](const std::string& suffix, size_t limit = 4096) {
            body = {99, 88}; error = "stale";
            const bool ok = r2n64::httpGet(base + suffix, limit, testCa, cancel, body, error);
            if (ok) require(error.empty(), "Successful request retained old error");
            else require(body.empty() && !error.empty(), "Failed request exposed partial/stale body");
            ++checks; return ok;
        };
        for (const std::string& url : std::vector<std::string>{"http://localhost/", "file:///etc/passwd", "https:///missing", "https://user:password@localhost/", "https://localhost/has space", std::string("https://localhost/\0tail", 23)}) {
            body = {1};
            require(!r2n64::httpGet(url, 20, testCa, cancel, body, error) && body.empty(), "Unsafe URL accepted");
            ++checks;
        }
        require(!get("/ok", 0), "Zero limit accepted");
        cancel = true; require(!get("/ok") && error.find("cancelada") != std::string::npos, "Pre-cancel ignored"); cancel = false;
        const auto fixtures = std::filesystem::path(argv[4]);
        const auto validCa = (fixtures / "valid.pem").string();
        const auto getWithCa = [&](const std::string& path) {
            body = {11, 22}; error = "stale CA error";
            const bool ok = r2n64::httpGet(base + "/ok", 20, path, cancel, body, error);
            require(ok ? error.empty() : body.empty() && !error.empty(), "CA check retained stale error/body");
            ++checks;
            return ok;
        };
        for (const auto& entry : std::vector<std::pair<std::string, std::string>>{
                {testCa + ".missing", "open"}, {"", "ruta"}, {testCa + std::string("\0ignored", 8), "ruta"},
                {(fixtures / "empty.pem").string(), "vacio"}, {(fixtures / "directory.pem").string(), "tipo"},
                {(fixtures / "link.pem").string(), "open"}, {(fixtures / "fifo.pem").string(), "tipo"},
                {(fixtures / "oversized.pem").string(), "tamano"}}) {
            const auto started = std::chrono::steady_clock::now();
            require(!getWithCa(entry.first), "Unsafe or unavailable CA was accepted");
            require(std::chrono::steady_clock::now() - started < std::chrono::seconds(2),
                    "CA validation blocked, including a FIFO with no writer");
            require(error.find("(" + entry.second + ", errno ") != std::string::npos,
                    "Local CA failure did not identify its stage and numeric errno: " + error);
            const auto number = error.find("errno ") + 6;
            require(number < error.size() && error[number] >= '0' && error[number] <= '9', "CA errno is not numeric");
            require(error.find(fixtures.string()) == std::string::npos && error.find(testCa) == std::string::npos,
                    "CA diagnostic exposed a local path");
            require(getWithCa(validCa) && body == std::vector<uint8_t>({0,1,2,3,4,5,6,255}),
                    "A valid CA in a spaced path failed after local validation rejection");
        }
        require(!getWithCa((fixtures / "malformed.pem").string()) && error.find("CA") != std::string::npos &&
                error.find("curl ") != std::string::npos, "Malformed PEM did not reach and fail curl certificate loading");
        require(getWithCa(validCa) && body.size() == 8, "Valid CA failed after malformed PEM");
        require(getWithCa((fixtures / "boundary.pem").string()) && body.size() == 8,
                "A valid PEM bundle at the 2 MiB boundary was rejected");
        require(!r2n64::httpGet(base + "/ok", 20, publicCa, cancel, body, error) && error.find("TLS") != std::string::npos, "Untrusted certificate accepted"); ++checks;
        std::string wrongHost = base; wrongHost.replace(wrongHost.find("localhost"), 9, "127.0.0.1");
        require(!r2n64::httpGet(wrongHost + "/ok", 20, testCa, cancel, body, error) && error.find("TLS") != std::string::npos, "Wrong certificate hostname accepted"); ++checks;
        require(get("/ok", 8) && body == std::vector<uint8_t>({0, 1, 2, 3, 4, 5, 6, 255}), "HTTPS content changed");
        // Exercise the PKG streaming path against the same trusted local TLS
        // fixture: redirects, truncated bodies, limits, cancellation and disk errors.
        std::atomic<uint64_t> streamBytes{0};
        const std::string streamPath=(fixtures/"stream.pkg").string();
        auto stream=[&](const std::string& suffix,uint64_t size,const std::string& ca) {
            const int fd=::open(streamPath.c_str(),O_RDWR|O_CREAT|O_TRUNC,0600);
            require(fd>=0,"stream file");
            const bool ok=r2n64::httpDownload(base+suffix,size,ca,cancel,fd,streamBytes,error);
            require(uint64_t(::lseek(fd,0,SEEK_END))<=size,"stream exceeded limit");
            ::close(fd); ++checks; return ok;
        };
        require(stream("/ok",8,testCa)&&streamBytes==8,"streamed HTTPS failed");
        require(stream("/redirect",8,testCa)&&streamBytes==8,"stream redirect body contaminated PKG");
        require(!stream("/ok",7,testCa),"stream size limit ignored");
        require(!stream("/ok",9,testCa),"stream shorter than manifest accepted");
        require(!stream("/truncated",128,testCa),"stream truncated accepted");
        require(!stream("/chunked",4096,testCa),"chunked overflow accepted");
        require(!stream("/downgrade",8,testCa),"stream TLS downgrade accepted");
        require(!stream("/ok",8,publicCa),"stream untrusted TLS accepted");
        cancel=true; require(!stream("/ok",8,testCa),"stream pre-cancel ignored"); cancel=false;
        {
            const int fd=::open(streamPath.c_str(),O_RDONLY|O_TRUNC);
            require(!r2n64::httpDownload(base+"/ok",8,testCa,cancel,fd,streamBytes,error),"stream read-only destination accepted");
            ::close(fd);
        }
        require(get("/empty") && body.empty(), "Valid empty response rejected");
        require(get("/redirect") && body.size() == 8, "HTTPS redirect failed or retained redirect body");
        require(!get("/downgrade") && error.find("HTTPS") != std::string::npos, "HTTP downgrade accepted");
        require(!get("/loop") && error.find("redirecciones") != std::string::npos, "Redirect loop unbounded");
        for (const char* status : {"404", "403", "500"}) {
            require(!get(std::string("/status/") + status) && error.find(status) != std::string::npos, "HTTP status not reported");
        }
        require(!get("/ok", 7) && error.find("limite") != std::string::npos, "Content-Length limit ignored");
        require(!get("/chunked", 4096) && error.find("limite") != std::string::npos, "Chunked streaming limit ignored");
        require(!get("/truncated") && error.find("completo") != std::string::npos, "Truncated body accepted");
        bool downloaded = false;
        std::thread worker([&] { downloaded = get("/slow", 1024 * 1024); });
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); cancel = true; worker.join();
        require(!downloaded && error.find("cancelada") != std::string::npos, "Active cancellation ignored"); cancel = false;
        require(!get("/stall") && error.find("tiempo") != std::string::npos, "Low-speed timeout ignored");
        require(get("/ok") && body.size() == 8, "Request failed after timeout/cancel");
        r2n64::httpShutdown();
        require(get("/ok") && body.size() == 8, "Request failed after shutdown/reinitialize");
        r2n64::httpShutdown();
        std::cout << "PASS: " << checks << " HTTPS cases; CA file diagnostics, TLS peer/host, limits, redirects, errors, cancellation and timeout\n";
    } catch (const std::exception& failure) {
        std::cerr << "FAIL: " << failure.what() << '\n';
        r2n64::httpShutdown(); return 1;
    }
}
