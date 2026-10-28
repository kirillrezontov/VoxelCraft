#include "game/simulation/creatures.hpp"
#include <entt/entity/registry.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace vc {
namespace {
struct Identity { std::uint64_t id; };
struct Motion { glm::dvec3 position{},velocity{};double yaw{},age{},phase{};bool grounded{}; };
struct Creature { CreatureKind kind;unsigned health;double rethink{},contact{},flee{};glm::dvec2 heading{}; };
struct Dropped { ItemStack stack; };
voxel::Aabb bounds(const Motion& motion,bool drop=false) {
    const double half=drop?0.16:0.35,height=drop?0.32:1.5;
    return {motion.position+glm::dvec3(-half,0,-half),motion.position+glm::dvec3(half,height,half)};
}
bool finite(glm::dvec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
std::optional<double> rayBox(glm::dvec3 origin,glm::dvec3 direction,const voxel::Aabb& box,double reach) {
    double near=0,far=reach;
    for(int axis=0;axis<3;++axis) {
        if(std::abs(direction[axis])<1e-12) { if(origin[axis]<box.min[axis]||origin[axis]>box.max[axis]) return {}; }
        else {
            double a=(box.min[axis]-origin[axis])/direction[axis],b=(box.max[axis]-origin[axis])/direction[axis];
            if(a>b) std::swap(a,b);
            near=std::max(near,a);far=std::min(far,b);if(near>far) return {};
        }
    }
    return near;
}
}
struct LivingWorld::Impl {
    entt::registry entities;
    std::uint64_t nextId{1},random;
    double accumulated{},spawnDelay{1},attackDelay{};
    explicit Impl(std::uint64_t seed):random(seed?seed:1) {}
    double rand() { random^=random<<13;random^=random>>7;random^=random<<17;return static_cast<double>(random>>11)*(1.0/9007199254740992.0); }
    entt::entity entity(std::uint64_t id) const {
        for(const auto e:entities.view<Identity>()) if(entities.get<Identity>(e).id==id) return e;
        return entt::null;
    }
    entt::entity create(glm::dvec3 feet) {
        if(!finite(feet)||glm::any(glm::greaterThan(glm::abs(feet),glm::dvec3(1e15)))) throw std::invalid_argument("Invalid actor position");
        if(nextId==UINT64_MAX) throw std::overflow_error("Actor identity exhausted");
        const auto e=entities.create();entities.emplace<Identity>(e,nextId++);entities.emplace<Motion>(e,feet);return e;
    }
};
LivingWorld::LivingWorld(std::uint64_t seed):impl_(std::make_unique<Impl>(seed)) {}
LivingWorld::~LivingWorld()=default;
std::size_t LivingWorld::creatureCount() const { return impl_->entities.view<Creature>().size(); }
std::size_t LivingWorld::dropCount() const { return impl_->entities.view<Dropped>().size(); }
std::optional<std::uint64_t> LivingWorld::spawn(CreatureKind kind,glm::dvec3 feet) {
    if(kind!=CreatureKind::Grazer&&kind!=CreatureKind::Stalker) throw std::invalid_argument("Invalid creature kind");
    if(creatureCount()>=32) return {};
    const auto e=impl_->create(feet);impl_->entities.emplace<Creature>(e,kind,kind==CreatureKind::Grazer?8u:12u);
    return impl_->entities.get<Identity>(e).id;
}
bool LivingWorld::drop(glm::dvec3 feet,ItemStack stack,const ItemRegistry& items) {
    const auto& item=items.definition(stack.item);
    if(stack.empty()||stack.count>item.stackSize||stack.durability>item.durability||(item.durability&&!stack.durability)) throw std::invalid_argument("Invalid dropped stack");
    if(dropCount()>=32) return false;
    const auto e=impl_->create(feet);impl_->entities.emplace<Dropped>(e,stack);return true;
}
CreatureEvents LivingWorld::update(double seconds,const voxel::World& world,const voxel::BlockRegistry& blocks,
        glm::dvec3 player,SurvivalState* survival,const ItemRegistry& items,bool night,bool populate) {
    if(!std::isfinite(seconds)||seconds<0||!finite(player)) throw std::invalid_argument("Invalid creature update");
    impl_->accumulated+=std::min(seconds,0.1);CreatureEvents events;
    constexpr double dt=1.0/60;
    while(impl_->accumulated+1e-12>=dt) {
        impl_->accumulated-=dt;impl_->attackDelay=std::max(0.0,impl_->attackDelay-dt);impl_->spawnDelay-=dt;
        if(populate&&impl_->spawnDelay<=0) {
            impl_->spawnDelay=3;
            if(creatureCount()<16) {
                for(int attempt=0;attempt<16;++attempt) {
                    const double angle=impl_->rand()*6.28318530718,distance=10+impl_->rand()*22;
                    const auto x=static_cast<std::int64_t>(std::floor(player.x+std::cos(angle)*distance));
                    const auto z=static_cast<std::int64_t>(std::floor(player.z+std::sin(angle)*distance));
                    bool placed=false;
                    for(int dy=16;dy>=-24;--dy) {
                        const auto y=static_cast<std::int64_t>(std::floor(player.y))+dy;const voxel::Position p{x,y,z};
                        if(!world.loaded(p)||!blocks.definition(world.block(p)).solid) continue;
                        Motion motion;motion.position={static_cast<double>(x)+0.5,static_cast<double>(y)+1.001,static_cast<double>(z)+0.5};
                        const voxel::Position above{x,y+1,z};
                        if(!blocks.definition(world.block(above)).fluid&&!voxel::collides(world,blocks,bounds(motion))) {
                            (void)spawn(night&&impl_->rand()<0.6?CreatureKind::Stalker:CreatureKind::Grazer,motion.position);placed=true;break;
                        }
                    }
                    if(placed) break;
                }
            }
        }
        std::vector<entt::entity> remove;
        for(const auto e:impl_->entities.view<Motion,Creature>()) {
            auto& motion=impl_->entities.get<Motion>(e);auto& creature=impl_->entities.get<Creature>(e);
            const glm::dvec3 toward=player-motion.position;const double distance=glm::length(glm::dvec2(toward.x,toward.z));
            if(distance>128) { remove.push_back(e);continue; }
            const voxel::Position cell{static_cast<std::int64_t>(std::floor(motion.position.x)),static_cast<std::int64_t>(std::floor(motion.position.y)),static_cast<std::int64_t>(std::floor(motion.position.z))};
            if(!world.loaded(cell)) continue;
            motion.age+=dt;creature.rethink-=dt;creature.contact=std::max(0.0,creature.contact-dt);creature.flee=std::max(0.0,creature.flee-dt);
            const bool chasing=creature.kind==CreatureKind::Stalker&&night&&survival&&survival->alive()&&distance<24;
            const bool fleeing=creature.flee>0&&distance<12;
            double speed=0;
            if((chasing||fleeing)&&distance>0.05) {
                creature.heading=glm::dvec2(toward.x,toward.z)/distance*(fleeing?-1.0:1.0);speed=chasing?2.1:2.7;
            } else {
                if(creature.rethink<=0) {
                    creature.rethink=1.5+impl_->rand()*3;const double angle=impl_->rand()*6.28318530718;
                    creature.heading=impl_->rand()<0.25?glm::dvec2(0):glm::dvec2(std::sin(angle),-std::cos(angle));
                }
                speed=0.7;
            }
            motion.velocity.x=creature.heading.x*speed;motion.velocity.z=creature.heading.y*speed;
            if(glm::length(creature.heading)>0.1) motion.yaw=std::atan2(-creature.heading.x,-creature.heading.y);
            const bool fluid=blocks.definition(world.block(cell)).fluid;
            motion.velocity.y=fluid?2:std::max(-40.0,motion.velocity.y-24*dt);
            const auto moved=voxel::moveAndCollide(world,blocks,bounds(motion),motion.velocity*dt);motion.position+=moved.applied;
            motion.grounded=moved.blocked.y&&!moved.unavailable.y&&motion.velocity.y<0;
            if(moved.blocked.y) motion.velocity.y=0;
            if(motion.grounded&&(moved.blocked.x||moved.blocked.z)) motion.velocity.y=7;
            motion.phase=std::fmod(motion.phase+glm::length(glm::dvec2(moved.applied.x,moved.applied.z))*7,6.28318530718);
            if(chasing&&distance<1.1&&std::abs(toward.y-1.5)<1.8&&creature.contact<=0) {
                // A solid wall occludes contact, including corners.
                const auto origin=motion.position+glm::dvec3(0,0.8,0);
                const auto ray=voxel::raycast(world,origin,player-origin,glm::length(player-origin),&blocks);
                if(!ray) { events.damage+=survival->damage(2);creature.contact=1; }
            }
        }
        for(const auto e:remove) impl_->entities.destroy(e);
        remove.clear();
        for(const auto e:impl_->entities.view<Motion,Dropped>()) {
            auto& motion=impl_->entities.get<Motion>(e);const auto& dropped=impl_->entities.get<Dropped>(e);
            const voxel::Position p{static_cast<std::int64_t>(std::floor(motion.position.x)),static_cast<std::int64_t>(std::floor(motion.position.y)),static_cast<std::int64_t>(std::floor(motion.position.z))};
            if(!world.loaded(p)) continue; // Retain distant drops without losing loot.
            motion.age+=dt;motion.phase=std::fmod(motion.phase+dt*2,6.28318530718);motion.yaw=std::remainder(motion.yaw+dt,6.28318530718);
            motion.velocity.y=std::max(-40.0,motion.velocity.y-24*dt);
            const auto moved=voxel::moveAndCollide(world,blocks,bounds(motion,true),motion.velocity*dt);motion.position+=moved.applied;
            if(moved.blocked.y) motion.velocity.y=0;
            const auto d=player-(motion.position+glm::dvec3(0,1,0));
            if(survival&&survival->alive()&&glm::dot(d,d)<2.25&&survival->inventory.add(dropped.stack.item,dropped.stack.count,items,dropped.stack.durability)) {
                ++events.collected;remove.push_back(e);
            }
        }
        for(const auto e:remove) impl_->entities.destroy(e);
    }
    return events;
}
std::optional<std::uint64_t> LivingWorld::target(const voxel::World& world,const voxel::BlockRegistry& blocks,glm::dvec3 origin,glm::dvec3 direction,double maxDistance) const {
    if(!finite(origin)||!finite(direction)||!std::isfinite(maxDistance)||maxDistance<0||maxDistance>6) throw std::invalid_argument("Invalid creature ray");
    if(glm::length(direction)<1e-12) return {};
    direction=glm::normalize(direction);std::optional<std::uint64_t> result;
    if(const auto block=voxel::raycast(world,origin,direction,maxDistance,&blocks)) maxDistance=block->distance;
    for(const auto e:impl_->entities.view<Motion,Creature>()) {
        const auto& motion=impl_->entities.get<Motion>(e);
        const voxel::Position p{static_cast<std::int64_t>(std::floor(motion.position.x)),static_cast<std::int64_t>(std::floor(motion.position.y)),static_cast<std::int64_t>(std::floor(motion.position.z))};
        if(!world.loaded(p)) continue;
        if(const auto distance=rayBox(origin,direction,bounds(motion),maxDistance)) {
            maxDistance=*distance;result=impl_->entities.get<Identity>(e).id;
        }
    }
    return result;
}
AttackResult LivingWorld::attack(std::uint64_t id,glm::dvec3 from,SurvivalState* survival,const ItemRegistry& items) {
    if(!finite(from)) throw std::invalid_argument("Invalid attack origin");
    const auto e=impl_->entity(id);
    if(e==entt::null||!impl_->entities.all_of<Creature>(e)||impl_->attackDelay>0||(survival&&!survival->alive())) return {};
    auto& creature=impl_->entities.get<Creature>(e);auto& motion=impl_->entities.get<Motion>(e);
    if(glm::length(from-(motion.position+glm::dvec3(0,0.75,0)))>4) return {};
    const auto& stack=survival?survival->inventory.selected():ItemStack{};
    const auto* tool=!stack.empty()?&items.definition(stack.item):nullptr;
    const unsigned damage=tool&&tool->tool==ToolKind::Pickaxe?(tool->tier>=2?5u:3u):2u;
    if(creature.health<=damage&&dropCount()>=32) return {false,false,true};
    impl_->attackDelay=0.45;creature.flee=3;creature.health-=std::min(damage,creature.health);
    if(survival&&tool&&tool->durability) survival->inventory.damageTool();
    if(!creature.health) {
        const auto loot=items.require(creature.kind==CreatureKind::Grazer?"voxelcraft:raw_meat":"voxelcraft:coal");
        const auto position=motion.position;impl_->entities.destroy(e);
        (void)drop(position,{loot,1,0},items);return {true,true,false};
    }
    motion.velocity.y=4;return {true,false,false};
}
std::vector<ActorView> LivingWorld::views() const {
    std::vector<ActorView> result;
    for(const auto e:impl_->entities.view<Identity,Motion>()) {
        const auto& motion=impl_->entities.get<Motion>(e);const auto* creature=impl_->entities.try_get<Creature>(e);
        result.push_back({impl_->entities.get<Identity>(e).id,creature?(creature->kind==CreatureKind::Grazer?0u:1u):2u,
            motion.position,static_cast<float>(motion.yaw),static_cast<float>(motion.phase),creature?creature->health:0});
    }
    return result;
}
std::vector<SavedActor> LivingWorld::save(const ItemRegistry& items) const {
    std::vector<SavedActor> result;
    for(const auto e:impl_->entities.view<Identity,Motion>()) {
        const auto& m=impl_->entities.get<Motion>(e);SavedActor actor;
        actor.id=impl_->entities.get<Identity>(e).id;actor.position=m.position;actor.velocity=m.velocity;actor.yaw=m.yaw;actor.age=m.age;actor.phase=m.phase;
        if(const auto* c=impl_->entities.try_get<Creature>(e)) { actor.kind=c->kind==CreatureKind::Grazer?"voxelcraft:grazer":"voxelcraft:stalker";actor.health=c->health; }
        else { const auto& stack=impl_->entities.get<Dropped>(e).stack;actor.kind="voxelcraft:dropped_item";actor.item={items.definition(stack.item).identifier,stack.count,stack.durability}; }
        result.push_back(std::move(actor));
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.id<b.id;});validateActors(result);return result;
}
void LivingWorld::restore(const std::vector<SavedActor>& actors,ItemRegistry& items) {
    validateActors(actors);auto next=std::make_unique<Impl>(impl_->random);auto resolved=items;
    for(const auto& actor:actors) {
        const auto e=next->create(actor.position);next->entities.get<Identity>(e).id=actor.id;next->nextId=std::max(next->nextId,actor.id+1);
        auto& m=next->entities.get<Motion>(e);m.velocity=actor.velocity;m.yaw=actor.yaw;m.age=actor.age;m.phase=actor.phase;
        if(actor.kind=="voxelcraft:dropped_item") {
            std::array<SavedItemStack,SurvivalInventory::slotCount> stacks{};stacks[0]=actor.item;SurvivalInventory temporary;temporary.restore(stacks,0,resolved);
            next->entities.emplace<Dropped>(e,temporary.selected());
        } else next->entities.emplace<Creature>(e,actor.kind=="voxelcraft:grazer"?CreatureKind::Grazer:CreatureKind::Stalker,actor.health);
    }
    items=std::move(resolved);impl_=std::move(next);
}
}
