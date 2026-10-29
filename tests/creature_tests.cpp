#include "game/simulation/creatures.hpp"
#include "game/player/save_state.hpp"
#include "tests/player_fixture.hpp"
#include "game/content/blocks.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace {
vc::voxel::World floorWorld(const vc::voxel::BlockRegistry& blocks) {
    vc::voxel::World world;
    for(int x=-1;x<=1;++x) for(int z=-1;z<=1;++z) {
        vc::voxel::Chunk chunk({x,0,z});
        for(int xx=0;xx<32;++xx) for(int zz=0;zz<32;++zz) chunk.set({xx,0,zz},blocks.require("voxelcraft:stone"));
        world.insert(std::move(chunk));
    }
    return world;
}
void advance(vc::LivingWorld& living,const vc::voxel::World& world,const vc::voxel::BlockRegistry& blocks,
        const vc::ItemRegistry& items,int frames,vc::SurvivalState* player=nullptr,bool night=false) {
    for(int i=0;i<frames;++i) living.update(1.0/60,world,blocks,{0.5,2.62,0.5},player,items,night,false);
}
}
TEST(Creatures, WanderNightChaseContactAndVoxelCollision) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);const auto world=floorWorld(blocks);
    vc::LivingWorld living(42);const auto grazer=living.spawn(vc::CreatureKind::Grazer,{-8.5,4,-8.5});ASSERT_TRUE(grazer);
    advance(living,world,blocks,items,240);const auto wandering=living.views();ASSERT_EQ(wandering.size(),1u);
    EXPECT_NE(wandering[0].position,(glm::dvec3{-8.5,4,-8.5}));EXPECT_GE(wandering[0].position.y,0.9999);
    EXPECT_LE(wandering[0].position.y,1.001);EXPECT_GT(wandering[0].phase,0);
    vc::SurvivalState survival;ASSERT_TRUE(living.spawn(vc::CreatureKind::Stalker,{8.5,1.001,0.5}));
    advance(living,world,blocks,items,600,&survival,true);EXPECT_LT(survival.health(),20u);
    EXPECT_EQ(world.chunks().size(),9u); // ECS never owns terrain cells.
    for(const auto& actor:living.views()) EXPECT_GE(actor.position.y,0.9999);
}
TEST(Creatures, RayTargetsNearestAndRespectsCallerOcclusion) {
    const auto blocks=vc::builtinBlocks();const auto world=floorWorld(blocks);
    vc::LivingWorld living(1);const auto far=living.spawn(vc::CreatureKind::Grazer,{0,1,-2.5});
    const auto near=living.spawn(vc::CreatureKind::Stalker,{0,1,-1});ASSERT_TRUE(far&&near);
    EXPECT_EQ(living.target(world,blocks,{0,2.2,1},{0,0,-10},6),near);
    EXPECT_FALSE(living.target(world,blocks,{0,2.2,1},{0,0,-1},1));
    EXPECT_FALSE(living.target(world,blocks,{0,2.2,1},{1,0,0},6));
    EXPECT_THROW(living.target(world,blocks,{},{0,0,-1},7),std::invalid_argument);
}
TEST(Creatures, CooldownToolCombatDropsCollectionAndFood) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);const auto world=floorWorld(blocks);
    vc::LivingWorld living(7);vc::SurvivalState survival;
    ASSERT_TRUE(survival.inventory.add(items.require("voxelcraft:stone_pickaxe"),1,items));
    const auto actor=living.spawn(vc::CreatureKind::Grazer,{0.5,1.001,-1});ASSERT_TRUE(actor);
    EXPECT_TRUE(living.attack(*actor,{0.5,2.62,0.5},&survival,items).hit);
    EXPECT_FALSE(living.attack(*actor,{0.5,2.62,0.5},&survival,items).hit);
    advance(living,world,blocks,items,30);const auto from=living.views().front().position+glm::dvec3(0,1,0);
    const auto hit=living.attack(*actor,from,&survival,items);EXPECT_TRUE(hit.killed);EXPECT_EQ(living.creatureCount(),0u);EXPECT_EQ(living.dropCount(),1u);
    EXPECT_EQ(survival.inventory.selected().durability,129);
    const auto loot=living.views().front().position;
    const auto events=living.update(0.1,world,blocks,loot+glm::dvec3(0,1,0),&survival,items,false,false);
    EXPECT_EQ(events.collected,1u);EXPECT_EQ(living.dropCount(),0u);EXPECT_EQ(survival.inventory.count(items.require("voxelcraft:raw_meat")),1u);
    EXPECT_EQ(survival.damage(8),8u);survival.inventory.select(1);
    EXPECT_TRUE(survival.eat(items));EXPECT_EQ(survival.health(),15u);EXPECT_TRUE(survival.inventory.selected().empty());
    EXPECT_FALSE(survival.eat(items));
}
TEST(Creatures, FullInventoryRetainsLootAndDropCapPreventsLoss) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);const auto world=floorWorld(blocks);
    vc::SurvivalState survival;ASSERT_TRUE(survival.inventory.add(items.require("voxelcraft:dirt"),36*64,items));
    vc::LivingWorld living(1);
    for(int i=0;i<32;++i) ASSERT_TRUE(living.drop({0.5,1.001,0.5},{items.require("voxelcraft:coal"),1,0},items));
    EXPECT_FALSE(living.drop({0,1,0},{items.require("voxelcraft:coal"),1,0},items));
    advance(living,world,blocks,items,2,&survival);EXPECT_EQ(living.dropCount(),32u);
    const auto creature=living.spawn(vc::CreatureKind::Grazer,{0,1,0});ASSERT_TRUE(creature);
    for(int attack=0;attack<3;++attack) {
        auto p=living.save(items).back().position; // Last identity belongs to creature.
        EXPECT_TRUE(living.attack(*creature,p+glm::dvec3(0,1,0),&survival,items).hit);
        advance(living,world,blocks,items,28);
    }
    const auto p=living.save(items).back().position;
    const auto blocked=living.attack(*creature,p+glm::dvec3(0,1,0),&survival,items);
    EXPECT_TRUE(blocked.noDropSpace);EXPECT_FALSE(blocked.hit);EXPECT_EQ(living.creatureCount(),1u);
    // Unloaded drops remain in the save and do not age out.
    living.update(0.1,world,blocks,{1000,2,1000},nullptr,items,false,false);EXPECT_EQ(living.dropCount(),32u);
}
TEST(Creatures, SaveStableIdentityMigrationMissingLootAndTransactionalRestore) {
    const auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);vc::LivingWorld living(42);
    ASSERT_TRUE(living.spawn(vc::CreatureKind::Grazer,{-0.5,5,-32.5}));
    ASSERT_TRUE(living.drop({1,3,4},{items.require("voxelcraft:coal"),3,0},items));
    vc::PlayerSaveState state;state.actors=living.save(items);const auto bytes=vc::encodePlayer(state);
    const auto decoded=vc::decodePlayer(bytes);ASSERT_EQ(decoded.actors.size(),2u);
    vc::LivingWorld restored(9);restored.restore(decoded.actors,items);const auto saved=restored.save(items);
    EXPECT_EQ(saved[0].id,state.actors[0].id);EXPECT_EQ(saved[0].position,state.actors[0].position);
    EXPECT_EQ(saved[1].item.identifier,"voxelcraft:coal");EXPECT_EQ(saved[1].item.count,3);
    auto missing=decoded.actors;missing[1].item.identifier="example:lost_loot";restored.restore(missing,items);
    EXPECT_TRUE(items.find("example:lost_loot"));EXPECT_EQ(restored.save(items)[1].item.identifier,"example:lost_loot");
    auto invalid=missing;invalid[1].id=invalid[0].id;
    EXPECT_THROW(restored.restore(invalid,items),std::runtime_error);EXPECT_EQ(restored.save(items).size(),2u);
    invalid=missing;invalid[0].velocity.x=std::numeric_limits<double>::infinity();EXPECT_THROW(restored.restore(invalid,items),std::runtime_error);
    invalid=missing;invalid[0].kind="unknown:type";EXPECT_THROW(restored.restore(invalid,items),std::runtime_error);
    const auto versionFour=vc::test::legacyPlayer(4);
    EXPECT_TRUE(vc::decodePlayer(versionFour).actors.empty());
}
TEST(Creatures, BoundedPopulationAndDaytimeSafety) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);const auto world=floorWorld(blocks);
    vc::LivingWorld living(42);vc::SurvivalState survival;
    ASSERT_TRUE(living.spawn(vc::CreatureKind::Stalker,{0.5,1.001,0.5}));
    advance(living,world,blocks,items,120,&survival,false);EXPECT_EQ(survival.health(),20u);
    for(int i=0;i<31;++i) EXPECT_TRUE(living.spawn(vc::CreatureKind::Grazer,{10,1.001,10}));
    EXPECT_FALSE(living.spawn(vc::CreatureKind::Grazer,{}));EXPECT_EQ(living.creatureCount(),32u);
    EXPECT_THROW(living.update(-1,world,blocks,{},nullptr,items,false),std::invalid_argument);
}
TEST(Creatures, MultiplePlayersRetainDistantCreaturesAndReceiveIndependentCombat) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);auto world=floorWorld(blocks);
    for(int x=15;x<=17;++x) for(int z=-1;z<=1;++z) {
        vc::voxel::Chunk chunk({x,0,z});
        for(int a=0;a<32;++a) for(int b=0;b<32;++b) chunk.set({a,0,b},blocks.require("voxelcraft:stone"));
        world.insert(std::move(chunk));
    }
    vc::LivingWorld living(42);const auto near=living.spawn(vc::CreatureKind::Grazer,{1,1.001,1});
    const auto far=living.spawn(vc::CreatureKind::Grazer,{513,1.001,1});ASSERT_TRUE(near&&far);
    const std::array players{vc::LivingPlayer{{0.5,2.62,0.5},nullptr},vc::LivingPlayer{{512.5,2.62,0.5},nullptr}};
    for(int i=0;i<60;++i) living.update(1.0/60,world,blocks,players,items,false,false);
    EXPECT_EQ(living.creatureCount(),2u);
    const auto position=living.save(items)[0].position+glm::dvec3(0,0.75,0);
    EXPECT_TRUE(living.attack(*near,position,nullptr,items,1).hit);
    EXPECT_FALSE(living.attack(*near,position,nullptr,items,1).hit);
    EXPECT_TRUE(living.attack(*near,position,nullptr,items,2).hit);
    living.update(0.1,world,blocks,std::span<const vc::LivingPlayer>{},items,false,false);EXPECT_EQ(living.creatureCount(),2u);
}
TEST(Creatures, MultiplayerLootCanPassFullInventoryAndStalkersTargetSurvivalPlayers) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);const auto world=floorWorld(blocks);
    vc::SurvivalState full,recipient;ASSERT_TRUE(full.inventory.add(items.require("voxelcraft:dirt"),36*64,items));
    vc::LivingWorld living(1);ASSERT_TRUE(living.drop({0.5,1.001,0.5},{items.require("voxelcraft:coal"),1,0},items));
    const std::array players{vc::LivingPlayer{{0.5,2.62,0.5},&full},vc::LivingPlayer{{0.5,2.62,0.6},&recipient}};
    const auto events=living.update(0.1,world,blocks,players,items,false,false);EXPECT_EQ(events.collected,1u);EXPECT_EQ(living.dropCount(),0u);
    EXPECT_EQ(recipient.inventory.count(items.require("voxelcraft:coal")),1u);EXPECT_EQ(full.inventory.count(items.require("voxelcraft:coal")),0u);
    ASSERT_TRUE(living.spawn(vc::CreatureKind::Stalker,{0.5,1.001,0.5}));
    const std::array targets{vc::LivingPlayer{{0.5,2.62,0.6},nullptr},vc::LivingPlayer{{0.5,2.62,1.0},&recipient}};
    for(int i=0;i<60;++i) living.update(1.0/60,world,blocks,targets,items,true,false);
    EXPECT_LT(recipient.health(),20u);
}
