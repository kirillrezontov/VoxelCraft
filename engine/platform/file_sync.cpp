#include "engine/platform/file_sync.hpp"
#include "engine/core/paths.hpp"
#include <stdexcept>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace vc {
namespace {
#ifdef _WIN32
[[noreturn]] void failed(const std::filesystem::path& path) {
    throw std::runtime_error("Save synchronization failed: "+pathText(path)+" (Windows error "+std::to_string(GetLastError())+")");
}
#else
void syncPath(const std::filesystem::path& path,int flags) {
    const int descriptor=open(path.c_str(),flags|O_CLOEXEC);
    if(descriptor<0) throw std::runtime_error("Cannot open save for synchronization: "+pathText(path)+": "+std::strerror(errno));
    int result;
    do { result=fsync(descriptor); } while(result<0&&errno==EINTR);
    const int error=errno;
    close(descriptor);
    if(result<0) throw std::runtime_error("Save synchronization failed: "+pathText(path)+": "+std::strerror(error));
}
#endif
}
void syncFile(const std::filesystem::path& path) {
#ifdef _WIN32
    const HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) failed(path);
    const BOOL result=FlushFileBuffers(file);
    const DWORD error=GetLastError();CloseHandle(file);
    if(!result) { SetLastError(error);failed(path); }
#else
    syncPath(path,O_RDONLY);
#endif
}
void syncDirectory(const std::filesystem::path& path) {
#ifdef _WIN32
    (void)path;
#else
    syncPath(path,O_RDONLY|O_DIRECTORY);
#endif
}
void replaceFile(const std::filesystem::path& staging,const std::filesystem::path& destination) {
#ifdef _WIN32
    if(!MoveFileExW(staging.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) failed(destination);
#else
    std::filesystem::rename(staging,destination);
#endif
    syncDirectory(destination.parent_path().empty()?std::filesystem::path("."):destination.parent_path());
}
void publishDirectory(const std::filesystem::path& staging,const std::filesystem::path& destination) {
#ifdef _WIN32
    if(!MoveFileExW(staging.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)) failed(destination);
#else
    std::filesystem::rename(staging,destination);
#endif
    syncDirectory(destination.parent_path());
}
}
