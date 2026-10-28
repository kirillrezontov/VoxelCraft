#pragma once
#include "engine/voxel/registry.hpp"
#include <array>
#include <map>
#include <optional>
#include <span>

namespace vc {
using ItemId=std::uint16_t;
enum class ToolKind { None,Pickaxe };
struct ItemDefinition {
    std::string identifier;
    std::optional<voxel::BlockId> block;
    std::uint16_t stackSize{64},durability{};
    ToolKind tool{ToolKind::None};
    unsigned tier{};
    double miningSpeed{1};
    bool missing{};
};
class ItemRegistry {
public:
    ItemId add(ItemDefinition definition);
    std::optional<ItemId> find(std::string_view name) const;
    ItemId require(std::string_view name) const;
    const ItemDefinition& definition(ItemId id) const { return definitions_.at(id); }
    std::size_t size() const { return definitions_.size(); }
private:
    std::vector<ItemDefinition> definitions_;
    std::map<std::string,ItemId,std::less<>> ids_;
};
ItemRegistry builtinItems(const voxel::BlockRegistry& blocks);
struct ItemStack {
    ItemId item{};
    std::uint16_t count{},durability{};
    bool empty() const { return count==0; }
    bool operator==(const ItemStack&) const=default;
};
struct SavedItemStack { std::string identifier;std::uint16_t count{},durability{};bool operator==(const SavedItemStack&) const=default; };
struct Ingredient { ItemId item;std::uint16_t count; };
struct Recipe { std::string name;std::vector<Ingredient> ingredients;ItemId output;std::uint16_t count; };
std::vector<Recipe> builtinRecipes(const ItemRegistry& items);
enum class CraftResult { Crafted,MissingIngredients,NoSpace };
class SurvivalInventory {
public:
    static constexpr std::size_t slotCount=36,hotbarSize=9;
    const auto& slots() const { return slots_; }
    std::size_t selectedSlot() const { return selected_; }
    const ItemStack& selected() const { return slots_[selected_]; }
    bool select(std::size_t index);
    bool cycle(int steps);
    bool add(ItemId item,std::uint16_t count,const ItemRegistry& registry,std::uint16_t durability=0);
    bool consumeSelected(std::uint16_t count=1);
    void damageTool();
    bool swap(std::size_t a,std::size_t b);
    bool transfer(std::size_t from,std::size_t to,std::uint16_t count,const ItemRegistry& registry);
    bool moveToBackpack(std::size_t from,const ItemRegistry& registry);
    CraftResult craft(const Recipe& recipe,const ItemRegistry& registry);
    unsigned count(ItemId item) const;
    std::array<SavedItemStack,slotCount> save(const ItemRegistry& registry) const;
    void restore(const std::array<SavedItemStack,slotCount>& stacks,std::size_t selected,ItemRegistry& registry);
    std::uint64_t revision() const { return revision_; }
private:
    bool addInto(ItemStack stack,const ItemRegistry& registry,std::size_t begin=0);
    bool remove(ItemId item,std::uint16_t count);
    std::array<ItemStack,slotCount> slots_{};
    std::size_t selected_{};
    std::uint64_t revision_{};
};
}
