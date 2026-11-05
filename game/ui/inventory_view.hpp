#pragma once
#include "game/gameplay/inventory.hpp"

namespace vc {
// Owning presentation snapshots, never another authoritative inventory. The host
// item-atlas cache uses (identifier, resourceRevision) and retires its pages using
// renderer completion. Tile/block metadata comes from the current item registry.
struct ItemIconView {
    std::string identifier;
    std::uint64_t resourceRevision{};
    std::optional<TextureTile> tile;
    std::optional<voxel::BlockId> block;
};
struct ItemSlotView {
    std::size_t slot{};
    std::string name;
    ItemIconView icon;
    std::uint16_t count{},durability{};
    bool selected{},infinite{};
};
struct RecipeView {
    std::size_t recipe{};
    std::string name;
    ItemIconView output;
    std::uint16_t count{};
    CraftResult availability{CraftResult::MissingIngredients};
};
struct InventoryView {
    std::uint64_t revision{};
    bool survival{};
    std::size_t selected{};
    std::vector<ItemSlotView> slots;
    std::vector<RecipeView> recipes;
};
InventoryView inventoryView(const CreativeInventory&,const SurvivalInventory*,
    const ItemRegistry&,std::span<const Recipe>,std::uint64_t resourceRevision);
// Intents retain the existing transaction type. Bounds checks here are for UI
// dispatch; applyInventoryAction/server still validates against current state.
bool validInventoryIntent(const InventoryView&,const InventoryAction&);
}
