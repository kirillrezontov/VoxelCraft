#include "engine/core/paths.hpp"
#include "engine/platform/file_lock.hpp"
#include <stdexcept>
#include <string>
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
#include <sys/file.h>
#include <unistd.h>
#endif

namespace vc {
struct FileLock::Impl {
#ifdef _WIN32
    HANDLE file{INVALID_HANDLE_VALUE};
    ~Impl() { if(file!=INVALID_HANDLE_VALUE) CloseHandle(file); }
#else
    int file{-1};
    ~Impl() { if(file>=0) close(file); }
#endif
};
FileLock::FileLock(const std::filesystem::path& path):impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    impl_->file=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(impl_->file==INVALID_HANDLE_VALUE) {
        const auto error=GetLastError();
        if(error==ERROR_SHARING_VIOLATION) throw std::runtime_error("World is locked by another session: "+vc::pathText(path));
        throw std::runtime_error("Cannot lock world: "+vc::pathText(path)+" (Windows error "+std::to_string(error)+')');
    }
#else
    impl_->file=open(path.c_str(),O_RDWR|O_CREAT|O_CLOEXEC,0600);
    if(impl_->file<0) throw std::runtime_error("Cannot open world lock: "+vc::pathText(path)+": "+std::strerror(errno));
    if(flock(impl_->file,LOCK_EX|LOCK_NB)!=0) {
        if(errno==EWOULDBLOCK||errno==EAGAIN) throw std::runtime_error("World is locked by another session: "+vc::pathText(path));
        throw std::runtime_error("Cannot lock world: "+vc::pathText(path)+": "+std::strerror(errno));
    }
#endif
}
FileLock::~FileLock()=default;
}
