#include "engine/platform/dynamic_library.hpp"
#include "engine/core/paths.hpp"
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace vc {
struct DynamicLibrary::Impl {
#ifdef _WIN32
    HMODULE handle{};
    ~Impl() { if(handle) FreeLibrary(handle); }
#else
    void* handle{};
    ~Impl() { if(handle) dlclose(handle); }
#endif
};
DynamicLibrary::DynamicLibrary(const std::filesystem::path& file):impl_(std::make_unique<Impl>()) {
    if(!file.is_absolute()||!std::filesystem::is_regular_file(file)) throw std::invalid_argument("Native library requires an existing absolute file");
#ifdef _WIN32
    impl_->handle=LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!impl_->handle) throw std::runtime_error("Native DLL load failed (error "+std::to_string(GetLastError())+"): "+pathText(file));
#else
    impl_->handle=dlopen(file.c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!impl_->handle) { const char* error=dlerror();throw std::runtime_error(error?error:"Native SO load failed"); }
#endif
}
DynamicLibrary::~DynamicLibrary()=default;
void* DynamicLibrary::symbol(std::string_view name) const {
    if(name.empty()||name.find('\0')!=std::string_view::npos) throw std::invalid_argument("Invalid native symbol");
    const auto terminated=std::string(name);
#ifdef _WIN32
    auto* address=GetProcAddress(impl_->handle,terminated.c_str());
    if(!address) throw std::runtime_error("Missing native export: "+terminated);
    return reinterpret_cast<void*>(address);
#else
    (void)dlerror();auto* address=dlsym(impl_->handle,terminated.c_str());
    if(const auto* error=dlerror()) throw std::runtime_error(error);
    if(!address) throw std::runtime_error("Missing native export: "+terminated);
    return address;
#endif
}
}
