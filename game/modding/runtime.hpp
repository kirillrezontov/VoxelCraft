#pragma once
#include "game/modding/save_data.hpp"
#include "game/gameplay/survival.hpp"
#include "game/content/materials.hpp"
#include "engine/voxel/collision.hpp"
#include <filesystem>
#include <memory>

namespace vc {
enum class ModEventKind { Load,Tick,Place,Break,Craft };
struct ModEvent {
    ModEventKind kind;
    voxel::Position position{};
    std::string identifier{};
    double seconds{};
};
struct ModContext {
    voxel::World& world;
    const voxel::BlockRegistry& blocks;
    const ItemRegistry& items;
    SurvivalState* survival;
    voxel::Aabb player;
    std::span<const voxel::Aabb> players{};
};
class ModRuntime {
public:
    // Empty/nonexistent root loads no mods. Present packs must be valid.
    ModRuntime(const std::filesystem::path& root,voxel::BlockRegistry& blocks);
    ~ModRuntime();
    ModRuntime(const ModRuntime&)=delete;
    ModRuntime& operator=(const ModRuntime&)=delete;
    void installItems(ItemRegistry& items,const voxel::BlockRegistry& blocks,std::vector<Recipe>& recipes) const;
    void installMining(MiningRules& rules,const voxel::BlockRegistry& blocks,const ItemRegistry& items) const;
    void restore(const std::vector<SavedMod>& saved);
    std::vector<SavedMod> save() const;
    std::vector<std::string> dispatch(const ModEvent& event,ModContext context);
    std::vector<std::string> takeDiagnostics();
    std::size_t size() const;
    std::vector<std::string> order() const;
    const ContentAtlas& atlas() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
