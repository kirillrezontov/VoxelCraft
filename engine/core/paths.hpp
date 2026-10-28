#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace vc {
// SDL and CLI strings are UTF-8; explicit conversion also works on Windows.
inline std::filesystem::path utf8Path(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
inline std::string pathText(const std::filesystem::path& path) {
    const auto text=path.u8string();
    return {reinterpret_cast<const char*>(text.data()),text.size()};
}
}
