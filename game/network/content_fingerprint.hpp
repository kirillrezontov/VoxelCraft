#pragma once
#include <cstdint>
#include <vector>

namespace vc {
namespace voxel { class BlockRegistry; }
class ItemRegistry;
class MiningRules;
class ModRuntime;
struct Recipe;
namespace netgame {
// Compute before saved/network palettes add missing-content fallbacks. This is
// content composition, independent of packet encoding and transport.
std::uint64_t contentFingerprint(const voxel::BlockRegistry& blocks, const ItemRegistry& items,
    const std::vector<Recipe>& recipes, const MiningRules& mining, const ModRuntime& mods);
}
}
