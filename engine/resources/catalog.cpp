#include "engine/resources/catalog.hpp"
#include "engine/core/paths.hpp"
#include "engine/core/utf8.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
#include <utility>

namespace vc {
namespace {
void check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
bool component(std::string_view text,bool namespacePart=false) {
    return !text.empty()&&text!="."&&text!=".."&&std::all_of(text.begin(),text.end(),[&](unsigned char c){
        return (c>='a'&&c<='z')||(!namespacePart&&c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';
    });
}
std::filesystem::path inside(const std::filesystem::path& root,const std::filesystem::path& path) {
    const auto file=std::filesystem::weakly_canonical(path);auto cursor=file.begin();
    for(const auto& part:root) { check(cursor!=file.end()&&*cursor==part,"Resource symlink escapes its provider");++cursor; }
    return file;
}
std::optional<std::vector<std::uint8_t>> read(const std::filesystem::path& root,const std::filesystem::path& relative,std::size_t maximum) {
    check(maximum&&maximum<=ResourceCatalog::maxResourceBytes,"Invalid resource byte budget");
    const auto file=inside(root,root/relative);if(!std::filesystem::exists(file)) return {};
    check(std::filesystem::is_regular_file(file),"Resource must be a regular file");
    const auto size=std::filesystem::file_size(file);check(size<=maximum,"Resource exceeds byte budget");
    std::ifstream input(file,std::ios::binary);check(static_cast<bool>(input),"Resource open failed");
    // Bound allocation and growth even if the file changes after file_size().
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size)+1);
    input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(!input.bad()&&input.gcount()==static_cast<std::streamsize>(size),"Resource changed during bounded read");
    bytes.resize(static_cast<std::size_t>(size));return bytes;
}
std::filesystem::path relative(std::string_view id) {
    check(ResourceCatalog::validIdentifier(id),"Invalid namespaced resource identifier");
    const auto colon=id.find(':');return utf8Path(id.substr(0,colon))/utf8Path(id.substr(colon+1));
}
}
bool ResourceCatalog::validIdentifier(std::string_view id) {
    if(id.size()>562) return false;
    const auto colon=id.find(':');
    if(colon==std::string_view::npos||colon>48||!component(id.substr(0,colon),true)) return false;
    auto path=id.substr(colon+1);if(path.empty()||path.size()>512) return false;
    for(;;) { const auto slash=path.find('/');if(!component(path.substr(0,slash))) return false;if(slash==std::string_view::npos) break;path.remove_prefix(slash+1); }
    return true;
}
ResourceCatalog::ResourceCatalog(std::filesystem::path builtin):builtin_(std::filesystem::canonical(std::move(builtin))) {
    check(std::filesystem::is_directory(builtin_),"Builtin resource provider must be a directory");
}
ResourcePackInfo ResourceCatalog::inspect(const std::filesystem::path& directory) {
    const auto root=std::filesystem::canonical(directory);check(std::filesystem::is_directory(root),"Resource pack must be a directory");
    const auto bytes=read(root,"resource-pack.json",16384);check(bytes.has_value(),"Missing resource-pack.json");
    const auto data=nlohmann::json::parse(bytes->begin(),bytes->end(),[](int depth,auto,auto&){check(depth<=16,"Resource manifest nesting limit");return true;});
    check(data.is_object(),"Resource manifest must be an object");
    for(const auto& [key,value]:data.items()) { (void)value;check(key=="id"||key=="name"||key=="version"||key=="priority","Unknown resource manifest field"); }
    ResourcePackInfo pack;pack.id=data.at("id").get<std::string>();pack.name=data.at("name").get<std::string>();pack.version=data.at("version").get<std::string>();
    check(pack.id.size()<=48&&component(pack.id,true)&&pack.id!="builtin","Invalid resource pack ID");
    check(!pack.name.empty()&&pack.name.size()<=128&&utf8::valid(pack.name)&&utf8::singleLine(pack.name,128)==pack.name,"Invalid resource pack name");
    check(!pack.version.empty()&&pack.version.size()<=24&&component(pack.version),"Invalid resource pack version");
    if(data.contains("priority")) { check(data["priority"].is_number_integer(),"Resource priority must be integer");const auto priority=data["priority"].get<int64_t>();check(priority>=-1000&&priority<=1000,"Resource priority range");pack.priority=static_cast<int>(priority); }
    pack.directory=root;pack.resources=inside(root,root/"resources");check(std::filesystem::is_directory(pack.resources),"Resource pack resources directory is missing");return pack;
}
void ResourceCatalog::activate(const std::vector<std::filesystem::path>& directories) {
    check(directories.size()<=maxPacks,"At most 32 active resource packs");std::vector<ResourcePackInfo> next;std::set<std::string> ids;
    for(const auto& path:directories) { auto pack=inspect(path);check(ids.insert(pack.id).second,"Duplicate resource pack ID");next.push_back(std::move(pack)); }
    std::sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.priority!=b.priority?a.priority>b.priority:a.id<b.id;});
    check(revision_!=UINT64_MAX,"Resource revision exhausted");active_=std::move(next);++revision_;
}
std::optional<std::vector<std::uint8_t>> ResourceCatalog::load(std::string_view id,std::size_t maximum) {
    const auto path=relative(id);
    for(const auto& pack:active_) {
        try { if(auto value=read(pack.resources,path,maximum)) return value; }
        catch(const std::exception& error) { if(diagnostics_.size()<128) diagnostics_.push_back(pack.id+": "+utf8::singleLine(error.what(),240)); }
    }
    return read(builtin_,path,maximum);
}
std::optional<std::vector<std::uint8_t>> ResourceCatalog::loadBuiltin(std::string_view id,std::size_t maximum) const { return read(builtin_,relative(id),maximum); }
std::vector<std::string> ResourceCatalog::takeDiagnostics() { return std::exchange(diagnostics_,{}); }
}
