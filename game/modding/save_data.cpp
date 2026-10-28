#include "game/modding/save_data.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <stdexcept>

namespace vc {
bool validModId(const std::string& id) {
    return !id.empty()&&id.size()<=48&&id!="voxelcraft"&&id.front()>='a'&&id.front()<='z'&&
        std::all_of(id.begin(),id.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';});
}
bool validModVersion(const std::string& version) {
    if(version.size()>24) return false;
    std::size_t from=0;
    for(int i=0;i<3;++i) {
        const auto to=version.find('.',from);const auto end=to==std::string::npos?version.size():to;
        if(end==from||(i<2&&to==std::string::npos)||(i==2&&to!=std::string::npos)) return false;
        unsigned part{};const auto parsed=std::from_chars(version.data()+from,version.data()+end,part);
        if(parsed.ec!=std::errc{}||parsed.ptr!=version.data()+end||part>65535) return false;
        from=end+1;
    }
    return true;
}
void validateMods(const std::vector<SavedMod>& mods) {
    if(mods.size()>32) throw std::runtime_error("Too many saved mods");
    std::set<std::string> ids;std::size_t bytes=0;
    for(const auto& mod:mods) {
        if(!validModId(mod.id)||!validModVersion(mod.version)||!ids.insert(mod.id).second||mod.state.size()>64)
            throw std::runtime_error("Invalid saved mod metadata");
        for(const auto& [key,value]:mod.state) {
            if(key.empty()||key.size()>64||value.size()>256) throw std::runtime_error("Invalid saved mod state");
            bytes+=key.size()+value.size()+4;
        }
    }
    if(bytes>8192) throw std::runtime_error("Saved mod state exceeds 8 KiB total");
}
}
