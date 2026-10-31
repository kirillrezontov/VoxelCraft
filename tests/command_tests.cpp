#include "game/gameplay/inventory.hpp"
#include "game/commands/commands.hpp"
#include "game/content/blocks.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace {
struct CommandWorld:testing::Test {
    vc::voxel::BlockRegistry blocks=vc::builtinBlocks();
    vc::ItemRegistry items=vc::builtinItems(blocks);
    vc::voxel::World world{{0,31}};
    vc::PlayerController player;
    vc::SurvivalState survival;
    vc::LivingWorld living{42};vc::DayCycle day;
    bool mode{true};std::array<std::string,9> hotbar{};std::uint32_t slot{};
    std::optional<glm::dvec3> spawn;std::uint64_t poseRevision{};
    vc::commands::Context context;
    vc::commands::Registry registry=vc::commands::builtinCommands();
    CommandWorld() {
        for(int x=-1;x<=1;++x) for(int z=-1;z<=1;++z) {
            vc::voxel::Chunk chunk({x,0,z});
            for(int a=0;a<32;++a) for(int b=0;b<32;++b) chunk.set({a,0,b},blocks.require("voxelcraft:stone"));
            world.insert(std::move(chunk));
        }
        player.camera.position={0.5,2.62,0.5};
        context.permission=vc::commands::Permission::Administrator;context.issuer="Alice";context.origin={0.5,1,0.5};
        context.world=&world;context.blocks=&blocks;context.items=&items;context.living=&living;context.day=&day;context.metadata={42,{0,31},1};
        context.players.push_back({"Alice",&player,&survival,&mode,&hotbar,&slot,&spawn,&poseRevision,vc::commands::Permission::Administrator});
    }
    auto execute(std::string_view command) { return registry.execute(command,context); }
};
}
TEST(CommandParser, QuotesEscapesWhitespaceAndLimits) {
    const auto parsed=vc::commands::parse("  /give\t@s \"demo:stone\"  64  ");EXPECT_EQ(parsed.name,"give");ASSERT_EQ(parsed.arguments.size(),3u);EXPECT_EQ(parsed.arguments[1],"demo:stone");
    EXPECT_EQ(vc::commands::parse(R"(say 'Привет мир' \"quoted\")").arguments[0],"Привет мир");
    for(const auto text:{"","/","say \"unterminated","say trailing\\"}) EXPECT_THROW(vc::commands::parse(text),vc::commands::Error);
    EXPECT_THROW(vc::commands::parse(std::string(1025,'a')),vc::commands::Error);
    EXPECT_THROW(vc::commands::parse(std::string("say \xff",5)),vc::commands::Error);
    EXPECT_DOUBLE_EQ(vc::commands::coordinate("~",42),42);EXPECT_DOUBLE_EQ(vc::commands::coordinate("~-10",42),32);
    for(const auto text:{"nan","inf","1e999","~nan","9223372036854775808","^1"}) EXPECT_THROW(vc::commands::coordinate(text,0),vc::commands::Error);
    EXPECT_THROW(vc::commands::unsignedNumber("18446744073709551616",UINT64_MAX),vc::commands::Error);
}
TEST_F(CommandWorld, RealGiveModeAndInventoryTransactions) {
    EXPECT_TRUE(execute("/give @s voxelcraft:wooden_pickaxe 1").success());
    EXPECT_EQ(survival.inventory.selected().item,items.require("voxelcraft:wooden_pickaxe"));EXPECT_EQ(survival.inventory.selected().durability,59);
    const auto saved=survival.inventory.save(items);
    EXPECT_TRUE(execute("/gamemode creative").success());EXPECT_FALSE(mode);EXPECT_TRUE(player.flying);EXPECT_EQ(survival.inventory.save(items),saved);
    EXPECT_TRUE(execute("/give @s voxelcraft:stone_pickaxe").success());EXPECT_EQ(hotbar[slot],"voxelcraft:stone_pickaxe");
    EXPECT_TRUE(execute("/gamemode survival").success());EXPECT_TRUE(mode);EXPECT_FALSE(player.flying);EXPECT_EQ(survival.inventory.count(items.require("voxelcraft:wooden_pickaxe")),1u);
    EXPECT_EQ(execute("/give @a voxelcraft:stone").code,vc::commands::Code::InvalidTarget);
    EXPECT_EQ(execute("/give Alice unknown:item").code,vc::commands::Code::InvalidArgument);
    EXPECT_EQ(execute("/give Alice voxelcraft:stone 0").code,vc::commands::Code::InvalidArgument);
    EXPECT_EQ(execute("/give Disconnected voxelcraft:stone").code,vc::commands::Code::InvalidTarget);
}
TEST_F(CommandWorld, TeleportRelativeCoordinatesSafetyAndSpawnPoint) {
    EXPECT_TRUE(execute("/teleport ~2 ~1 ~-3").success());EXPECT_EQ(player.camera.position,glm::dvec3(2.5,3.62,-2.5));EXPECT_GT(poseRevision,0u);
    EXPECT_TRUE(execute("/spawnpoint").success());ASSERT_TRUE(spawn);EXPECT_EQ(*spawn,glm::dvec3(2.5,2,-2.5));
    const auto before=player.camera.position;
    for(const auto text:{"/tp 0 0 0","/tp 0 31 0","/tp 10000 2 0","/tp nan 2 0","/tp 1e999 2 0"}) EXPECT_FALSE(execute(text).success())<<text;
    EXPECT_EQ(player.camera.position,before);
    context.sample=[&](vc::voxel::Position p){return p.y==0?blocks.require("voxelcraft:stone"):vc::voxel::air;};
    EXPECT_TRUE(execute("/tp -10000 5 -10000").success());EXPECT_EQ(player.camera.position,glm::dvec3(-10000,6.62,-10000));
}
TEST_F(CommandWorld, SummonRealEntitiesTimeQueriesAndLimits) {
    EXPECT_TRUE(execute("/summon voxelcraft:hostile ~2 ~ ~2").success());ASSERT_EQ(living.creatureCount(),1u);
    EXPECT_EQ(living.save(items)[0].kind,"voxelcraft:stalker");
    EXPECT_FALSE(execute("/summon unknown:mob").success());EXPECT_FALSE(execute("/summon voxelcraft:grazer 1").success());
    EXPECT_TRUE(execute("/time set night").success());EXPECT_DOUBLE_EQ(day.seconds(),900);
    EXPECT_TRUE(execute("/time query").success());EXPECT_TRUE(execute("/time set 24000").success());EXPECT_DOUBLE_EQ(day.seconds(),0);
    EXPECT_TRUE(execute("/seed").success());EXPECT_TRUE(execute("/position").success());EXPECT_TRUE(execute("/list").success());
    for(unsigned i=1;i<32;++i) ASSERT_TRUE(living.spawn(vc::CreatureKind::Grazer,{3.5,1,3.5}));
    EXPECT_FALSE(execute("/summon voxelcraft:grazer").success());EXPECT_EQ(living.creatureCount(),32u);
}
TEST_F(CommandWorld, PermissionFilteredHelpAndLiveRegistryCompletion) {
    context.permission=vc::commands::Permission::Player;
    EXPECT_EQ(execute("/gamemode creative").code,vc::commands::Code::PermissionDenied);EXPECT_TRUE(mode);
    EXPECT_EQ(execute("/op Alice").code,vc::commands::Code::PermissionDenied);
    const auto help=execute("/help");EXPECT_TRUE(help.success());for(const auto& line:help.lines) EXPECT_EQ(line.find("gamemode"),std::string::npos);
    vc::commands::CompletionContext completion{vc::commands::Permission::Administrator,&items,{"Alice","Bob"}};
    EXPECT_EQ(registry.complete("/gamemode cr",completion).at(0).replacement,"/gamemode creative");
    items.add({"demo:stone_tool",{},1,42,vc::ToolKind::Pickaxe});
    const auto choices=registry.complete("/give @s demo:sto",completion);ASSERT_EQ(choices.size(),1u);EXPECT_EQ(choices[0].label,"demo:stone_tool");
    completion.permission=vc::commands::Permission::Player;EXPECT_TRUE(registry.complete("/game",completion).empty());
    EXPECT_EQ(execute("/not-a-command").code,vc::commands::Code::UnknownCommand);
    EXPECT_EQ(execute("/help gamemode").code,vc::commands::Code::UnknownCommand);
}
TEST_F(CommandWorld, FullInventoryAndReadOnlyCraftingAvailability) {
    ASSERT_TRUE(survival.inventory.add(items.require("voxelcraft:stone"),36*64,items));const auto before=survival.inventory.slots();
    EXPECT_FALSE(execute("/give @s voxelcraft:stone_pickaxe").success());EXPECT_EQ(survival.inventory.slots(),before);
    const auto revision=survival.inventory.revision();const auto recipes=vc::builtinRecipes(items);
    EXPECT_EQ(survival.inventory.canCraft(recipes[0],items),vc::CraftResult::MissingIngredients);EXPECT_EQ(survival.inventory.revision(),revision);EXPECT_EQ(survival.inventory.slots(),before);
    context.permission=vc::commands::Permission::ServerConsole;context.issuer.clear();EXPECT_EQ(execute("/give @s voxelcraft:stone").code,vc::commands::Code::InvalidTarget);
}

#include "game/commands/permissions.hpp"
#include "engine/platform/console_input.hpp"
#include "tests/temp_directory.hpp"
#include <fstream>
TEST(CommandPermissions, AtomicPersistentProfileRolesAndCorruptionDetection) {
    vc::test::TempDirectory temp;const auto path=temp.path()/"operators.vcp";
    {
        vc::commands::PermissionStore roles(path);EXPECT_EQ(roles.get("Alice"),vc::commands::Permission::Player);
        EXPECT_TRUE(roles.set("ALICE",vc::commands::Permission::Administrator));EXPECT_TRUE(roles.set("Bob",vc::commands::Permission::Moderator));
    }
    {
        vc::commands::PermissionStore roles(path);EXPECT_EQ(roles.get("alice"),vc::commands::Permission::Administrator);
        EXPECT_EQ(roles.get("BOB"),vc::commands::Permission::Moderator);roles.set("Bob",vc::commands::Permission::Player);
        EXPECT_THROW(roles.set("Mallory",vc::commands::Permission::ServerConsole),std::invalid_argument);
    }
    vc::commands::PermissionStore roles(path);EXPECT_EQ(roles.get("bob"),vc::commands::Permission::Player);
    std::ofstream(path,std::ios::app).put('x');EXPECT_THROW(vc::commands::PermissionStore bad(path),std::runtime_error);
}
TEST(ConsoleInput, FragmentationUnicodeEofAndLengthBounds) {
    vc::ConsoleLines lines(16);lines.feed("/help\r\nlist\nПривет");
    auto ready=lines.take();ASSERT_EQ(ready.size(),2u);EXPECT_EQ(ready[0].text,"/help");EXPECT_EQ(ready[1].text,"list");
    lines.finish();ready=lines.take();ASSERT_EQ(ready.size(),1u);EXPECT_EQ(ready[0].text,"Привет");
    lines.feed(std::string(1000,'x')+"\nlist\n");ready=lines.take();ASSERT_EQ(ready.size(),2u);EXPECT_TRUE(ready[0].tooLong);EXPECT_FALSE(ready[1].tooLong);
}
TEST(CreativeItems, CompleteToolsOrdinaryItemsModMetadataAndSavedAssignments) {
    const auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);
    vc::ItemDefinition mod{"demo:custom",{},16};mod.presentation={"Особый материал","Тестовое описание","materials"};items.add(mod);
    vc::CreativeInventory creative(items);
    for(const auto* name:{"voxelcraft:wooden_pickaxe","voxelcraft:stone_pickaxe","voxelcraft:coal","voxelcraft:stick","demo:custom"})
        EXPECT_NE(std::ranges::find(creative.catalog(),items.require(name)),creative.catalog().end());
    creative.assignItem(8,items.require("voxelcraft:stone_pickaxe"));creative.select(8);EXPECT_FALSE(creative.selectedBlock());
    const auto names=creative.names();vc::CreativeInventory restored(items);restored.restore(names,8);EXPECT_EQ(restored.selectedItem(),items.require("voxelcraft:stone_pickaxe"));
    EXPECT_EQ(creative.filter("КИРКА").size(),0u);EXPECT_EQ(creative.filter("ОСОБЫЙ").size(),1u);EXPECT_EQ(creative.filter("voxelcraft:stone_pickaxe").size(),1u);
    EXPECT_EQ(creative.filter("",vc::ItemCategory::Tools).size(),2u);
}
TEST(CreativeItems, MissingToolSharedByCreativeAndCountedInventoriesKeepsDurability) {
    const auto blocks=vc::builtinBlocks();auto items=vc::builtinItems(blocks);std::array<std::string,9> names{};names[8]="absent:pickaxe";
    vc::retainHotbarItems(items,names);vc::CreativeInventory creative(items);creative.restore(names,8);
    const auto id=items.require("absent:pickaxe");std::array<vc::SavedItemStack,36> saved{};saved[0]={"absent:pickaxe",1,73};saved[9]={"absent:pickaxe",1,73};
    vc::SurvivalInventory counted;counted.restore(saved,0,items);EXPECT_EQ(creative.selectedItem(),id);EXPECT_EQ(counted.save(items),saved);
    EXPECT_EQ(items.definition(id).stackSize,1);EXPECT_GE(items.definition(id).durability,73);
    EXPECT_FALSE(counted.transfer(0,9,1,items));EXPECT_EQ(counted.count(id),2u);EXPECT_EQ(counted.save(items),saved);
}
