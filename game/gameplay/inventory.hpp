#pragma once
#include "game/gameplay/items.hpp"
#include <memory>

namespace vc {
// Nine non-consuming item assignments. Counted items and durability have exactly
// one implementation: SurvivalInventory. Names, never runtime IDs, are persisted.
class CreativeInventory {
public:
    static constexpr std::size_t slotCount=9;
    explicit CreativeInventory(const voxel::BlockRegistry& registry); // Legacy CPU callers.
    explicit CreativeInventory(const ItemRegistry& items);
    std::size_t selectedSlot() const { return selected_; }
    std::optional<ItemId> selectedItem() const { return slots_[selected_]; }
    std::optional<voxel::BlockId> selectedBlock() const;
    const auto& slots() const { return slots_; }
    const std::vector<ItemId>& catalog() const;
    std::vector<ItemId> filter(std::string_view query={},ItemCategory category=ItemCategory::All) const;
    const ItemRegistry& items() const { return *items_; }
    bool select(std::size_t slot);
    bool cycle(int steps);
    bool assignItem(std::size_t slot,std::optional<ItemId> item);
    bool assign(std::size_t slot,std::optional<voxel::BlockId> block); // Existing block callers.
    std::array<std::string,slotCount> names() const;
    std::array<std::string,slotCount> names(const voxel::BlockRegistry&) const { return names(); }
    void restore(const std::array<std::string,slotCount>& names,std::size_t selected);
    void restore(const std::array<std::string,slotCount>& names,std::size_t selected,const voxel::BlockRegistry&) { restore(names,selected); }
    std::uint64_t revision() const { return revision_; }
private:
    void defaults();
    std::shared_ptr<ItemRegistry> owned_;
    const ItemRegistry* items_{};
    std::array<std::optional<ItemId>,slotCount> slots_{};
    mutable std::vector<ItemId> catalog_;
    mutable std::size_t catalogSize_{};
    std::size_t selected_{};
    std::uint64_t revision_{};
};
}
