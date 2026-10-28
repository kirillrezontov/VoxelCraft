#pragma once
#include <filesystem>
#include <memory>

namespace vc {
// Non-blocking exclusive process lock. The OS releases it even after a crash.
// The empty lock file remains in place so concurrent opens refer to one inode.
class FileLock {
public:
    explicit FileLock(const std::filesystem::path& path);
    ~FileLock();
    FileLock(const FileLock&)=delete;
    FileLock& operator=(const FileLock&)=delete;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
