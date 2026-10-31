#include "game/commands/permissions.hpp"
#include "game/player/profile.hpp"
#include "engine/core/binary.hpp"
#include "engine/platform/file_lock.hpp"
#include "engine/platform/file_sync.hpp"
#include <fstream>

namespace vc::commands {
PermissionStore::PermissionStore(std::filesystem::path file):file_(std::move(file)) {
    if(file_.empty()||!std::filesystem::exists(file_)) return;
    const auto size=std::filesystem::file_size(file_);if(size>4096) throw std::runtime_error("Permission file exceeds size limit");
    binary::Bytes bytes(static_cast<std::size_t>(size));std::ifstream input(file_,std::ios::binary);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error("Cannot read permission configuration");
    auto in=binary::Reader::checked(bytes);in.magic("VCROLES1");if(in.u32()!=1) throw std::runtime_error("Unsupported permission configuration version");
    const auto count=in.u32();if(count>32) throw std::runtime_error("Too many operator profiles");
    for(std::uint32_t n=0;n<count;++n) {
        const auto name=in.text();const auto role=in.u32();
        if(!validPlayerName(name)||name!=playerNameKey(name)||role<1||role>2||!roles_.emplace(name,static_cast<Permission>(role)).second)
            throw std::runtime_error("Invalid or duplicate operator profile");
    }
    in.finish();
}
Permission PermissionStore::get(std::string_view name) const {
    const auto found=roles_.find(playerNameKey(name));return found==roles_.end()?Permission::Player:found->second;
}
bool PermissionStore::set(std::string_view name,Permission permission) {
    if(!validPlayerName(name)||permission>Permission::Administrator) throw std::invalid_argument("Invalid operator profile or permission");
    const auto key=playerNameKey(name);auto next=roles_;
    if(permission==Permission::Player) next.erase(key);else next[key]=permission;
    if(next==roles_) return false;
    if(next.size()>32) throw std::runtime_error("Too many operator profiles");
    persist(next);roles_=std::move(next);return true;
}
void PermissionStore::persist(const std::map<std::string,Permission>& roles) const {
    if(file_.empty()) return;
    const auto parent=file_.parent_path().empty()?std::filesystem::path("."):file_.parent_path();std::filesystem::create_directories(parent);
    auto lockPath=file_;lockPath+=".lock";FileLock lock(lockPath);
    auto temporary=file_;temporary+=".tmp";
    binary::Writer out;out.raw("VCROLES1");out.u32(1);out.u32(static_cast<std::uint32_t>(roles.size()));
    for(const auto& [name,role]:roles) { out.text(name);out.u32(static_cast<std::uint32_t>(role)); }out.seal();
    try {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        output.write(reinterpret_cast<const char*>(out.bytes.data()),static_cast<std::streamsize>(out.bytes.size()));output.close();
        if(!output) throw std::runtime_error("Cannot write operator configuration");
        syncFile(temporary);replaceFile(temporary,file_);
    } catch(...) { std::error_code ignored;std::filesystem::remove(temporary,ignored);throw; }
}
}
