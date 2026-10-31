#include "game/network/content_fingerprint.hpp"
#include "game/modding/runtime.hpp"
#include "engine/core/binary.hpp"

namespace vc::netgame {
std::uint64_t contentFingerprint(const voxel::BlockRegistry& blocks,const ItemRegistry& items,
        const std::vector<Recipe>& recipes,const MiningRules& mining,const ModRuntime& mods) {
    binary::Writer out;out.raw("VCCONTENT1");out.u32(static_cast<std::uint32_t>(blocks.size()));
    for(std::size_t i=0;i<blocks.size();++i) {
        const auto id=static_cast<voxel::BlockId>(i);const auto& b=blocks.definition(id);out.text(b.identifier);
        out.u32((b.opaque?1u:0u)|(b.solid?2u:0u)|(b.fluid?4u:0u));out.u32(b.emission);out.f64(b.opacity);
        for(int axis=0;axis<3;++axis) out.f64(b.color[axis]);
        for(const auto tile:b.faceTextures) out.u16(tile);
        const auto& rule=mining.rule(id);out.f64(rule.seconds);out.u32(static_cast<std::uint32_t>(rule.tool));out.u32(rule.requiredTier);
        out.text(rule.drop?items.definition(*rule.drop).identifier:"");out.u16(rule.dropCount);
    }
    out.u32(static_cast<std::uint32_t>(items.size()));
    for(std::size_t i=0;i<items.size();++i) {
        const auto& item=items.definition(static_cast<ItemId>(i));out.text(item.identifier);
        out.text(item.block?blocks.definition(*item.block).identifier:"");out.u16(item.stackSize);out.u16(item.durability);
        out.u32(static_cast<std::uint32_t>(item.tool));out.u32(item.tier);out.f64(item.miningSpeed);
        out.text(item.presentation.name);out.text(item.presentation.description);out.text(item.presentation.category);out.text(item.presentation.icon);out.u32(item.presentation.iconTile.value_or(0));
    }
    out.u32(static_cast<std::uint32_t>(recipes.size()));
    for(const auto& recipe:recipes) {
        out.text(recipe.name);out.text(items.definition(recipe.output).identifier);out.u16(recipe.count);out.u32(static_cast<std::uint32_t>(recipe.ingredients.size()));
        for(const auto& part:recipe.ingredients) { out.text(items.definition(part.item).identifier);out.u16(part.count); }
    }
    const auto packs=mods.save();out.u32(static_cast<std::uint32_t>(packs.size()));
    for(const auto& mod:packs) { out.text(mod.id);out.text(mod.version);out.u32(mod.stateVersion); }
    const auto image=mods.atlas().image();out.u32(image.width);out.u32(image.height);out.u64(binary::checksum(image.pixels));return binary::checksum(out.bytes);
}
}
