#include "game/modding/runtime.hpp"
#include "game/player/save_state.hpp"
#include "game/content/blocks.hpp"
#include "game/content/atlas.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "engine/voxel/storage.hpp"
#include "engine/voxel/mesher.hpp"
#include "engine/core/binary.hpp"
#include "tests/temp_directory.hpp"
#include "tests/player_fixture.hpp"
#include <gtest/gtest.h>
#include <fstream>

namespace {
void write(const std::filesystem::path& path,std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path,std::ios::binary);file<<text;
    if(!file) throw std::runtime_error("Cannot write test mod");
}
void pack(const std::filesystem::path& root,std::string_view id,std::string_view content="{}",
        std::string_view script={},std::string_view dependencies="{}") {
    const auto directory=root/std::string(id);
    write(directory/"mod.json","{\"id\":\""+std::string(id)+"\",\"version\":\"1.0.0\",\"api\":1,\"dependencies\":"+std::string(dependencies)+"}");
    write(directory/"content.json",content);
    if(!script.empty()) write(directory/"init.lua",script);
}
struct Modding : testing::Test {
    vc::test::TempDirectory temp;
    vc::voxel::BlockRegistry blocks=vc::builtinBlocks();
    vc::voxel::World world;
    vc::SurvivalState survival;
    vc::ItemRegistry items=vc::builtinItems(blocks);
    const vc::voxel::Aabb player{{0.2,1,0.2},{0.8,2.8,0.8}};
    Modding() {
        world.insert(vc::voxel::Chunk({0,0,0}));
        world.insert(vc::voxel::Chunk({-1,0,0}));
        (void)world.takeDirtyChunks();
    }
    vc::ModContext context(bool creative=false) { return {world,blocks,items,creative?nullptr:&survival,player}; }
};
}

TEST_F(Modding, EmptyRootsAndDeterministicDependencyOrder) {
    vc::ModRuntime none({},blocks);EXPECT_EQ(none.size(),0u);
    vc::ModRuntime absent(temp.path()/"absent",blocks);EXPECT_TRUE(absent.save().empty());
    pack(temp.path(),"aaa","{}",{},R"({"zzz":"1.0.0"})");pack(temp.path(),"zzz");pack(temp.path(),"bbb");
    vc::ModRuntime mods(temp.path(),blocks);
    EXPECT_EQ(mods.order(),(std::vector<std::string>{"zzz","aaa","bbb"}));
}

TEST_F(Modding, ContentCraftingAndPagedUiUseRegisteredItems) {
    pack(temp.path(),"demo",R"({
      "blocks":[{"id":"demo:glowstone","texture":"stone","color":[0.2,0.8,1],"emission":12}],
      "items":[{"id":"demo:pickaxe","stack_size":1,"durability":200,"tool":"pickaxe","tier":3,"mining_speed":8}],
      "recipes":[{"name":"GLOWSTONE","ingredients":{"voxelcraft:dirt":1},"output":"demo:glowstone","count":1}]
    })");
    vc::ModRuntime mods(temp.path(),blocks);
    auto loadedItems=vc::builtinItems(blocks);auto recipes=vc::builtinRecipes(loadedItems);
    mods.installItems(loadedItems,blocks,recipes);
    EXPECT_EQ(blocks.definition(blocks.require("demo:glowstone")).emission,12);
    EXPECT_EQ(loadedItems.definition(loadedItems.require("demo:pickaxe")).durability,200);
    ASSERT_EQ(recipes.size(),6u);
    vc::CreativeInventory creative(blocks);vc::SurvivalUiContext uiContext{&survival,&loadedItems,&recipes};
    vc::SandboxUiController ui(creative,{},&uiContext);
    ASSERT_TRUE(survival.inventory.add(loadedItems.require("voxelcraft:dirt"),1,loadedItems));
    EXPECT_TRUE(ui.process({.craftingToggle=true},1280,720));
    ui.process({.scroll=-1},1280,720);EXPECT_EQ(ui.page(),1u);
    auto hud=ui.rebuild(blocks,vc::builtinAtlasLayout(),1280,720,0);ASSERT_TRUE(hud);EXPECT_FALSE(hud->textured.indices.empty());
    const auto button=vc::SandboxUiLayout(1280,720).recipeButton(0);
    ui.process({.primary=true,.x=button.x+1,.y=button.y+1},1280,720);
    EXPECT_EQ(survival.inventory.count(loadedItems.require("demo:glowstone")),1u);
    EXPECT_EQ(survival.inventory.count(loadedItems.require("voxelcraft:dirt")),0u);
    EXPECT_EQ(ui.takeCraftedItems(),(std::vector<std::string>{"demo:glowstone"}));EXPECT_TRUE(ui.takeCraftedItems().empty());
    ui.process({.primary=true,.x=button.x+1,.y=button.y+1},1280,720);EXPECT_TRUE(ui.takeCraftedItems().empty());
    ui.process({.scroll=-1000},1280,720);EXPECT_EQ(ui.page(),1u);
    ui.process({.scroll=1000},1280,720);EXPECT_EQ(ui.page(),0u);
}

TEST_F(Modding, RejectsBadPacksWithoutPartialBlockRegistration) {
    const auto before=blocks.size();
    pack(temp.path(),"demo",R"({"blocks":[{"id":"demo:valid"},{"id":"voxelcraft:bad"}]})");
    EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::exception);EXPECT_EQ(blocks.size(),before);
    pack(temp.path(),"demo",R"({"blocks":[{"id":"demo:valid"}],"recipes":[{"name":"BAD","ingredients":{"absent:item":1},"output":"demo:valid","count":1}]})");
    EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::exception);EXPECT_EQ(blocks.size(),before);
    pack(temp.path(),"demo","{}","error({})");
    EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::runtime_error);EXPECT_EQ(blocks.size(),before);
    write(temp.path()/"demo"/"init.lua","return { on_typo = function() end }");
    EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::runtime_error);
    write(temp.path()/"demo"/"mod.json",R"({"id":"demo","version":"1.0.0","api":2})");
    EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::exception);
}

TEST_F(Modding, RejectsMissingMismatchedAndCyclicDependencies) {
    pack(temp.path(),"aaa","{}",{},R"({"bbb":"1.0.0"})");
    EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::runtime_error);
    pack(temp.path(),"bbb");write(temp.path()/"bbb"/"mod.json",R"({"id":"bbb","version":"2.0.0","api":1})");
    EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::runtime_error);
    pack(temp.path(),"bbb","{}",{},R"({"aaa":"1.0.0"})");
    EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::runtime_error);
}

TEST_F(Modding, HooksReadStagedEditsAndCommitInventoryMessagesAndState) {
    pack(temp.path(),"demo","{}",R"(
      local v = voxelcraft
      return {
        on_load = function()
          assert(v.api_version == 1 and v.is_survival())
          local x,y,z = v.player_position()
          assert(x == 0.5 and y == 1 and z == 0.5)
          assert(v.set_block(-1,1,0,"voxelcraft:lamp"))
          assert(v.get_block(-1,1,0) == "voxelcraft:lamp")
          assert(v.give_item("voxelcraft:coal",2))
          assert(v.set_state("visits","1"))
          v.message("ready")
        end,
        on_tick = function(e) assert(e.dt == 0.1); assert(v.set_state("tick","yes")) end,
        on_place = function(e) assert(e.x == -1 and e.y == 1 and e.z == 0 and e.id == "voxelcraft:lamp"); assert(v.set_state("place","yes")) end,
        on_break = function(e) assert(e.id == "voxelcraft:lamp"); assert(v.set_state("broken","yes")) end,
        on_craft = function(e) assert(v.set_state("crafted",e.id)) end
      }
    )");
    vc::ModRuntime mods(temp.path(),blocks);
    EXPECT_EQ(mods.dispatch({vc::ModEventKind::Load},context()),(std::vector<std::string>{"demo: ready"}));
    EXPECT_EQ(world.block({-1,1,0}),blocks.require("voxelcraft:lamp"));EXPECT_TRUE(world.modified({-1,0,0}));
    EXPECT_EQ(survival.inventory.count(items.require("voxelcraft:coal")),2u);
    mods.dispatch({vc::ModEventKind::Tick,{}, {},0.1},context());
    mods.dispatch({vc::ModEventKind::Place,{-1,1,0},"voxelcraft:lamp"},context());
    mods.dispatch({vc::ModEventKind::Break,{-1,1,0},"voxelcraft:lamp"},context());
    mods.dispatch({vc::ModEventKind::Craft,{},"voxelcraft:lamp"},context());
    const auto saved=mods.save();ASSERT_EQ(saved.size(),1u);EXPECT_EQ(saved[0].state.size(),5u);
    EXPECT_EQ(saved[0].state.at("crafted"),"voxelcraft:lamp");EXPECT_TRUE(mods.takeDiagnostics().empty());
}

TEST_F(Modding, FailedHookRollsBackAndDisablesOnlyItsScript) {
    pack(temp.path(),"aaa","{}",R"(return {on_load=function()
      assert(voxelcraft.set_block(-1,1,0,"voxelcraft:lamp"))
      assert(voxelcraft.give_item("voxelcraft:coal",2))
      assert(voxelcraft.set_state("bad","value"))
      voxelcraft.message("must roll back")
      error({})
    end,on_tick=function() voxelcraft.message("must stay disabled") end})");
    pack(temp.path(),"bbb","{}",R"(return {on_load=function() voxelcraft.message("healthy") end})");
    vc::ModRuntime mods(temp.path(),blocks);
    EXPECT_EQ(mods.dispatch({vc::ModEventKind::Load},context()),(std::vector<std::string>{"bbb: healthy"}));
    EXPECT_EQ(world.block({-1,1,0}),vc::voxel::air);EXPECT_TRUE(world.takeDirtyChunks().empty());
    EXPECT_EQ(survival.inventory.count(items.require("voxelcraft:coal")),0u);EXPECT_TRUE(mods.save()[0].state.empty());
    const auto diagnostics=mods.takeDiagnostics();ASSERT_EQ(diagnostics.size(),1u);
    EXPECT_NE(diagnostics[0].find("Disabled script aaa"),std::string::npos);
    EXPECT_TRUE(mods.dispatch({vc::ModEventKind::Tick,{}, {},0.1},context()).empty());EXPECT_TRUE(mods.takeDiagnostics().empty());
}

TEST_F(Modding, InstructionAndMemoryBudgetsStopScripts) {
    pack(temp.path(),"cpu","{}",R"(return {on_load=function() while true do end end})");
    pack(temp.path(),"memory","{}",R"(return {on_load=function() local s=string.rep("x",8*1024*1024); voxelcraft.message(s) end})");
    vc::ModRuntime mods(temp.path(),blocks);
    EXPECT_TRUE(mods.dispatch({vc::ModEventKind::Load},context()).empty());EXPECT_EQ(mods.takeDiagnostics().size(),2u);
    EXPECT_TRUE(mods.dispatch({vc::ModEventKind::Load},context()).empty());EXPECT_TRUE(mods.takeDiagnostics().empty());
}

TEST_F(Modding, ApiRejectsUnsafeEditsAndRetainsRestrictedEnvironment) {
    pack(temp.path(),"demo","{}",R"(return {on_load=function()
      local v=voxelcraft
      assert(io == nil and os == nil and package == nil and debug == nil and require == nil)
      assert(load == nil and loadfile == nil and dofile == nil and pcall == nil and xpcall == nil)
      assert(not v.is_survival() and not v.give_item("voxelcraft:coal",1))
      assert(not v.set_block(0,1,0,"voxelcraft:stone"))
      assert(not v.set_block(12,1,0,"voxelcraft:stone"))
      assert(not v.set_block(0,-513,0,"voxelcraft:stone"))
      assert(v.get_block(32,1,0) == nil)
      assert(v.set_state("test","ok") and v.get_state("test") == "ok")
      assert(v.set_state("test",nil) and v.get_state("test") == nil)
      assert(not v.set_state("large",string.rep("x",257)))
    end})");
    vc::ModRuntime mods(temp.path(),blocks);
    // Oversized string is an API error, so even earlier state writes roll back.
    EXPECT_TRUE(mods.dispatch({vc::ModEventKind::Load},context(true)).empty());
    const auto diagnostics=mods.takeDiagnostics();ASSERT_EQ(diagnostics.size(),1u);
    EXPECT_NE(diagnostics[0].find("API string exceeds limit"),std::string::npos);EXPECT_TRUE(mods.save()[0].state.empty());
}

TEST_F(Modding, PersistedModStateMissingContentAndVersionDiagnostics) {
    pack(temp.path(),"demo",R"({"blocks":[{"id":"demo:glowstone"}]})",
        R"(return {on_load=function() assert(voxelcraft.set_state("visits","1")) end})");
    vc::ModRuntime mods(temp.path(),blocks);mods.dispatch({vc::ModEventKind::Load},context());
    world.setBlock({-1,1,0},blocks.require("demo:glowstone"));
    vc::PlayerSaveState state;state.mods=mods.save();
    {
        vc::voxel::WorldStore store(temp.path()/"world");
        store.save(world,blocks,{},vc::encodePlayer(state));
        auto missingBlocks=vc::builtinBlocks();auto archive=store.load(missingBlocks);ASSERT_TRUE(archive);
        const auto decoded=vc::decodePlayer(archive->gameData);
        vc::ModRuntime missing({},missingBlocks);missing.restore(decoded.mods);
        EXPECT_EQ(missing.save()[0].state.at("visits"),"1");EXPECT_EQ(missing.takeDiagnostics().size(),1u);
        EXPECT_EQ(missingBlocks.definition(archive->world.block({-1,1,0})).identifier,"demo:glowstone");
        EXPECT_EQ(missingBlocks.definition(archive->world.block({-1,1,0})).color,(glm::vec3{1,0,1}));
        state.mods=missing.save();
    }
    auto reloadedBlocks=vc::builtinBlocks();vc::ModRuntime reloaded(temp.path(),reloadedBlocks);reloaded.restore(state.mods);
    EXPECT_EQ(reloaded.save()[0].state.at("visits"),"1");EXPECT_TRUE(reloaded.takeDiagnostics().empty());
    state.mods[0].version="0.9.0";reloaded.restore(state.mods);EXPECT_EQ(reloaded.takeDiagnostics().size(),1u);
    EXPECT_EQ(reloaded.save()[0].version,"1.0.0");
}

TEST(PlayerSave, ModMetadataRoundTripValidationAndLegacyMigration) {
    vc::PlayerSaveState state;state.mods={{"demo","1.0.0",{{"count","2"}},3}};
    const auto bytes=vc::encodePlayer(state);const auto decoded=vc::decodePlayer(bytes);
    ASSERT_EQ(decoded.mods.size(),1u);EXPECT_EQ(decoded.mods[0].state,state.mods[0].state);
    EXPECT_EQ(decoded.mods[0].stateVersion,3u);
    state.mods.push_back(state.mods[0]);EXPECT_THROW(vc::encodePlayer(state),std::runtime_error);
    state.mods.resize(1);state.mods[0].state["oversize"]=std::string(257,'x');EXPECT_THROW(vc::encodePlayer(state),std::runtime_error);
    state.mods.clear();const auto versionFive=vc::test::legacyPlayer(5,state);
    EXPECT_TRUE(vc::decodePlayer(versionFive).mods.empty());
    auto corrupt=bytes;corrupt[versionFive.size()]=33;EXPECT_THROW(vc::decodePlayer(corrupt),std::runtime_error);
}

TEST(PlayerSave, ReadsVersionSixModsAsStateVersionOneAndRejectsInvalidSchemas) {
    auto prefix=vc::test::legacyPlayer(6);prefix.resize(prefix.size()-4);
    vc::binary::Writer fixture;fixture.raw(prefix);fixture.u32(1);
    fixture.text("demo");fixture.text("1.0.0");fixture.u32(1);fixture.text("visits");fixture.text("7");
    const auto decoded=vc::decodePlayer({reinterpret_cast<const char*>(fixture.bytes.data()),fixture.bytes.size()});
    ASSERT_EQ(decoded.mods.size(),1u);EXPECT_EQ(decoded.mods[0].stateVersion,1u);EXPECT_EQ(decoded.mods[0].state.at("visits"),"7");
    auto invalid=decoded;invalid.mods[0].stateVersion=0;EXPECT_THROW(vc::encodePlayer(invalid),std::runtime_error);
    invalid.mods[0].stateVersion=65536;EXPECT_THROW(vc::encodePlayer(invalid),std::runtime_error);
}

TEST_F(Modding, ExplicitMigrationTransformsSavedStateOnceAndPreservesMissingSchemas) {
    pack(temp.path(),"demo","{}",R"(return {
      on_migrate=function(e)
        assert(e.from_version == "1.0.0" and e.to_version == "2.0.0")
        assert(e.from_state_version == 1 and e.to_state_version == 3)
        local old=voxelcraft.get_state("old_counter")
        assert(voxelcraft.set_state("old_counter",nil))
        assert(voxelcraft.set_state("visits",tostring(tonumber(old)+1)))
      end,
      on_load=function() assert(voxelcraft.get_state("visits") == "8") end
    })");
    write(temp.path()/"demo"/"mod.json",R"({"id":"demo","version":"2.0.0","api":1,"state_version":3})");
    vc::ModRuntime mods(temp.path(),blocks);mods.restore({{"demo","1.0.0",{{"old_counter","7"}},1}});
    auto saved=mods.save();ASSERT_EQ(saved.size(),1u);EXPECT_EQ(saved[0].stateVersion,3u);
    EXPECT_EQ(saved[0].version,"2.0.0");EXPECT_EQ(saved[0].state.at("visits"),"8");EXPECT_FALSE(saved[0].state.contains("old_counter"));
    EXPECT_EQ(mods.takeDiagnostics().size(),2u);mods.dispatch({vc::ModEventKind::Load},context());EXPECT_TRUE(mods.takeDiagnostics().empty());
    mods.restore(saved);EXPECT_EQ(mods.save()[0].state.at("visits"),"8");EXPECT_TRUE(mods.takeDiagnostics().empty());
    auto missingBlocks=vc::builtinBlocks();vc::ModRuntime missing({},missingBlocks);missing.restore(saved);
    vc::PlayerSaveState retainedState;retainedState.mods=missing.save();
    const auto retained=vc::decodePlayer(vc::encodePlayer(retainedState));
    ASSERT_EQ(retained.mods.size(),1u);EXPECT_EQ(retained.mods[0].stateVersion,3u);
}

TEST_F(Modding, MissingMigrationAndDowngradeFailWithoutChangingRuntimeState) {
    pack(temp.path(),"demo");write(temp.path()/"demo"/"mod.json",R"({"id":"demo","version":"2.0.0","api":1,"state_version":2})");
    vc::ModRuntime mods(temp.path(),blocks);
    EXPECT_THROW(mods.restore({{"demo","1.0.0",{{"old","value"}},1}}),std::runtime_error);
    EXPECT_EQ(mods.save()[0].stateVersion,2u);EXPECT_TRUE(mods.save()[0].state.empty());EXPECT_TRUE(mods.takeDiagnostics().empty());
    EXPECT_THROW(mods.restore({{"demo","3.0.0",{{"future","value"}},3}}),std::runtime_error);
    EXPECT_TRUE(mods.save()[0].state.empty());EXPECT_TRUE(mods.takeDiagnostics().empty());
}

TEST_F(Modding, FailingMigrationRollsBackAllPacksAndDisallowsWorldEffects) {
    pack(temp.path(),"aaa","{}",R"(local mutated=false;return {
      on_migrate=function() mutated=true; assert(voxelcraft.set_state("new","value")) end,
      on_load=function() assert(not mutated); voxelcraft.message("original VM") end
    })");
    pack(temp.path(),"bbb","{}",R"(return {on_migrate=function()
      assert(voxelcraft.set_state("partial","value"))
      voxelcraft.set_block(0,1,0,"voxelcraft:stone")
    end})");
    for(const auto* id:{"aaa","bbb"}) write(temp.path()/id/"mod.json","{\"id\":\""+std::string(id)+"\",\"version\":\"2.0.0\",\"api\":1,\"state_version\":2}");
    vc::ModRuntime mods(temp.path(),blocks);
    EXPECT_THROW(mods.restore({{"aaa","1.0.0",{},1},{"bbb","1.0.0",{},1}}),std::runtime_error);
    for(const auto& mod:mods.save()) EXPECT_TRUE(mod.state.empty());
    EXPECT_TRUE(mods.takeDiagnostics().empty());EXPECT_TRUE(world.takeDirtyChunks().empty());
    EXPECT_EQ(mods.dispatch({vc::ModEventKind::Load},context()),(std::vector<std::string>{"aaa: original VM"}));
    EXPECT_TRUE(mods.takeDiagnostics().empty());
}

TEST_F(Modding, MigrationInstructionBudgetAbortsStartupTransaction) {
    pack(temp.path(),"demo","{}",R"(return {on_migrate=function() while true do end end})");
    write(temp.path()/"demo"/"mod.json",R"({"id":"demo","version":"2.0.0","api":1,"state_version":2})");
    vc::ModRuntime mods(temp.path(),blocks);
    EXPECT_THROW(mods.restore({{"demo","1.0.0",{{"keep","yes"}},1}}),std::runtime_error);
    EXPECT_TRUE(mods.save()[0].state.empty());EXPECT_TRUE(mods.takeDiagnostics().empty());
}

TEST_F(Modding, MiningDefinitionsApplyTimeTierDropsAndToolWear) {
    pack(temp.path(),"demo",R"({
      "blocks":[{"id":"demo:ore","mining":{"seconds":0.5,"tool":"pickaxe","tier":2,"drop":"demo:gem","drop_count":3}}],
      "items":[{"id":"demo:gem","stack_size":16}]
    })");
    vc::ModRuntime mods(temp.path(),blocks);items=vc::builtinItems(blocks);auto recipes=vc::builtinRecipes(items);
    mods.installItems(items,blocks,recipes);vc::MiningRules rules(blocks,items);mods.installMining(rules,blocks,items);
    const auto ore=blocks.require("demo:ore");const auto gem=items.require("demo:gem");const vc::voxel::RayHit hit{{2,1,0},{0,1,0},1};
    EXPECT_EQ(rules.rule(ore).seconds,0.5);EXPECT_EQ(rules.rule(ore).dropCount,3);
    world.setBlock(hit.block,ore);
    for(int i=0;i<4;++i) EXPECT_FALSE(survival.mine(true,hit,0.1,world,items,rules).changed);
    EXPECT_GT(survival.miningProgress(),0); // Bare hands are slower and cannot harvest.
    for(int i=0;i<20&&world.block(hit.block)!=vc::voxel::air;++i) survival.mine(true,hit,0.1,world,items,rules);
    EXPECT_EQ(world.block(hit.block),vc::voxel::air);EXPECT_EQ(survival.inventory.count(gem),0u);
    ASSERT_TRUE(survival.inventory.add(items.require("voxelcraft:stone_pickaxe"),1,items));world.setBlock(hit.block,ore);
    EXPECT_TRUE(survival.mine(true,hit,0.1,world,items,rules).changed);
    EXPECT_EQ(survival.inventory.count(gem),3u);EXPECT_EQ(survival.inventory.selected().durability,130);
    survival.inventory=vc::SurvivalInventory{};ASSERT_TRUE(survival.inventory.add(items.require("voxelcraft:dirt"),36*64,items));
    vc::MiningRule freeDrop=rules.rule(ore);freeDrop.tool=vc::ToolKind::None;freeDrop.requiredTier=0;freeDrop.seconds=0.05;
    rules.set(ore,freeDrop,items);world.setBlock(hit.block,ore);
    const auto full=survival.mine(true,hit,0.1,world,items,rules);EXPECT_TRUE(full.inventoryFull);EXPECT_FALSE(full.changed);EXPECT_EQ(world.block(hit.block),ore);
}

TEST_F(Modding, InvalidMiningDeclarationsRejectPackTransactionally) {
    const auto before=blocks.size();
    for(const auto* mining:{R"({"seconds":0})",R"({"seconds":4000})",R"({"tool":"axe"})",
        R"({"tier":2})",R"({"drop":"demo:missing"})",R"({"drop":null,"drop_count":2})",R"({"drop_count":65})"}) {
        pack(temp.path(),"demo","{\"blocks\":[{\"id\":\"demo:ore\",\"mining\":"+std::string(mining)+"}]}");
        EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::exception);EXPECT_EQ(blocks.size(),before);
    }
    pack(temp.path(),"demo",R"({"blocks":[{"id":"demo:ore","mining":{"drop":null}}]})");
    vc::ModRuntime mods(temp.path(),blocks);items=vc::builtinItems(blocks);vc::MiningRules rules(blocks,items);mods.installMining(rules,blocks,items);
    EXPECT_FALSE(rules.rule(blocks.require("demo:ore")).drop);
}

TEST_F(Modding, ExternalTextureNamesResolveBeforeBlocksIncludingDependencies) {
    pack(temp.path(),"aaa",R"({"blocks":[{"id":"aaa:stone","texture":"zzz:custom"}]})",{},R"({"zzz":"1.0.0"})");
    pack(temp.path(),"zzz",R"({"textures":[{"id":"zzz:custom","file":"tile.png"}]})");
    std::filesystem::copy_file(std::filesystem::path(VC_TEST_DATA_DIR)/"tile.png",temp.path()/"zzz"/"tile.png");
    vc::ModRuntime mods(temp.path(),blocks);const auto id=mods.atlas().require("zzz:custom");
    EXPECT_EQ(mods.atlas().size(),18u);EXPECT_EQ(blocks.definition(blocks.require("aaa:stone")).faceTextures[0],id);
    EXPECT_GE(id,vc::builtinAtlasLayout().tileCount());
    vc::voxel::Chunk chunk({0,0,0});chunk.set({1,1,1},blocks.require("aaa:stone"));
    const auto mesh=vc::voxel::meshChunk(chunk,world,blocks,mods.atlas().layout());
    ASSERT_FALSE(mesh.vertices.empty());const auto begin=mods.atlas().layout().uv(id,{0,0});
    EXPECT_GE(mesh.vertices[0].uv.x,begin.x);EXPECT_GE(mesh.vertices[0].uv.y,begin.y);
}

TEST_F(Modding, BadTextureNamespacePathAndSymlinkCannotPartiallyRegisterContent) {
    const auto before=blocks.size();
    for(const auto* path:{"../outside.png","/outside.png","textures/../../outside.png","C:/outside.png","bad\\file.png"}) {
        std::string escaped;for(const char c:std::string(path)) { if(c=='\\') escaped+='\\';escaped+=c; }
        pack(temp.path(),"demo","{\"textures\":[{\"id\":\"demo:tile\",\"file\":\""+escaped+"\"}],\"blocks\":[{\"id\":\"demo:block\"}]}");
        EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::exception);EXPECT_EQ(blocks.size(),before);
    }
    std::filesystem::copy_file(std::filesystem::path(VC_TEST_DATA_DIR)/"tile.png",temp.path()/"outside.png");
    pack(temp.path(),"demo",R"({"textures":[{"id":"voxelcraft:bad","file":"tile.png"}]})");
    EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::runtime_error);EXPECT_EQ(blocks.size(),before);
    std::error_code error;std::filesystem::create_symlink(temp.path()/"outside.png",temp.path()/"demo"/"escape.png",error);
    if(!error) {
        pack(temp.path(),"demo",R"({"textures":[{"id":"demo:tile","file":"escape.png"}]})");
        EXPECT_THROW(vc::ModRuntime(temp.path(),blocks),std::runtime_error);EXPECT_EQ(blocks.size(),before);
    }
}
