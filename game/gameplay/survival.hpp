#pragma once
#include "game/gameplay/items.hpp"
#include "engine/voxel/raycast.hpp"

namespace vc {
struct MiningRule {
    double seconds{1};
    ToolKind tool{ToolKind::None};
    unsigned requiredTier{};
    std::optional<ItemId> drop;
};
class MiningRules {
public:
    MiningRules(const voxel::BlockRegistry& blocks,const ItemRegistry& items);
    const MiningRule& rule(voxel::BlockId id) const { return rules_.at(id); }
private:
    std::vector<MiningRule> rules_;
};
struct MiningResult { bool changed{},inventoryFull{},toolBroke{}; };
class SurvivalState {
public:
    SurvivalInventory inventory;
    unsigned health() const { return health_; }
    bool alive() const { return health_>0; }
    double miningProgress() const { return progress_; }
    bool mining() const { return target_.has_value(); }
    MiningResult mine(bool held,const std::optional<voxel::RayHit>& hit,double seconds,
        voxel::World& world,const ItemRegistry& items,const MiningRules& rules);
    void cancelMining();
    unsigned land(double speed);
    unsigned damage(unsigned amount);
    bool eat(const ItemRegistry& items);
    void restoreHealth(unsigned health);
    void respawn();
private:
    unsigned health_{20};
    std::optional<voxel::Position> target_;
    voxel::BlockId targetBlock_{};
    std::optional<ItemId> tool_;
    std::size_t slot_{};
    double progress_{};
    bool fullReported_{};
};
}
