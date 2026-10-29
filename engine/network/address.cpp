#include "engine/network/address.hpp"
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace vc::net {
Endpoint parseEndpoint(std::string_view text,std::uint16_t defaultPort) {
    if(text.empty()||text.size()>270||!defaultPort) throw std::invalid_argument("Invalid server address");
    Endpoint result;result.port=defaultPort;std::string_view port;
    if(text.front()=='[') {
        const auto end=text.find(']');if(end==std::string_view::npos||end==1) throw std::invalid_argument("Invalid bracketed IPv6 address");
        result.host=text.substr(1,end-1);const auto suffix=text.substr(end+1);
        if(!suffix.empty()) { if(suffix.front()!=':'||suffix.size()==1) throw std::invalid_argument("Invalid server port suffix");port=suffix.substr(1); }
        if(result.host.find(':')==std::string::npos) throw std::invalid_argument("Bracketed server addresses require IPv6");
    } else if(std::count(text.begin(),text.end(),':')==1) {
        const auto split=text.find(':');result.host=text.substr(0,split);port=text.substr(split+1);
        if(port.empty()) throw std::invalid_argument("Missing server port");
    } else result.host=text;
    if(result.host.empty()||result.host.size()>255||result.host.find_first_of("[]/\\?#")!=std::string::npos||
        std::any_of(result.host.begin(),result.host.end(),[](unsigned char c){return c<=32||c==127;})) throw std::invalid_argument("Invalid server host/IP");
    if(!port.empty()) {
        unsigned value{};const auto parsed=std::from_chars(port.data(),port.data()+port.size(),value);
        if(parsed.ec!=std::errc{}||parsed.ptr!=port.data()+port.size()||!value||value>65535) throw std::invalid_argument("Server port must be 1..65535");
        result.port=static_cast<std::uint16_t>(value);
    }
    return result;
}
}
