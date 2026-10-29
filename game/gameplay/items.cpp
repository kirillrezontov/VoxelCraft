#include "game/gameplay/items.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace vc {
void validateSavedItem(const SavedItemStack& item) {
    if(item.identifier.empty()) {
        if(item.count||item.durability) throw std::runtime_error("Empty saved item carries a quantity");
    } else if(item.identifier.size()>256||!voxel::BlockRegistry::validIdentifier(item.identifier)||!item.count||item.count>64)
        throw std::runtime_error("Invalid saved item stack");
}
InventoryActionResult applyInventoryAction(SurvivalInventory& inventory,const InventoryAction& action,
        const ItemRegistry& items,const std::vector<Recipe>& recipes) {
    bool changed=false;
    switch(action.kind) {
    case InventoryActionKind::Swap: changed=inventory.swap(action.from,action.to);break;
    case InventoryActionKind::TransferHalf: changed=inventory.transfer(action.from,action.to,static_cast<std::uint16_t>((inventory.slots().at(action.from).count+1)/2),items);break;
    case InventoryActionKind::TransferAll: changed=inventory.transfer(action.from,action.to,inventory.slots().at(action.from).count,items);break;
    case InventoryActionKind::QuickTransfer: changed=inventory.quickTransfer(action.from,items);break;
    case InventoryActionKind::TransferOne: changed=inventory.transfer(action.from,action.to,1,items);break;
    case InventoryActionKind::MoveToBackpack: changed=inventory.moveToBackpack(action.from,items);break;
    case InventoryActionKind::Craft:
        if(action.recipe>=recipes.size()) throw std::invalid_argument("Unknown crafting recipe");
        switch(inventory.craft(recipes[action.recipe],items)) {
        case CraftResult::Crafted:return InventoryActionResult::Changed;
        case CraftResult::MissingIngredients:return InventoryActionResult::MissingIngredients;
        case CraftResult::NoSpace:return InventoryActionResult::NoSpace;
        }
        break;
    default: throw std::invalid_argument("Unknown inventory action");
    }
    return changed?InventoryActionResult::Changed:InventoryActionResult::NoChange;
}
ItemId ItemRegistry::add(ItemDefinition definition) {
    if(definition.presentation.name.empty()) {
        definition.presentation.name=definition.identifier.substr(definition.identifier.find(':')+1);
        bool capitalize=true;
        for(auto& c:definition.presentation.name) {
            if(c=='_') { c=' ';capitalize=true; }
            else if(capitalize) { if(c>='a'&&c<='z') c=static_cast<char>(c-'a'+'A');capitalize=false; }
        }
    }
    if(definition.presentation.category.empty()) {
        const auto name=definition.identifier;
        definition.presentation.category=definition.tool!=ToolKind::None?"tools":
            name=="voxelcraft:raw_meat"?"food":definition.block?
            (name=="voxelcraft:planks"||name=="voxelcraft:cobblestone"||name=="voxelcraft:lamp"?"building":"natural"):
            (name=="voxelcraft:stick"||name=="voxelcraft:coal"?"materials":"miscellaneous");
    }
    if(!definition.presentation.iconTile&&!definition.block) {
        constexpr std::array<std::string_view,5> names{"voxelcraft:stick","voxelcraft:coal","voxelcraft:raw_meat","voxelcraft:wooden_pickaxe","voxelcraft:stone_pickaxe"};
        auto tile=TextureTile{22};
        for(std::size_t i=0;i<names.size();++i) if(definition.identifier==names[i]) tile=static_cast<TextureTile>(17+i);
        definition.presentation.iconTile=tile;
    }
    validatePresentation(definition.presentation);
    if(!voxel::BlockRegistry::validIdentifier(definition.identifier)||definition.identifier.size()>256)
        throw std::invalid_argument("Invalid item identifier: "+definition.identifier);
    if(ids_.contains(definition.identifier)) throw std::invalid_argument("Duplicate item identifier: "+definition.identifier);
    if(!definition.stackSize||definition.stackSize>64||(definition.durability&&definition.stackSize!=1)||
        !std::isfinite(definition.miningSpeed)||definition.miningSpeed<=0)
        throw std::invalid_argument("Invalid item stack/tool definition");
    if(definitions_.size()>std::numeric_limits<ItemId>::max()) throw std::overflow_error("Item registry is full");
    const auto id=static_cast<ItemId>(definitions_.size());definitions_.push_back(std::move(definition));
    try { ids_.emplace(definitions_.back().identifier,id); }
    catch(...) { definitions_.pop_back();throw; }
    return id;
}
std::optional<ItemId> ItemRegistry::find(std::string_view name) const {
    const auto found=ids_.find(name);return found==ids_.end()?std::nullopt:std::optional(found->second);
}
ItemId ItemRegistry::require(std::string_view name) const {
    const auto id=find(name);if(!id) throw std::invalid_argument("Unknown item: "+std::string(name));return *id;
}
void retainHotbarItems(ItemRegistry& items,const std::array<std::string,9>& names) {
    for(const auto& name:names) if(!name.empty()&&!items.find(name)) items.add({name,{},64,0,ToolKind::None,0,1,true});
}
ItemRegistry builtinItems(const voxel::BlockRegistry& blocks) {
    ItemRegistry items;
    for(std::size_t id=1;id<blocks.size();++id) {
        const auto& block=blocks.definition(static_cast<voxel::BlockId>(id));
        ItemDefinition item{block.identifier,static_cast<voxel::BlockId>(id)};item.presentation=block.presentation;items.add(std::move(item));
    }
    items.add({"voxelcraft:stick",{}});items.add({"voxelcraft:coal",{}});
    items.add({"voxelcraft:raw_meat",{}});
    items.add({"voxelcraft:wooden_pickaxe",{},1,59,ToolKind::Pickaxe,1,3});
    items.add({"voxelcraft:stone_pickaxe",{},1,131,ToolKind::Pickaxe,2,5});
    return items;
}
std::vector<Recipe> builtinRecipes(const ItemRegistry& items) {
    const auto id=[&](std::string_view name){return items.require(name);};
    return {{"4 PLANKS",{{id("voxelcraft:log"),1}},id("voxelcraft:planks"),4},
        {"4 STICKS",{{id("voxelcraft:planks"),2}},id("voxelcraft:stick"),4},
        {"WOODEN PICKAXE",{{id("voxelcraft:planks"),3},{id("voxelcraft:stick"),2}},id("voxelcraft:wooden_pickaxe"),1},
        {"STONE PICKAXE",{{id("voxelcraft:cobblestone"),3},{id("voxelcraft:stick"),2}},id("voxelcraft:stone_pickaxe"),1},
        {"LAMP",{{id("voxelcraft:planks"),4},{id("voxelcraft:coal"),1}},id("voxelcraft:lamp"),1}};
}
bool SurvivalInventory::select(std::size_t index) {
    if(index>=hotbarSize) throw std::out_of_range("Survival hotbar slot outside 0..8");
    if(index==selected_) return false;
    selected_=index;++revision_;return true;
}
bool SurvivalInventory::cycle(int steps) { return select(static_cast<std::size_t>((static_cast<int>(selected_)+steps%9+9)%9)); }
bool SurvivalInventory::addInto(ItemStack incoming,const ItemRegistry& registry,std::size_t begin) {
    const auto& definition=registry.definition(incoming.item);
    for(std::size_t i=begin;i<slotCount&&incoming.count;++i) {
        auto& slot=slots_[i];
        if(slot.empty()||slot.item!=incoming.item||slot.durability!=incoming.durability||slot.count>=definition.stackSize) continue;
        const auto take=std::min<std::uint16_t>(incoming.count,static_cast<std::uint16_t>(definition.stackSize-slot.count));
        slot.count=static_cast<std::uint16_t>(slot.count+take);incoming.count=static_cast<std::uint16_t>(incoming.count-take);
    }
    for(std::size_t i=begin;i<slotCount&&incoming.count;++i) {
        if(!slots_[i].empty()) continue;
        const auto take=std::min(incoming.count,definition.stackSize);slots_[i]={incoming.item,take,incoming.durability};
        incoming.count=static_cast<std::uint16_t>(incoming.count-take);
    }
    return incoming.count==0;
}
bool SurvivalInventory::add(ItemId item,std::uint16_t count,const ItemRegistry& registry,std::uint16_t durability) {
    const auto& definition=registry.definition(item);
    if(!count) return true;
    if(durability>definition.durability) throw std::invalid_argument("Item durability exceeds definition");
    const auto life=durability?durability:definition.durability;
    auto next=*this;if(!next.addInto({item,count,life},registry)) return false;
    next.revision_=revision_+1;*this=std::move(next);return true;
}
bool SurvivalInventory::consumeSelected(std::uint16_t count) {
    auto& slot=slots_[selected_];if(!count||slot.count<count) return false;
    slot.count=static_cast<std::uint16_t>(slot.count-count);if(!slot.count) slot={};++revision_;return true;
}
void SurvivalInventory::damageTool() {
    auto& slot=slots_[selected_];if(slot.empty()||!slot.durability) return;
    --slot.durability;if(!slot.durability) slot={};++revision_;
}
bool SurvivalInventory::swap(std::size_t a,std::size_t b) {
    if(a>=slotCount||b>=slotCount) throw std::out_of_range("Inventory slot outside 0..35");
    if(slots_[a]==slots_[b]) return false;
    std::swap(slots_[a],slots_[b]);++revision_;return true;
}
bool SurvivalInventory::transfer(std::size_t from,std::size_t to,std::uint16_t count,const ItemRegistry& registry) {
    if(from>=slotCount||to>=slotCount) throw std::out_of_range("Inventory slot outside 0..35");
    if(from==to||!count||slots_[from].empty()) return false;
    auto next=*this;auto& source=next.slots_[from];auto& target=next.slots_[to];
    const auto max=registry.definition(source.item).stackSize;
    if(!target.empty()&&(target.item!=source.item||target.durability!=source.durability)) return false;
    const auto capacity=target.empty()?max:static_cast<std::uint16_t>(max-target.count);
    const auto take=std::min({count,source.count,capacity});if(!take) return false;
    if(target.empty()) target={source.item,0,source.durability};
    target.count=static_cast<std::uint16_t>(target.count+take);source.count=static_cast<std::uint16_t>(source.count-take);
    if(!source.count) source={};
    next.revision_=revision_+1;*this=std::move(next);return true;
}
bool SurvivalInventory::moveToBackpack(std::size_t from,const ItemRegistry& registry) {
    if(from>=hotbarSize) throw std::out_of_range("Quick move requires a hotbar slot");
    if(slots_[from].empty()) return false;
    auto next=*this;const auto stack=next.slots_[from];next.slots_[from]={};
    if(!next.addInto(stack,registry,hotbarSize)) return false;
    next.revision_=revision_+1;*this=std::move(next);return true;
}
unsigned SurvivalInventory::count(ItemId item) const {
    unsigned total=0;for(const auto& slot:slots_) if(!slot.empty()&&slot.item==item) total+=slot.count;return total;
}
bool SurvivalInventory::remove(ItemId item,std::uint16_t amount) {
    if(count(item)<amount) return false;
    for(auto& slot:slots_) {
        if(slot.empty()||slot.item!=item) continue;
        const auto take=std::min(amount,slot.count);slot.count=static_cast<std::uint16_t>(slot.count-take);amount=static_cast<std::uint16_t>(amount-take);
        if(!slot.count) slot={};
        if(!amount) return true;
    }
    return true;
}
bool SurvivalInventory::quickTransfer(std::size_t from,const ItemRegistry& registry) {
    if(from>=slotCount) throw std::out_of_range("Inventory slot outside 0..35");
    if(slots_[from].empty()) return false;
    auto next=*this;const auto end=from<hotbarSize?slotCount:hotbarSize,begin=from<hotbarSize?hotbarSize:0;
    // Fill existing compatible stacks before empty slots; partial transfers keep
    // their remainder in the source. Every counted item remains in this object.
    for(const bool empty:{false,true}) for(auto to=begin;to<end;++to) {
        if(next.slots_[from].empty()) break;
        if(next.slots_[to].empty()!=empty) continue;
        (void)next.transfer(from,to,next.slots_[from].count,registry);
    }
    if(next.slots_==slots_) return false;
    next.revision_=revision_+1;*this=std::move(next);return true;
}
CraftResult SurvivalInventory::canCraft(const Recipe& recipe,const ItemRegistry& registry) const {
    std::map<ItemId,unsigned> needed;
    for(const auto& ingredient:recipe.ingredients) {
        if(!ingredient.count) throw std::invalid_argument("Recipe ingredient count must be positive");
        (void)registry.definition(ingredient.item);needed[ingredient.item]+=ingredient.count;
    }
    const auto& output=registry.definition(recipe.output);
    if(!recipe.count) throw std::invalid_argument("Recipe output count must be positive");
    for(const auto& [item,amount]:needed) if(count(item)<amount) return CraftResult::MissingIngredients;
    unsigned capacity=0;auto remaining=needed;
    for(const auto& slot:slots_) {
        auto countAfter=static_cast<unsigned>(slot.count);
        if(!slot.empty()) { auto& amount=remaining[slot.item];const auto taken=std::min(amount,countAfter);amount-=taken;countAfter-=taken; }
        if(!countAfter) capacity+=output.stackSize;
        else if(slot.item==recipe.output&&slot.durability==output.durability) capacity+=output.stackSize-countAfter;
    }
    return capacity>=recipe.count?CraftResult::Crafted:CraftResult::NoSpace;
}
CraftResult SurvivalInventory::craft(const Recipe& recipe,const ItemRegistry& registry) {
    auto next=*this;
    for(const auto& ingredient:recipe.ingredients) {
        if(!ingredient.count) throw std::invalid_argument("Recipe ingredient count must be positive");
        if(!next.remove(ingredient.item,ingredient.count)) return CraftResult::MissingIngredients;
    }
    if(!recipe.count) throw std::invalid_argument("Recipe output count must be positive");
    if(!next.add(recipe.output,recipe.count,registry)) return CraftResult::NoSpace;
    next.revision_=revision_+1;*this=std::move(next);return CraftResult::Crafted;
}
std::array<SavedItemStack,SurvivalInventory::slotCount> SurvivalInventory::save(const ItemRegistry& registry) const {
    std::array<SavedItemStack,slotCount> result;
    for(std::size_t i=0;i<slotCount;++i) if(!slots_[i].empty()) result[i]={registry.definition(slots_[i].item).identifier,slots_[i].count,slots_[i].durability};
    return result;
}
void SurvivalInventory::restore(const std::array<SavedItemStack,slotCount>& stacks,std::size_t selected,ItemRegistry& registry) {
    SurvivalInventory next;auto resolved=registry;
    for(std::size_t i=0;i<slotCount;++i) {
        const auto& saved=stacks[i];
        if(saved.identifier.empty()) { if(saved.count||saved.durability) throw std::runtime_error("Empty saved item carries quantity/durability");continue; }
        auto item=resolved.find(saved.identifier);
        if(!item) item=resolved.add({saved.identifier,{},saved.durability?std::uint16_t{1}:std::uint16_t{64},
            saved.durability?std::numeric_limits<std::uint16_t>::max():std::uint16_t{0},ToolKind::None,0,1,true});
        // A creative name may have registered a generic missing placeholder
        // before counted stacks resolve. Refine that same ID using saved tool
        // data so transfers keep one-item stacks and durability survives reload.
        auto& missing=resolved.definitions_.at(*item);
        if(missing.missing&&saved.durability&&!missing.durability) {
            missing.stackSize=1;missing.durability=std::numeric_limits<std::uint16_t>::max();
        }
        const auto& definition=resolved.definition(*item);
        if(!saved.count||saved.count>definition.stackSize||saved.durability>definition.durability||(!definition.missing&&definition.durability&&!saved.durability))
            throw std::runtime_error("Invalid saved item quantity/durability");
        next.slots_[i]={*item,saved.count,saved.durability};
    }
    next.select(selected);next.revision_=revision_+1;registry=std::move(resolved);*this=std::move(next);
}
}
