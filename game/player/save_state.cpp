#include "game/player/save_state.hpp"
#include "engine/core/binary.hpp"
#include "engine/voxel/registry.hpp"
#include <cmath>
#include <stdexcept>

namespace vc {
namespace {
void validate(const PlayerSaveState& player) {
    validateActors(player.actors);
    validateMods(player.mods);
    validateProfiles(player.profiles);validatePlayerPose(player.camera);validateHotbar(player.hotbar,player.selectedSlot);
    if(player.health>20) throw std::runtime_error("Invalid saved player health");
    if(!std::isfinite(player.worldTime)||player.worldTime<0||player.worldTime>=1200) throw std::runtime_error("Invalid saved world time");
    for(const auto& stack:player.items) validateSavedItem(stack);
}
}
std::string encodePlayer(const PlayerSaveState& player) {
    validate(player);
    binary::Writer out;out.raw("VCPLAYER");out.u32(9);
    for(int axis=0;axis<3;++axis) out.f64(player.camera.position[axis]);
    out.f64(player.camera.yaw);out.f64(player.camera.pitch);out.u32(player.flying?1:0);
    out.u32(player.selectedSlot);
    for(const auto& name:player.hotbar) out.text(name);
    out.u32(player.survival?1:0);out.u32(player.health);
    for(const auto& stack:player.items) { out.text(stack.identifier);out.u16(stack.count);out.u16(stack.durability); }
    out.f64(player.worldTime);
    out.u32(static_cast<std::uint32_t>(player.actors.size()));
    for(const auto& a:player.actors) {
        out.u64(a.id);out.text(a.kind);
        for(int axis=0;axis<3;++axis) out.f64(a.position[axis]);
        for(int axis=0;axis<3;++axis) out.f64(a.velocity[axis]);
        out.f64(a.yaw);out.f64(a.age);out.f64(a.phase);out.u32(a.health);
        out.text(a.item.identifier);out.u16(a.item.count);out.u16(a.item.durability);
    }
    out.u32(static_cast<std::uint32_t>(player.mods.size()));
    for(const auto& mod:player.mods) {
        out.text(mod.id);out.text(mod.version);out.u32(mod.stateVersion);out.u32(static_cast<std::uint32_t>(mod.state.size()));
        for(const auto& [key,value]:mod.state) { out.text(key);out.text(value); }
    }
    out.u32(static_cast<std::uint32_t>(player.profiles.size()));
    for(const auto& profile:player.profiles) writeProfile(out,profile);
    return {reinterpret_cast<const char*>(out.bytes.data()),out.bytes.size()};
}
PlayerSaveState decodePlayer(std::string_view data) {
    binary::Reader in({reinterpret_cast<const std::uint8_t*>(data.data()),data.size()});
    in.magic("VCPLAYER");const auto version=in.u32();
    if(version<1||version>9) throw std::runtime_error("Unsupported player save version");
    PlayerSaveState player;
    for(int axis=0;axis<3;++axis) player.camera.position[axis]=in.f64();
    player.camera.yaw=in.f64();player.camera.pitch=in.f64();
    const auto flying=in.u32();if(flying>1) throw std::runtime_error("Invalid saved movement mode");
    player.flying=flying==1;
    if(version==1) player.hotbar[0]=in.text();
    else {
        player.selectedSlot=in.u32();
        for(auto& name:player.hotbar) name=in.text();
    }
    if(version>=3) {
        const auto survival=in.u32();if(survival>1) throw std::runtime_error("Invalid saved game mode");
        player.survival=survival==1;player.health=in.u32();
        for(auto& stack:player.items) { stack.identifier=in.text();stack.count=in.u16();stack.durability=in.u16(); }
    }
    if(version>=4) player.worldTime=in.f64();
    if(version>=5) {
        const auto count=in.u32();if(count>maxSavedActors) throw std::runtime_error("Too many saved actors");
        player.actors.resize(count);
        for(auto& a:player.actors) {
            a.id=in.u64();a.kind=in.text();
            for(int axis=0;axis<3;++axis) a.position[axis]=in.f64();
            for(int axis=0;axis<3;++axis) a.velocity[axis]=in.f64();
            a.yaw=in.f64();a.age=in.f64();a.phase=in.f64();a.health=in.u32();
            a.item.identifier=in.text();a.item.count=in.u16();a.item.durability=in.u16();
        }
    }
    if(version>=6) {
        const auto count=in.u32();if(count>32) throw std::runtime_error("Too many saved mods");
        player.mods.resize(count);
        for(auto& mod:player.mods) {
            mod.id=in.text();mod.version=in.text();
            if(version>=7) mod.stateVersion=in.u32();
            const auto entries=in.u32();if(entries>64) throw std::runtime_error("Too many saved mod state entries");
            for(std::uint32_t i=0;i<entries;++i) {
                auto key=in.text();auto value=in.text();
                if(!mod.state.emplace(std::move(key),std::move(value)).second)
                    throw std::runtime_error("Duplicate saved mod state key");
            }
        }
    }
    if(version>=8) {
        const auto count=in.u32();if(count>maxSavedProfiles) throw std::runtime_error("Too many saved player profiles");
        player.profiles.reserve(count);for(std::uint32_t i=0;i<count;++i) player.profiles.push_back(readProfile(in,version>=9));
    }
    in.finish();validate(player);
    return player;
}
}
