#include "view_settings.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("r2n64-view-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        const auto check=[](bool value){if(!value)throw std::runtime_error("View preference test failed");};
        bool enabled=false;std::string error;
        check(pokemon3d::loadViewPreference(root,enabled,error)&&enabled);
        check(pokemon3d::saveViewPreference(root,false,error));
        check(pokemon3d::loadViewPreference(root,enabled,error)&&!enabled);
        check(pokemon3d::saveViewPreference(root,true,error));
        check(pokemon3d::loadViewPreference(root,enabled,error)&&enabled);
        const auto path=root/"configs/pokemon3d-view.txt";
        for(const auto& entry:fs::directory_iterator(root/"configs"))
            check(entry.path()==path); // Successful replacement leaves no temporary.
        std::ofstream(path)<<"R2N64-VIEW-2\n3d=off\n";
        check(!pokemon3d::loadViewPreference(root,enabled,error)&&enabled&&!error.empty());
        check(fs::file_size(path)==20);
        std::ofstream(path)<<std::string(65,'x');
        check(!pokemon3d::loadViewPreference(root,enabled,error));
        fs::remove(path);fs::create_directory(path);
        check(!pokemon3d::loadViewPreference(root,enabled,error));
        check(!pokemon3d::saveViewPreference(root,false,error));
        fs::remove_all(root);
        std::cout<<"PASS: defaults, on/off persistence, replace, invalid version/size/type\n";
        return 0;
    } catch(const std::exception& e) {
        fs::remove_all(root);std::cerr<<e.what()<<'\n';return 1;
    }
}
