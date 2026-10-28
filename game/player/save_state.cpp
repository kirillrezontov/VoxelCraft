#include "game/player/save_state.hpp"
#include "engine/core/binary.hpp"
#include "engine/voxel/registry.hpp"
#include <cmath>
#include <stdexcept>

namespace vc {
namespace {
void validate(const PlayerSaveState& player) {
    validateActors(player.actors);
    for(int axis=0;axis<3;++axis)
        if(!std::isfinite(player.camera.position[axis])||std::abs(player.camera.position[axis])>1e15)
            throw std::runtime_error("Saved player position is outside supported physics coordinates");
    if(!std::isfinite(player.camera.yaw)||std::abs(player.camera.yaw)>6.283186||
        !std::isfinite(player.camera.pitch)||std::abs(player.camera.pitch)>1.553344)
        throw std::runtime_error("Invalid saved camera orientation");
    if(player.selectedSlot>=player.hotbar.size()) throw std::runtime_error("Invalid saved hotbar selection");
    for(const auto& name:player.hotbar)
        if(!name.empty()&&(name.size()>256||!voxel::BlockRegistry::validIdentifier(name)||name=="voxelcraft:air"))
            throw std::runtime_error("Invalid saved hotbar block identifier");
    if(player.health>20) throw std::runtime_error("Invalid saved player health");
    if(!std::isfinite(player.worldTime)||player.worldTime<0||player.worldTime>=1200) throw std::runtime_error("Invalid saved world time");
    for(const auto& stack:player.items) {
        if(stack.identifier.empty()) {
            if(stack.count||stack.durability) throw std::runtime_error("Empty saved item carries a quantity");
        } else if(stack.identifier.size()>256||!voxel::BlockRegistry::validIdentifier(stack.identifier)||!stack.count||stack.count>64)
            throw std::runtime_error("Invalid saved item stack");
    }
}
}
std::string encodePlayer(const PlayerSaveState& player) {
    validate(player);
    binary::Writer out;out.raw("VCPLAYER");out.u32(5);
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
    return {reinterpret_cast<const char*>(out.bytes.data()),out.bytes.size()};
}
PlayerSaveState decodePlayer(std::string_view data) {
    binary::Reader in({reinterpret_cast<const std::uint8_t*>(data.data()),data.size()});
    in.magic("VCPLAYER");const auto version=in.u32();
    if(version<1||version>5) throw std::runtime_error("Unsupported player save version");
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
    in.finish();validate(player);
    return player;
}
}
