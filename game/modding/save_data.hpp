#pragma once
#include <map>
#include <string>
#include <vector>

namespace vc {
struct SavedMod {
    std::string id,version;
    std::map<std::string,std::string,std::less<>> state;
};
bool validModId(const std::string& id);
bool validModVersion(const std::string& version);
void validateMods(const std::vector<SavedMod>& mods);
}
