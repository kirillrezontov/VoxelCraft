#include "game/player/profile.hpp"
#include "engine/core/binary.hpp"
#include "engine/voxel/registry.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace vc {
bool validPlayerName(std::string_view name) {
    return !name.empty()&&name.size()<=24&&std::all_of(name.begin(),name.end(),[](char c) {
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';
    });
}
std::string playerNameKey(std::string_view name) {
    std::string key(name);for(auto& c:key) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');return key;
}
void validatePlayerPose(const Camera& camera) {
    for(int axis=0;axis<3;++axis) if(!std::isfinite(camera.position[axis])||std::abs(camera.position[axis])>1e15)
        throw std::runtime_error("Saved player position is outside supported physics coordinates");
    if(!std::isfinite(camera.yaw)||std::abs(camera.yaw)>6.283186||!std::isfinite(camera.pitch)||std::abs(camera.pitch)>1.553344)
        throw std::runtime_error("Invalid saved camera orientation");
}
void validateHotbar(const std::array<std::string,9>& hotbar,std::uint32_t selectedSlot) {
    if(selectedSlot>=hotbar.size()) throw std::runtime_error("Invalid saved hotbar selection");
    for(const auto& name:hotbar)
        if(!name.empty()&&(name.size()>256||!voxel::BlockRegistry::validIdentifier(name)||name=="voxelcraft:air"))
            throw std::runtime_error("Invalid saved hotbar block identifier");
}
void validateProfile(const PlayerProfile& profile) {
    if(!validPlayerName(profile.name)) throw std::runtime_error("Player name must be 1..24 ASCII letters, digits, underscores or hyphens");
    validateSpawnPoint(profile.spawnPoint);
    validatePlayerPose(profile.camera);validateHotbar(profile.hotbar,profile.selectedSlot);
    if(profile.health>20) throw std::runtime_error("Invalid profile health");
    for(const auto& item:profile.items) validateSavedItem(item);
}
void validateProfiles(const std::vector<PlayerProfile>& profiles) {
    if(profiles.size()>maxSavedProfiles) throw std::runtime_error("Too many saved player profiles");
    std::set<std::string> names;
    for(const auto& profile:profiles) { validateProfile(profile);if(!names.insert(playerNameKey(profile.name)).second) throw std::runtime_error("Duplicate saved player profile"); }
}
void validateSpawnPoint(const std::optional<glm::dvec3>& spawn) {
    if(spawn) for(int axis=0;axis<3;++axis) if(!std::isfinite((*spawn)[axis])||std::abs((*spawn)[axis])>1e15-16)
        throw std::runtime_error("Invalid persistent respawn coordinates");
}
void writeSpawnPoint(binary::Writer& out,const std::optional<glm::dvec3>& spawn) {
    validateSpawnPoint(spawn);out.u32(spawn?1:0);if(spawn) for(int axis=0;axis<3;++axis) out.f64((*spawn)[axis]);
}
std::optional<glm::dvec3> readSpawnPoint(binary::Reader& in) {
    const auto present=in.u32();if(present>1) throw std::runtime_error("Invalid respawn point flag");
    std::optional<glm::dvec3> spawn;if(present) { spawn.emplace();for(int axis=0;axis<3;++axis) (*spawn)[axis]=in.f64(); }
    validateSpawnPoint(spawn);return spawn;
}
void writeProfile(binary::Writer& out,const PlayerProfile& profile,bool extendedFields) {
    validateProfile(profile);out.text(profile.name);
    for(int axis=0;axis<3;++axis) out.f64(profile.camera.position[axis]);
    out.f64(profile.camera.yaw);out.f64(profile.camera.pitch);out.u32(profile.flying?1:0);out.u32(profile.selectedSlot);
    for(const auto& name:profile.hotbar) out.text(name);
    out.u32(profile.health);
    for(const auto& item:profile.items) { out.text(item.identifier);out.u16(item.count);out.u16(item.durability); }
    if(extendedFields) { out.u32(profile.survivalMode?(*profile.survivalMode?2:1):0);writeSpawnPoint(out,profile.spawnPoint); }
}
PlayerProfile readProfile(binary::Reader& in,bool survivalFields,bool extendedFields) {
    PlayerProfile profile;profile.name=in.text();
    for(int axis=0;axis<3;++axis) profile.camera.position[axis]=in.f64();
    profile.camera.yaw=in.f64();profile.camera.pitch=in.f64();const auto flying=in.u32();
    if(flying>1) throw std::runtime_error("Invalid profile movement mode");
    profile.flying=flying==1;profile.selectedSlot=in.u32();for(auto& name:profile.hotbar) name=in.text();
    if(survivalFields) {
        profile.health=in.u32();
        for(auto& item:profile.items) { item.identifier=in.text();item.count=in.u16();item.durability=in.u16(); }
    }
    if(extendedFields) {
        const auto mode=in.u32();if(mode>2) throw std::runtime_error("Invalid profile game mode");
        if(mode) profile.survivalMode=mode==2;
        profile.spawnPoint=readSpawnPoint(in);
    }
    validateProfile(profile);return profile;
}
}
