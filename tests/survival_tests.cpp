#include "game/gameplay/survival.hpp"
#include "game/player/controller.hpp"
#include "game/player/save_state.hpp"
#include "game/player/spawn.hpp"
#include "engine/voxel/streaming.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include <gtest/gtest.h>
#include <cmath>

namespace {
vc::voxel::RayHit hit(vc::voxel::Position p) { return {p,{0,1,0},1}; }
vc::MiningResult finish(vc::SurvivalState& state,const vc::voxel::RayHit& target,vc::voxel::World& world,
        const vc::ItemRegistry& items,const vc::MiningRules& rules,int steps=100) {
    for(int i=0;i<steps;++i) {
        const auto result=state.mine(true,target,0.1,world,items,rules);
        if(result.changed||result.inventoryFull) return result;
    }
    return {};
}
}
TEST(SurvivalInventory, StacksTransfersAndFullInventoryAreTransactional) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);const auto dirt=items.require("voxelcraft:dirt");
    vc::SurvivalInventory inventory;ASSERT_TRUE(inventory.add(dirt,140,items));
    EXPECT_EQ(inventory.slots()[0].count,64);EXPECT_EQ(inventory.slots()[1].count,64);EXPECT_EQ(inventory.slots()[2].count,12);
    EXPECT_TRUE(inventory.moveToBackpack(0,items));EXPECT_TRUE(inventory.slots()[0].empty());EXPECT_EQ(inventory.slots()[9].count,64);
    EXPECT_TRUE(inventory.transfer(9,2,10,items));EXPECT_EQ(inventory.slots()[2].count,22);EXPECT_EQ(inventory.count(dirt),140u);
    EXPECT_TRUE(inventory.swap(0,9));EXPECT_EQ(inventory.selected().count,54);
    vc::SurvivalInventory full;ASSERT_TRUE(full.add(dirt,36*64,items));const auto before=full.slots();
    EXPECT_FALSE(full.add(dirt,1,items));EXPECT_EQ(full.slots(),before);
    EXPECT_FALSE(full.moveToBackpack(0,items));EXPECT_EQ(full.slots(),before);
}
TEST(Crafting, WoodToStoneToolProgressionAndNoSpaceRollback) {
    const auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);const auto recipes=vc::builtinRecipes(items);
    vc::SurvivalInventory inventory;ASSERT_TRUE(inventory.add(items.require("voxelcraft:log"),2,items));
    EXPECT_EQ(inventory.craft(recipes[0],items),vc::CraftResult::Crafted);EXPECT_EQ(inventory.craft(recipes[0],items),vc::CraftResult::Crafted);
    EXPECT_EQ(inventory.count(items.require("voxelcraft:planks")),8u);
    EXPECT_EQ(inventory.craft(recipes[1],items),vc::CraftResult::Crafted);
    EXPECT_EQ(inventory.craft(recipes[2],items),vc::CraftResult::Crafted);
    EXPECT_EQ(inventory.count(items.require("voxelcraft:wooden_pickaxe")),1u);
    EXPECT_EQ(inventory.craft(recipes[3],items),vc::CraftResult::MissingIngredients);
    inventory.add(items.require("voxelcraft:cobblestone"),3,items);
    EXPECT_EQ(inventory.craft(recipes[3],items),vc::CraftResult::Crafted);
    EXPECT_EQ(inventory.count(items.require("voxelcraft:stone_pickaxe")),1u);
    std::array<vc::SavedItemStack,36> packed;
    for(auto& stack:packed) stack={"voxelcraft:dirt",64,0};
    packed[0]={"voxelcraft:log",2,0};
    vc::SurvivalInventory full;full.restore(packed,0,items);const auto before=full.slots();
    EXPECT_EQ(full.craft(recipes[0],items),vc::CraftResult::NoSpace);EXPECT_EQ(full.slots(),before);
}
TEST(SurvivalInventory, MissingItemIdentityDurabilityAndSavePayloadSurvive) {
    auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);const auto custom=items.add({"example:tool",{},1,20,vc::ToolKind::Pickaxe,3,8});
    vc::SurvivalInventory inventory;inventory.add(custom,1,items,7);
    vc::PlayerSaveState state;state.survival=true;state.health=13;state.items=inventory.save(items);
    const auto decoded=vc::decodePlayer(vc::encodePlayer(state));EXPECT_TRUE(decoded.survival);EXPECT_EQ(decoded.health,13u);
    auto missing=vc::builtinItems(blocks);vc::SurvivalInventory restored;restored.restore(decoded.items,0,missing);
    EXPECT_TRUE(missing.definition(restored.selected().item).missing);EXPECT_EQ(restored.selected().durability,7);
    EXPECT_EQ(restored.save(missing),state.items);
    auto invalid=decoded.items;invalid[0].count=2;const auto before=restored.slots();
    EXPECT_THROW(restored.restore(invalid,0,missing),std::runtime_error);EXPECT_EQ(restored.slots(),before);
}
TEST(Mining, TimedCancellationHarvestTierAndToolWear) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);const vc::MiningRules rules(blocks,items);
    vc::voxel::World world;world.insert(vc::voxel::Chunk({0,0,0}));
    const auto stone=blocks.require("voxelcraft:stone");world.setBlock({1,1,1},stone);
    vc::SurvivalState state;
    for(int i=0;i<10;++i) EXPECT_FALSE(state.mine(true,hit({1,1,1}),0.1,world,items,rules).changed);
    EXPECT_GT(state.miningProgress(),0);state.mine(false,std::nullopt,0.1,world,items,rules);EXPECT_EQ(state.miningProgress(),0);
    EXPECT_TRUE(finish(state,hit({1,1,1}),world,items,rules).changed);
    EXPECT_EQ(state.inventory.count(items.require("voxelcraft:cobblestone")),0u);
    world.setBlock({1,1,1},stone);state.inventory.add(items.require("voxelcraft:wooden_pickaxe"),1,items);
    EXPECT_TRUE(finish(state,hit({1,1,1}),world,items,rules).changed);
    EXPECT_EQ(state.inventory.count(items.require("voxelcraft:cobblestone")),1u);EXPECT_EQ(state.inventory.selected().durability,58);
    world.setBlock({2,1,1},blocks.require("voxelcraft:iron_ore"));
    EXPECT_TRUE(finish(state,hit({2,1,1}),world,items,rules).changed);
    EXPECT_EQ(state.inventory.count(items.require("voxelcraft:iron_ore")),0u);
}
TEST(Mining, FullInventoryKeepsWorldAndToolBreaksAfterSuccessfulHarvest) {
    const auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);const vc::MiningRules rules(blocks,items);
    vc::voxel::World world;world.insert(vc::voxel::Chunk({0,0,0}));world.setBlock({1,1,1},blocks.require("voxelcraft:dirt"));
    vc::SurvivalState state;state.inventory.add(items.require("voxelcraft:log"),36*64,items);
    EXPECT_TRUE(finish(state,hit({1,1,1}),world,items,rules).inventoryFull);
    EXPECT_EQ(world.block({1,1,1}),blocks.require("voxelcraft:dirt"));
    EXPECT_FALSE(state.mine(true,hit({1,1,1}),0.1,world,items,rules).inventoryFull);
    state.inventory.consumeSelected(64);EXPECT_TRUE(finish(state,hit({1,1,1}),world,items,rules).changed);
    EXPECT_EQ(state.inventory.count(items.require("voxelcraft:dirt")),1u);
    vc::SurvivalState fragile;std::array<vc::SavedItemStack,36> saved;saved[0]={"voxelcraft:wooden_pickaxe",1,1};fragile.inventory.restore(saved,0,items);
    world.setBlock({2,1,1},blocks.require("voxelcraft:stone"));const auto result=finish(fragile,hit({2,1,1}),world,items,rules);
    EXPECT_TRUE(result.changed);EXPECT_TRUE(result.toolBroke);EXPECT_TRUE(fragile.inventory.selected().empty());
    EXPECT_EQ(fragile.inventory.count(items.require("voxelcraft:cobblestone")),1u);
}
TEST(Survival, FallingReportsOneImpactAndRespawnKeepsInventory) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);vc::voxel::World world;
    vc::voxel::Chunk chunk({0,0,0});for(int x=0;x<32;++x) for(int z=0;z<32;++z) chunk.set({x,0,z},blocks.require("voxelcraft:stone"));world.insert(std::move(chunk));
    vc::PlayerController player;player.camera.position={4.5,16.62,4.5};vc::SurvivalState state;
    state.inventory.add(items.require("voxelcraft:log"),2,items);unsigned impacts=0;
    for(int frame=0;frame<180;++frame) { player.update({},1.0/60,world,blocks);if(state.land(player.landingSpeed)) ++impacts; }
    EXPECT_EQ(impacts,1u);EXPECT_LT(state.health(),20u);EXPECT_GT(state.health(),0u);EXPECT_TRUE(player.grounded);
    state.land(55);EXPECT_FALSE(state.alive());state.respawn();EXPECT_EQ(state.health(),20u);
    EXPECT_EQ(state.inventory.count(items.require("voxelcraft:log")),2u);
}
TEST(Survival, UnavailableStreamedFloorDoesNotCauseDamageOrGrantJumping) {
    const auto blocks=vc::builtinBlocks();vc::voxel::World world;world.insert(vc::voxel::Chunk({0,1,0}));
    vc::PlayerController player;player.camera.position={4.5,34.62,4.5};vc::SurvivalState state;
    for(int frame=0;frame<90;++frame) { player.update({},1.0/60,world,blocks);EXPECT_EQ(player.landingSpeed,0);state.land(player.landingSpeed); }
    EXPECT_EQ(state.health(),20u);EXPECT_FALSE(player.grounded);EXPECT_NEAR(player.bounds().min.y,32,1e-5);
    vc::voxel::Chunk floor({0,0,0});for(int x=0;x<32;++x) for(int z=0;z<32;++z) floor.set({x,0,z},blocks.require("voxelcraft:stone"));world.insert(std::move(floor));
    bool landed=false;
    for(int frame=0;frame<180;++frame) { player.update({},1.0/60,world,blocks);if(player.landingSpeed>1) landed=true;state.land(player.landingSpeed); }
    EXPECT_TRUE(landed);EXPECT_TRUE(player.grounded);EXPECT_FALSE(state.alive());
}
TEST(SurvivalUi, CraftingBackpackAndDeathUseActualInventory) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);const auto recipes=vc::builtinRecipes(items);
    vc::SurvivalState state;state.inventory.add(items.require("voxelcraft:log"),2,items);
    vc::CreativeInventory creative(blocks);vc::SurvivalUiContext context{&state,&items,&recipes};vc::SandboxUiController ui(creative,{},&context);
    EXPECT_TRUE(ui.process({.craftingToggle=true},1280,720));EXPECT_TRUE(ui.craftingOpen());
    const vc::SandboxUiLayout layout(1280,720);const auto recipe=layout.recipeButton(0);
    ui.process({.primary=true,.x=recipe.x+1,.y=recipe.y+1},1280,720);
    EXPECT_EQ(state.inventory.count(items.require("voxelcraft:planks")),4u);EXPECT_EQ(state.inventory.count(items.require("voxelcraft:log")),1u);
    EXPECT_TRUE(ui.rebuild(blocks,vc::builtinAtlasLayout(),1280,720,1));
    EXPECT_FALSE(ui.rebuild(blocks,vc::builtinAtlasLayout(),1280,720,2));
    ui.process({.toggle=true},1280,720);EXPECT_TRUE(ui.open());EXPECT_FALSE(ui.craftingOpen());
    const auto slot=layout.hotbarSlot(0);ui.process({.secondary=true,.x=slot.x+1,.y=slot.y+1},1280,720);
    EXPECT_TRUE(state.inventory.slots()[0].empty());EXPECT_EQ(state.inventory.slots()[9].count,1);
    const auto pack=layout.backpackCell(0);ui.process({.primary=true,.x=pack.x+1,.y=pack.y+1},1280,720);
    EXPECT_EQ(state.inventory.selected().item,items.require("voxelcraft:log"));
    state.land(55);EXPECT_TRUE(ui.open());EXPECT_FALSE(ui.process({.toggle=true},1280,720));
    auto hud=ui.rebuild(blocks,vc::builtinAtlasLayout(),1280,720,3);ASSERT_TRUE(hud);EXPECT_FALSE(hud->foreground.indices.empty());
    state.respawn();ui.closeMenus();EXPECT_FALSE(ui.open());
}
TEST(Spawn, AvoidsEditedObstructionsAndUnsupportedHeights) {
    const auto blocks=vc::builtinBlocks();const vc::voxel::TerrainGenerator generator(42,{},blocks,2);
    auto spawn=vc::findSpawn(generator,blocks);
    vc::voxel::World edits;const auto surface=generator.surfaceY(0,0);
    const auto p=vc::voxel::chunkPosition({0,surface+1,0});edits.insert(generator.generate(p));
    edits.setBlock({0,surface+1,0},blocks.require("voxelcraft:stone"));
    spawn=vc::findSpawn(generator,blocks,&edits);EXPECT_FALSE(spawn.x==0.5&&spawn.z==0.5);
    const vc::voxel::TerrainGenerator far(42,{2000000000000000LL,2000000000000000LL},blocks);
    EXPECT_THROW(vc::findSpawn(far,blocks),std::runtime_error);
}
TEST(Spawn, AvoidsOtherBodiesAndConsultsRetainedEditsWithoutResidency) {
    const auto blocks=vc::builtinBlocks();const vc::voxel::TerrainGenerator generator(42,{},blocks,2);
    vc::voxel::World world;const auto initial=vc::findSpawn(generator,blocks);
    vc::PlayerController first;first.camera.position=initial;const std::array bodies{first.bounds()};
    const auto next=vc::findSpawn(generator,blocks,&world,bodies);vc::PlayerController second;second.camera.position=next;
    EXPECT_FALSE(first.bounds().intersects(second.bounds()));
    const auto x=static_cast<std::int64_t>(std::floor(initial.x)),z=static_cast<std::int64_t>(std::floor(initial.z)),y=generator.surfaceY(x,z)+1;
    const vc::voxel::Position blocked{x,y,z};world.insert(generator.generate(vc::voxel::chunkPosition(blocked)));
    ASSERT_TRUE(world.setBlock(blocked,blocks.require("voxelcraft:stone")));
    const std::array retained{vc::voxel::chunkPosition(blocked)};vc::voxel::ChunkStreamer streamer(world,generator,1);streamer.retain(retained);
    streamer.tick(std::span<const vc::voxel::Position>{});EXPECT_TRUE(world.chunks().empty());
    const auto restored=vc::findSpawn(generator,blocks,streamer);EXPECT_NE(restored,initial);EXPECT_TRUE(world.chunks().empty());
}
TEST(SurvivalUi, RemoteActionsRequestServerTransactionsWithoutChangingTheMirror) {
    const auto blocks=vc::builtinBlocks();const auto items=vc::builtinItems(blocks);const auto recipes=vc::builtinRecipes(items);
    vc::SurvivalState state;ASSERT_TRUE(state.inventory.add(items.require("voxelcraft:log"),1,items));
    vc::CreativeInventory creative(blocks);std::vector<vc::InventoryAction> actions;
    vc::SurvivalUiContext context{&state,&items,&recipes,[&](auto action){actions.push_back(action);}};
    vc::SandboxUiController ui(creative,{},&context);const vc::SandboxUiLayout layout(1280,720);
    vc::SandboxUiInput toggle;toggle.craftingToggle=true;EXPECT_TRUE(ui.process(toggle,1280,720));
    auto area=layout.recipeButton(0);vc::SandboxUiInput click;click.primary=true;click.x=area.x+1;click.y=area.y+1;ui.process(click,1280,720);
    ASSERT_EQ(actions.size(),1u);EXPECT_EQ(actions[0].kind,vc::InventoryActionKind::Craft);EXPECT_EQ(actions[0].recipe,0u);
    EXPECT_EQ(state.inventory.count(items.require("voxelcraft:log")),1u);EXPECT_EQ(state.inventory.count(items.require("voxelcraft:planks")),0u);EXPECT_TRUE(ui.takeCraftedItems().empty());
    EXPECT_EQ(vc::applyInventoryAction(state.inventory,actions[0],items,recipes),vc::InventoryActionResult::Changed);
    EXPECT_EQ(state.inventory.count(items.require("voxelcraft:planks")),4u);
}
