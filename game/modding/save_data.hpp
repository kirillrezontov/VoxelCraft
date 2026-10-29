#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace vc {
struct SavedMod {
    std::string id,version;
    std::map<std::string,std::string,std::less<>> state;
    std::uint32_t stateVersion{1};
};
bool validModId(const std::string& id);
bool validModVersion(const std::string& version);
void validateMods(const std::vector<SavedMod>& mods);
}
