#pragma once
#include "engine/core/binary.hpp"
#include "game/player/save_state.hpp"

namespace vc::test {
// Genuine old layouts, independent of the current encoder and its append-only
// fields. Fixtures deliberately contain no actors or mods.
inline std::string legacyPlayer(unsigned version,const PlayerSaveState& state={}) {
    if(version<3||version>8) throw std::invalid_argument("Legacy fixture version outside 3..8");
    binary::Writer out;out.raw("VCPLAYER");out.u32(version);
    for(int axis=0;axis<3;++axis) out.f64(state.camera.position[axis]);
    out.f64(state.camera.yaw);out.f64(state.camera.pitch);out.u32(state.flying?1:0);out.u32(state.selectedSlot);
    for(const auto& name:state.hotbar) out.text(name);
    out.u32(state.survival?1:0);out.u32(state.health);
    for(const auto& item:state.items) { out.text(item.identifier);out.u16(item.count);out.u16(item.durability); }
    if(version>=4) out.f64(state.worldTime);
    if(version>=5) out.u32(0);
    if(version>=6) out.u32(0);
    if(version>=8) {
        out.u32(static_cast<std::uint32_t>(state.profiles.size()));
        for(const auto& profile:state.profiles) {
            out.text(profile.name);for(int axis=0;axis<3;++axis) out.f64(profile.camera.position[axis]);
            out.f64(profile.camera.yaw);out.f64(profile.camera.pitch);out.u32(profile.flying?1:0);out.u32(profile.selectedSlot);
            for(const auto& name:profile.hotbar) out.text(name);
        }
    }
    return {reinterpret_cast<const char*>(out.bytes.data()),out.bytes.size()};
}
}
