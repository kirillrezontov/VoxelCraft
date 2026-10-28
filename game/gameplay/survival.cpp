#include "game/gameplay/survival.hpp"
#include "game/gameplay/interaction.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vc {
MiningRules::MiningRules(const voxel::BlockRegistry& blocks,const ItemRegistry& items) {
    rules_.resize(blocks.size());
    for(std::size_t i=1;i<blocks.size();++i) rules_[i].drop=items.find(blocks.definition(static_cast<voxel::BlockId>(i)).identifier);
    const auto set=[&](std::string_view block,MiningRule rule) { if(const auto id=blocks.find(block)) rules_[*id]=rule; };
    set("voxelcraft:grass",{0.6,ToolKind::None,0,items.require("voxelcraft:dirt")});
    set("voxelcraft:dirt",{0.45,ToolKind::None,0,items.require("voxelcraft:dirt")});
    set("voxelcraft:stone",{1.5,ToolKind::Pickaxe,1,items.require("voxelcraft:cobblestone")});
    set("voxelcraft:cobblestone",{2,ToolKind::Pickaxe,1,items.require("voxelcraft:cobblestone")});
    set("voxelcraft:log",{1.3,ToolKind::None,0,items.require("voxelcraft:log")});
    set("voxelcraft:planks",{0.8,ToolKind::None,0,items.require("voxelcraft:planks")});
    set("voxelcraft:leaves",{0.2,ToolKind::None,0,std::nullopt});
    set("voxelcraft:coal_ore",{3,ToolKind::Pickaxe,1,items.require("voxelcraft:coal")});
    set("voxelcraft:iron_ore",{3.5,ToolKind::Pickaxe,2,items.require("voxelcraft:iron_ore")});
    set("voxelcraft:water",{1,ToolKind::None,0,std::nullopt});
    set("voxelcraft:sand",{0.5,ToolKind::None,0,items.require("voxelcraft:sand")});
    set("voxelcraft:snow",{0.5,ToolKind::None,0,items.require("voxelcraft:snow")});
    set("voxelcraft:sandstone",{1.8,ToolKind::Pickaxe,1,items.require("voxelcraft:sandstone")});
}
void SurvivalState::cancelMining() { target_.reset();progress_=0;tool_.reset();fullReported_=false; }
MiningResult SurvivalState::mine(bool held,const std::optional<voxel::RayHit>& hit,double seconds,
        voxel::World& world,const ItemRegistry& items,const MiningRules& rules) {
    if(!std::isfinite(seconds)||seconds<0) throw std::invalid_argument("Mining delta must be finite and nonnegative");
    if(!held||!hit||!alive()||!world.loaded(hit->block)||world.block(hit->block)==voxel::air) { cancelMining();return {}; }
    const auto block=world.block(hit->block);const auto& stack=inventory.selected();
    const auto tool=stack.empty()?std::nullopt:std::optional(stack.item);
    if(target_!=hit->block||targetBlock_!=block||tool_!=tool||slot_!=inventory.selectedSlot()) {
        cancelMining();target_=hit->block;targetBlock_=block;tool_=tool;slot_=inventory.selectedSlot();
    }
    const auto& rule=rules.rule(block);
    const auto* definition=tool?&items.definition(*tool):nullptr;
    const bool matching=definition&&definition->tool==rule.tool&&rule.tool!=ToolKind::None;
    const bool harvest=rule.requiredTier==0||(matching&&definition->tier>=rule.requiredTier);
    const double duration=rule.seconds/(matching?definition->miningSpeed:1)*(harvest?1:3);
    progress_=std::min(1.0,progress_+std::min(seconds,0.1)/duration);
    if(progress_+1e-10<1) return {};
    auto next=inventory;
    if(harvest&&rule.drop&&!next.add(*rule.drop,1,items)) {
        const bool report=!fullReported_;fullReported_=true;return {false,report,false};
    }
    if(!destroyBlock(world,*hit)) { cancelMining();return {}; }
    const bool toolBroke=definition&&definition->tool!=ToolKind::None&&stack.durability==1;
    if(definition&&definition->tool!=ToolKind::None) next.damageTool();
    inventory=std::move(next);cancelMining();return {true,false,toolBroke};
}
unsigned SurvivalState::land(double speed) {
    if(!std::isfinite(speed)||speed<0||speed>1000) throw std::invalid_argument("Invalid landing impact speed");
    const auto damage=static_cast<unsigned>(std::max(0.0,std::floor(speed*speed/48-3)));
    const auto applied=std::min(health_,damage);health_-=applied;return applied;
}
void SurvivalState::restoreHealth(unsigned health) {
    if(health>20) throw std::runtime_error("Saved health exceeds maximum");
    health_=health;cancelMining();
}
void SurvivalState::respawn() { health_=20;cancelMining(); }
unsigned SurvivalState::damage(unsigned amount) {
    const auto applied=std::min(health_,amount);health_-=applied;return applied;
}
bool SurvivalState::eat(const ItemRegistry& items) {
    const auto& stack=inventory.selected();
    if(!alive()||health_==20||stack.empty()||items.definition(stack.item).identifier!="voxelcraft:raw_meat") return false;
    if(!inventory.consumeSelected()) return false;
    health_=std::min(20u,health_+3);return true;
}
}
