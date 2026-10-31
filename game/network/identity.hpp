#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace vc::netgame {
using ProfileTokens=std::map<std::string,std::string>;
bool validIdentityToken(std::string_view token);
bool equalIdentityToken(std::string_view expected,std::string_view supplied);
// Small local files; never include credentials or their file contents in diagnostics.
ProfileTokens readProfileTokens(const std::filesystem::path& path);
std::string readIdentityToken(const std::filesystem::path& path);
}
