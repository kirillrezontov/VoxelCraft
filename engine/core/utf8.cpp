#include "engine/core/utf8.hpp"
#include <algorithm>
#include <stdexcept>

namespace vc::utf8 {
std::vector<Codepoint> decode(std::string_view text) {
    if(text.size()>1024*1024) throw std::invalid_argument("UTF-8 text exceeds 1 MiB");
    std::vector<Codepoint> result;
    for(std::size_t at=0;at<text.size();) {
        const auto start=at;const auto first=static_cast<unsigned char>(text[at++]);
        char32_t code=first;unsigned remaining=0;char32_t minimum=0;
        if(first<0x80) {}
        else if(first>=0xc2&&first<=0xdf) { code=first&0x1f;remaining=1;minimum=0x80; }
        else if(first>=0xe0&&first<=0xef) { code=first&0x0f;remaining=2;minimum=0x800; }
        else if(first>=0xf0&&first<=0xf4) { code=first&7;remaining=3;minimum=0x10000; }
        else throw std::invalid_argument("Invalid UTF-8 leading byte");
        for(unsigned n=0;n<remaining;++n) {
            if(at==text.size()) throw std::invalid_argument("Truncated UTF-8 code point");
            const auto byte=static_cast<unsigned char>(text[at++]);
            if((byte&0xc0)!=0x80) throw std::invalid_argument("Invalid UTF-8 continuation");
            code=(code<<6)|(byte&0x3f);
        }
        if(code<minimum||code>0x10ffff||(code>=0xd800&&code<=0xdfff)) throw std::invalid_argument("Invalid UTF-8 scalar value");
        result.push_back({code,start,at});
    }
    return result;
}
bool valid(std::string_view text) { try { (void)decode(text);return true; } catch(const std::invalid_argument&) { return false; } }
std::size_t previous(std::string_view text,std::size_t byte) {
    byte=std::min(byte,text.size());if(!byte) return 0;
    --byte;while(byte&&(static_cast<unsigned char>(text[byte])&0xc0)==0x80) --byte;return byte;
}
std::size_t next(std::string_view text,std::size_t byte) {
    byte=std::min(byte,text.size());if(byte==text.size()) return byte;
    ++byte;while(byte<text.size()&&(static_cast<unsigned char>(text[byte])&0xc0)==0x80) ++byte;return byte;
}
std::string lower(std::string_view text) {
    std::string result;
    for(const auto& point:decode(text)) {
        auto c=point.value;
        if(c>=U'A'&&c<=U'Z') c+=32;
        if(c>=0x410&&c<=0x42f) c+=32;
        if(c==0x401) c=0x451;
        if(c<0x80) result+=static_cast<char>(c);
        else if(c<0x800) { result+=static_cast<char>(0xc0|(c>>6));result+=static_cast<char>(0x80|(c&63)); }
        else if(c<0x10000) { result+=static_cast<char>(0xe0|(c>>12));result+=static_cast<char>(0x80|((c>>6)&63));result+=static_cast<char>(0x80|(c&63)); }
        else { result+=static_cast<char>(0xf0|(c>>18));result+=static_cast<char>(0x80|((c>>12)&63));result+=static_cast<char>(0x80|((c>>6)&63));result+=static_cast<char>(0x80|(c&63)); }
    }
    return result;
}
std::string truncate(std::string_view text,std::size_t maximum) {
    const auto points=decode(text);std::size_t end=0;
    for(const auto& point:points) { if(point.end>maximum) break;end=point.end; }
    return std::string(text.substr(0,end));
}
std::string singleLine(std::string_view text,std::size_t maximum) {
    std::string result;
    for(const auto& point:decode(text.substr(0,std::min(text.size(),std::size_t{1024*1024})))) {
        if(point.value<32||point.value==127) { if(result.size()<maximum) result+=' ';continue; }
        if(result.size()+point.end-point.begin>maximum) break;
        result.append(text.substr(point.begin,point.end-point.begin));
    }
    return result;
}
}
