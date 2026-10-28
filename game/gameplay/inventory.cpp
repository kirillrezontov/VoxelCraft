#include "game/gameplay/inventory.hpp"
#include <algorithm>
#include <stdexcept>

namespace vc {
CreativeInventory::CreativeInventory(const voxel::BlockRegistry& registry) {
    for(std::size_t i=1;i<registry.size();++i) catalog_.push_back(static_cast<voxel::BlockId>(i));
    slots_[0]=registry.require("voxelcraft:grass");slots_[1]=registry.require("voxelcraft:dirt");slots_[2]=registry.require("voxelcraft:stone");
}
bool CreativeInventory::select(std::size_t slot) {
    if(slot>=slotCount) throw std::out_of_range("Hotbar slot outside 0..8");
    if(selected_==slot) return false;
    selected_=slot;return true;
}
bool CreativeInventory::cycle(int steps) {
    const auto next=(static_cast<int>(selected_)+steps%9+9)%9;
    return select(static_cast<std::size_t>(next));
}
bool CreativeInventory::assign(std::size_t slot,std::optional<voxel::BlockId> block) {
    if(slot>=slotCount) throw std::out_of_range("Hotbar slot outside 0..8");
    if(block&&std::find(catalog_.begin(),catalog_.end(),*block)==catalog_.end())
        throw std::invalid_argument("Block is not in the creative inventory catalog");
    if(slots_[slot]==block) return false;
    slots_[slot]=block;return true;
}
std::array<std::string,CreativeInventory::slotCount> CreativeInventory::names(const voxel::BlockRegistry& registry) const {
    std::array<std::string,slotCount> result;
    for(std::size_t i=0;i<slotCount;++i) if(slots_[i]) result[i]=registry.definition(*slots_[i]).identifier;
    return result;
}
void CreativeInventory::restore(const std::array<std::string,slotCount>& names,std::size_t selected,const voxel::BlockRegistry& registry) {
    auto restored=*this;
    for(std::size_t i=0;i<slotCount;++i)
        restored.assign(i,names[i].empty()?std::nullopt:std::optional(registry.require(names[i])));
    restored.select(selected);*this=std::move(restored);
}
}
