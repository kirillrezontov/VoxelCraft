#include "game/gameplay/inventory.hpp"
#include "engine/core/utf8.hpp"
#include <algorithm>
#include <stdexcept>

namespace vc {
CreativeInventory::CreativeInventory(const voxel::BlockRegistry& registry)
    :owned_(std::make_shared<ItemRegistry>(builtinItems(registry))),items_(owned_.get()) { defaults(); }
CreativeInventory::CreativeInventory(const ItemRegistry& items):items_(&items) { defaults(); }
void CreativeInventory::defaults() {
    slots_[0]=items_->find("voxelcraft:grass");slots_[1]=items_->find("voxelcraft:dirt");slots_[2]=items_->find("voxelcraft:stone");
}
const std::vector<ItemId>& CreativeInventory::catalog() const {
    if(catalogSize_!=items_->size()) {
        catalog_.clear();for(std::size_t i=0;i<items_->size();++i) catalog_.push_back(static_cast<ItemId>(i));catalogSize_=items_->size();
    }
    return catalog_;
}
std::vector<ItemId> CreativeInventory::filter(std::string_view query,ItemCategory category) const {
    if(query.size()>256) throw std::invalid_argument("Inventory search exceeds 256 bytes");
    const auto needle=utf8::lower(query);std::vector<ItemId> result;
    for(const auto item:catalog()) {
        const auto& definition=items_->definition(item);
        if(category!=ItemCategory::All&&definition.presentation.category!=categoryName(category)) continue;
        const auto haystack=utf8::lower(definition.identifier+' '+definition.presentation.name+' '+definition.presentation.description);
        if(needle.empty()||haystack.find(needle)!=std::string::npos) result.push_back(item);
    }
    return result;
}
std::optional<voxel::BlockId> CreativeInventory::selectedBlock() const {
    return selectedItem()?items_->definition(*selectedItem()).block:std::nullopt;
}
bool CreativeInventory::select(std::size_t slot) {
    if(slot>=slotCount) throw std::out_of_range("Hotbar slot outside 0..8");
    if(selected_==slot) return false;
    selected_=slot;++revision_;return true;
}
bool CreativeInventory::cycle(int steps) { return select(static_cast<std::size_t>((static_cast<int>(selected_)+steps%9+9)%9)); }
bool CreativeInventory::assignItem(std::size_t slot,std::optional<ItemId> item) {
    if(slot>=slotCount) throw std::out_of_range("Hotbar slot outside 0..8");
    if(item&&*item>=items_->size()) throw std::invalid_argument("Item is not registered in the creative catalog");
    if(slots_[slot]==item) return false;
    slots_[slot]=item;++revision_;return true;
}
bool CreativeInventory::assign(std::size_t slot,std::optional<voxel::BlockId> block) {
    if(!block) return assignItem(slot,{});
    for(const auto item:catalog()) if(items_->definition(item).block==block) return assignItem(slot,item);
    throw std::invalid_argument("Block is not registered as an item");
}
std::array<std::string,CreativeInventory::slotCount> CreativeInventory::names() const {
    std::array<std::string,slotCount> result;
    for(std::size_t i=0;i<slotCount;++i) if(slots_[i]) result[i]=items_->definition(*slots_[i]).identifier;
    return result;
}
void CreativeInventory::restore(const std::array<std::string,slotCount>& names,std::size_t selected) {
    auto restored=*this;
    for(std::size_t i=0;i<slotCount;++i) restored.assignItem(i,names[i].empty()?std::nullopt:std::optional(items_->require(names[i])));
    restored.select(selected);restored.revision_=revision_+1;*this=std::move(restored);
}
}
