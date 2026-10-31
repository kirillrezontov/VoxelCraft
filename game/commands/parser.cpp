#include "game/commands/commands.hpp"
#include "engine/core/utf8.hpp"
#include <charconv>
#include <cmath>

namespace vc::commands {
std::string_view permissionName(Permission permission) {
    switch(permission) {
    case Permission::Player:return "player";case Permission::Moderator:return "moderator";
    case Permission::Administrator:return "administrator";case Permission::ServerConsole:return "server console";
    }
    return "invalid";
}
Result Result::ok(std::string message) { return {Code::Success,{std::move(message)}}; }
Result Result::error(Code code,std::string message) { return {code,{std::move(message)}}; }
Parsed parse(std::string_view text) {
    if(text.size()>maxCommandBytes) throw Error(Code::InvalidArgument,"Command exceeds 1024 bytes");
    if(!utf8::valid(text)) throw Error(Code::InvalidArgument,"Command is not valid UTF-8");
    for(const char byte:text) if(const auto c=static_cast<unsigned char>(byte);(c<32&&c!='\t')||c==127) throw Error(Code::InvalidArgument,"Control characters are not allowed in commands");
    std::vector<std::string> words;std::string word;bool started=false,escaped=false;char quote=0;
    for(char c:text) {
        if(escaped) { word+=c;started=true;escaped=false;continue; }
        if(c=='\\') { escaped=true;started=true;continue; }
        if(quote) { if(c==quote) quote=0;else word+=c;continue; }
        if(c=='\''||c=='"') { quote=c;started=true;continue; }
        if(c==' '||c=='\t') {
            if(started) { words.push_back(std::move(word));word.clear();started=false; }
        } else { word+=c;started=true; }
        if(words.size()>maxArguments) throw Error(Code::InvalidArgument,"Too many command arguments");
    }
    if(escaped||quote) throw Error(Code::InvalidArgument,escaped?"Unfinished command escape":"Unclosed quoted argument");
    if(started) words.push_back(std::move(word));
    if(words.empty()) throw Error(Code::MissingArgument,"Enter a command; use /help");
    if(words.size()>maxArguments+1) throw Error(Code::InvalidArgument,"Too many command arguments");
    Parsed result;result.name=words.front();if(result.name.starts_with('/')) result.name.erase(0,1);
    if(result.name.empty()) throw Error(Code::MissingArgument,"Enter a command after /");
    for(auto& c:result.name) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');
    result.arguments.assign(std::make_move_iterator(words.begin()+1),std::make_move_iterator(words.end()));return result;
}
double coordinate(std::string_view text,double relativeTo) {
    const bool relative=text.starts_with('~');if(relative) text.remove_prefix(1);
    double value=0;
    if(!text.empty()) {
        const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
        if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||!std::isfinite(value)) throw Error(Code::InvalidArgument,"Coordinate must be a finite number or ~offset");
    } else if(!relative) throw Error(Code::MissingArgument,"Missing coordinate");
    value+=relative?relativeTo:0;
    if(!std::isfinite(value)||std::abs(value)>1e15-16) throw Error(Code::InvalidArgument,"Coordinate exceeds supported physics range");
    return value;
}
std::uint64_t unsignedNumber(std::string_view text,std::uint64_t maximum) {
    std::uint64_t value{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||value>maximum) throw Error(Code::InvalidArgument,"Integer argument is invalid or outside its allowed range");
    return value;
}
}
