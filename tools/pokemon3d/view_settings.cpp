#include "view_settings.h"
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace pokemon3d {
namespace fs=std::filesystem;
namespace {
fs::path settingsPath(const fs::path& data,bool create) {
    const auto folder=data/"configs";
    const auto status=fs::symlink_status(folder);
    if(status.type()==fs::file_type::not_found) {
        if(create) fs::create_directories(folder);
    } else if(!fs::is_directory(status) || fs::is_symlink(status)) {
        throw std::runtime_error("View configuration folder must be a real directory");
    }
    const auto path=folder/"pokemon3d-view.txt";
    const auto file=fs::symlink_status(path);
    if(file.type()!=fs::file_type::not_found && !fs::is_regular_file(file))
        throw std::runtime_error("View preference must be a regular file, not a link");
    return path;
}
}
bool loadViewPreference(const fs::path& data,bool& enabled,std::string& error) {
    enabled=true;error.clear();
    try {
        const auto path=settingsPath(data,false);
        if(!fs::exists(path)) return true;
        if(fs::file_size(path)>64) throw std::runtime_error("View preference is too large");
        std::ifstream file(path,std::ios::binary);
        const std::string content((std::istreambuf_iterator<char>(file)),{});
        if(!file || (content!="R2N64-VIEW-1\n3d=on\n" && content!="R2N64-VIEW-1\n3d=off\n"))
            throw std::runtime_error("Invalid view preference; using 3D default");
        enabled=content=="R2N64-VIEW-1\n3d=on\n";
        return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool saveViewPreference(const fs::path& data,bool enabled,std::string& error) {
    error.clear();fs::path temporary,temporaryDirectory;bool ownsTemporary=false;
    try {
        const auto path=settingsPath(data,true);
        const auto unique=std::chrono::steady_clock::now().time_since_epoch().count();
        temporaryDirectory=path.string()+".tmp-"+std::to_string(unique);
        // Directory creation is exclusive on both desktop platforms. Never
        // truncate or clean up a temporary belonging to another writer.
        if(!fs::create_directory(temporaryDirectory)) throw std::runtime_error("Temporary preference already exists");
        ownsTemporary=true;temporary=temporaryDirectory/"value";
        std::ofstream file(temporary,std::ios::binary);
        file<<"R2N64-VIEW-1\n3d="<<(enabled?"on\n":"off\n");file.close();
        if(!file) throw std::runtime_error("Cannot write view preference");
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace view preference");
#else
        fs::rename(temporary,path);
#endif
        std::error_code ignored;fs::remove(temporaryDirectory,ignored);
        return true;
    } catch(const std::exception& e) {
        error=e.what();
        if(ownsTemporary) {
            std::error_code ignored;
            if(!temporary.empty()) fs::remove(temporary,ignored);
            fs::remove(temporaryDirectory,ignored);
        }
        return false;
    }
}
}
