#pragma once
#include <filesystem>
#include <memory>
#include <string_view>

namespace vc {
// Trusted executable code only. Native mechanics stay outside gameplay.
class DynamicLibrary {
public:
    explicit DynamicLibrary(const std::filesystem::path& absoluteFile);
    ~DynamicLibrary();
    DynamicLibrary(const DynamicLibrary&)=delete;
    DynamicLibrary& operator=(const DynamicLibrary&)=delete;
    void* symbol(std::string_view name) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
