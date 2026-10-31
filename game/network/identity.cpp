#include "game/network/identity.hpp"
#include "game/player/profile.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace vc::netgame {
namespace {
std::string read(const std::filesystem::path& path,std::uintmax_t limit) {
    if(std::filesystem::file_size(path)>limit) throw std::runtime_error("Identity file exceeds size limit");
    std::ifstream input(path,std::ios::binary);
    if(!input) throw std::runtime_error("Cannot open identity file");
    std::string result(static_cast<std::size_t>(limit)+1,'\0');
    input.read(result.data(),static_cast<std::streamsize>(result.size()));
    if(input.bad()||input.gcount()>static_cast<std::streamsize>(limit)) throw std::runtime_error("Cannot read bounded identity file");
    result.resize(static_cast<std::size_t>(input.gcount()));return result;
}
}
bool validIdentityToken(std::string_view token) {
    return token.size()==64&&std::all_of(token.begin(),token.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
bool equalIdentityToken(std::string_view expected,std::string_view supplied) {
    if(!validIdentityToken(expected)||!validIdentityToken(supplied)) return false;
    unsigned difference=0;
    for(std::size_t i=0;i<64;++i) difference|=static_cast<unsigned>(expected[i]^supplied[i]);
    return difference==0;
}
ProfileTokens readProfileTokens(const std::filesystem::path& path) {
    std::istringstream input(read(path,4096));ProfileTokens result;std::string name,token;
    while(input>>name) {
        if(!(input>>token)||!validPlayerName(name)||!validIdentityToken(token)||result.size()>=maxSavedProfiles||
            !result.emplace(playerNameKey(name),token).second) throw std::runtime_error("Invalid or duplicate identity file entry (expected NAME and 64 lowercase hex digits)");
    }
    if(result.empty()) throw std::runtime_error("Identity file must contain at least one player");
    return result;
}
std::string readIdentityToken(const std::filesystem::path& path) {
    std::istringstream input(read(path,128));std::string token,extra;
    if(!(input>>token)||!validIdentityToken(token)||(input>>extra)) throw std::runtime_error("Identity file must contain exactly 64 lowercase hex digits");
    return token;
}
}
