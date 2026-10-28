#pragma once
#include "engine/voxel/registry.hpp"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace vc {
// Creative sandbox inventory: a catalog of registered blocks and nine reusable
// hotbar assignments. Survival item quantities belong to the survival milestone.
class CreativeInventory {
public:
    static constexpr std::size_t slotCount=9;
    explicit CreativeInventory(const voxel::BlockRegistry& registry);
    std::size_t selectedSlot() const { return selected_; }
    std::optional<voxel::BlockId> selectedBlock() const { return slots_[selected_]; }
    const auto& slots() const { return slots_; }
    const auto& catalog() const { return catalog_; }
    bool select(std::size_t slot);
    bool cycle(int steps);
    bool assign(std::size_t slot,std::optional<voxel::BlockId> block);
    std::array<std::string,slotCount> names(const voxel::BlockRegistry& registry) const;
    void restore(const std::array<std::string,slotCount>& names,std::size_t selected,const voxel::BlockRegistry& registry);
private:
    std::array<std::optional<voxel::BlockId>,slotCount> slots_;
    std::vector<voxel::BlockId> catalog_;
    std::size_t selected_{};
};
}
