#pragma once
#include "game/simulation/entity_data.hpp"
#include "game/gameplay/survival.hpp"
#include "engine/voxel/collision.hpp"
#include <memory>

namespace vc {
enum class CreatureKind { Grazer,Stalker };
struct ActorView {
    std::uint64_t id{};
    unsigned model{}; // Grazer 0, stalker 1, drop 2.
    glm::dvec3 position{};
    float yaw{},phase{};
    unsigned health{};
};
struct CreatureEvents { unsigned damage{},collected{}; };
struct AttackResult { bool hit{},killed{},noDropSpace{}; };
struct LivingPlayer { glm::dvec3 position{};SurvivalState* survival{}; };
class LivingWorld {
public:
    explicit LivingWorld(std::uint64_t seed);
    ~LivingWorld();
    LivingWorld(const LivingWorld&)=delete;
    LivingWorld& operator=(const LivingWorld&)=delete;
    std::optional<std::uint64_t> spawn(CreatureKind kind,glm::dvec3 feet);
    bool drop(glm::dvec3 feet,ItemStack stack,const ItemRegistry& items);
    CreatureEvents update(double seconds,const voxel::World& world,const voxel::BlockRegistry& blocks,
        glm::dvec3 player,SurvivalState* survival,const ItemRegistry& items,bool night,bool populate=true);
    CreatureEvents update(double seconds,const voxel::World& world,const voxel::BlockRegistry& blocks,
        std::span<const LivingPlayer> players,const ItemRegistry& items,bool night,bool populate=true);
    std::optional<std::uint64_t> target(const voxel::World& world,const voxel::BlockRegistry& blocks,
        glm::dvec3 origin,glm::dvec3 direction,double maxDistance=3) const;
    AttackResult attack(std::uint64_t id,glm::dvec3 from,SurvivalState* survival,const ItemRegistry& items,std::uint64_t attacker=0);
    std::vector<ActorView> views() const;
    std::vector<SavedActor> save(const ItemRegistry& items) const;
    void restore(const std::vector<SavedActor>& actors,ItemRegistry& items);
    std::size_t creatureCount() const;
    std::size_t dropCount() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
