#include "game/ui/inventory_view.hpp"
namespace vc {
InventoryView inventoryView(const CreativeInventory& creative,const SurvivalInventory* survival,
        const ItemRegistry& items,std::span<const Recipe> recipes,std::uint64_t resourceRevision) {
    InventoryView view;view.survival=survival!=nullptr;
    view.revision=survival?survival->revision():creative.revision();
    view.selected=survival?survival->selectedSlot():creative.selectedSlot();
    const auto icon=[&](ItemId id) {
        const auto& item=items.definition(id);
        return ItemIconView{item.identifier,resourceRevision,item.presentation.iconTile,item.block};
    };
    const auto count=survival?SurvivalInventory::slotCount:CreativeInventory::slotCount;
    view.slots.reserve(count);
    for(std::size_t index=0;index<count;++index) {
        ItemSlotView slot;slot.slot=index;slot.selected=index==view.selected;
        const auto item=survival?(survival->slots()[index].empty()?std::optional<ItemId>{}:survival->slots()[index].item):creative.slots()[index];
        if(item) {
            slot.name=items.definition(*item).presentation.name;slot.icon=icon(*item);
            slot.infinite=!survival;
            if(survival) {slot.count=survival->slots()[index].count;slot.durability=survival->slots()[index].durability;}
        }
        view.slots.push_back(std::move(slot));
    }
    if(survival) for(std::size_t index=0;index<recipes.size();++index) {
        const auto& recipe=recipes[index];
        view.recipes.push_back({index,recipe.name,icon(recipe.output),recipe.count,survival->canCraft(recipe,items)});
    }
    return view;
}
bool validInventoryIntent(const InventoryView& view,const InventoryAction& action) {
    if(!view.survival) return false;
    switch(action.kind) {
    case InventoryActionKind::Craft:return action.recipe<view.recipes.size();
    case InventoryActionKind::MoveToBackpack:
    case InventoryActionKind::QuickTransfer:return action.from<view.slots.size();
    case InventoryActionKind::Swap:
    case InventoryActionKind::TransferOne:
    case InventoryActionKind::TransferHalf:
    case InventoryActionKind::TransferAll:return action.from<view.slots.size()&&action.to<view.slots.size();
    }
    return false;
}
}
