#pragma once
#include <filesystem>

namespace vc {
// Errors propagate: closing a buffered stream alone does not make a save durable.
void syncFile(const std::filesystem::path& path);
// POSIX directory metadata barrier. Windows publication uses WRITE_THROUGH;
// Windows does not provide a portable directory FlushFileBuffers equivalent.
void syncDirectory(const std::filesystem::path& path);
void replaceFile(const std::filesystem::path& staging,const std::filesystem::path& destination);
void publishDirectory(const std::filesystem::path& staging,const std::filesystem::path& destination);
}
