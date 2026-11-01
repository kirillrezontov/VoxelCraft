#pragma once
#include "game/modding/runtime.hpp"
#include <optional>

namespace vc {
// Session-owned content. Register packs before resolving a saved/network palette;
// rebuild gameplay values afterwards, before restoring inventories or starting jobs.
// References to the registry objects stay valid until destruction. Entry references
// and numeric item IDs must not be kept across rebuildGameplay(). Main thread only.
class ContentServices {
public:
    explicit ContentServices(const std::filesystem::path& modsDirectory = {});
    ContentServices(const ContentServices&) = delete;
    ContentServices& operator=(const ContentServices&) = delete;
    voxel::BlockRegistry& blocks() { return blocks_; }
    const voxel::BlockRegistry& blocks() const { return blocks_; }
    ModRuntime& mods() { return mods_; }
    const ModRuntime& mods() const { return mods_; }
    ItemRegistry& items() { return items_; }
    const ItemRegistry& items() const { return items_; }
    std::vector<Recipe>& recipes() { return recipes_; }
    const std::vector<Recipe>& recipes() const { return recipes_; }
    MiningRules& mining() { return *mining_; }
    const MiningRules& mining() const { return *mining_; }
    void rebuildGameplay();
private:
    voxel::BlockRegistry blocks_;
    ModRuntime mods_; // Destroyed before the registered blocks.
    ItemRegistry items_;
    std::vector<Recipe> recipes_;
    std::optional<MiningRules> mining_;
};
}
