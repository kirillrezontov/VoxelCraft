#pragma once
#include <filesystem>
#include <random>
#include <stdexcept>

namespace vc::test {
class TempDirectory {
public:
    TempDirectory() {
        std::random_device random;
        for(int attempt=0;attempt<16;++attempt) {
            path_=std::filesystem::temp_directory_path()/("voxelcraft-test-"+std::to_string(random())+'-'+std::to_string(random()));
            if(std::filesystem::create_directory(path_)) return;
        }
        throw std::runtime_error("Cannot create unique test directory");
    }
    ~TempDirectory() { std::error_code ignored;std::filesystem::remove_all(path_,ignored); }
    const std::filesystem::path& path() const { return path_; }
    TempDirectory(const TempDirectory&)=delete;
    TempDirectory& operator=(const TempDirectory&)=delete;
private:
    std::filesystem::path path_;
};
}
