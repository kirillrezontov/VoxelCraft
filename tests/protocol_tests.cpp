#include "game/network/protocol.hpp"
#include "game/content/blocks.hpp"
#include "game/player/save_state.hpp"
#include "tests/player_fixture.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace {
vc::netgame::Welcome welcome() {
    vc::netgame::Welcome message;message.id=7;message.tick=90;message.profile.name="Alice";
    message.profile.camera.position={-31.5,12.62,-63.5};message.profile.flying=true;message.profile.selectedSlot=2;
    const auto blocks=vc::builtinBlocks();
    for(std::size_t i=0;i<blocks.size();++i) message.blocks.push_back(blocks.definition(static_cast<vc::voxel::BlockId>(i)).identifier);
    return message;
}
}
TEST(PlayerProfiles, PersistentPoseHotbarAndCaseInsensitiveIdentityValidation) {
    vc::PlayerProfile profile;profile.name="Alice_1";profile.camera.position={-31.5,2.62,-64.5};profile.flying=true;profile.hotbar[8]="demo:custom";profile.selectedSlot=8;
    vc::PlayerSaveState state;state.profiles.push_back(profile);
    const auto decoded=vc::decodePlayer(vc::encodePlayer(state));ASSERT_EQ(decoded.profiles.size(),1u);
    EXPECT_EQ(decoded.profiles[0].name,profile.name);EXPECT_EQ(decoded.profiles[0].camera.position,profile.camera.position);
    EXPECT_EQ(decoded.profiles[0].hotbar,profile.hotbar);EXPECT_EQ(decoded.profiles[0].selectedSlot,8u);EXPECT_TRUE(decoded.profiles[0].flying);
    auto duplicate=profile;duplicate.name="aLICE_1";state.profiles.push_back(duplicate);EXPECT_THROW(vc::encodePlayer(state),std::runtime_error);
    for(const auto* name:{"","with space","slash/name","name\n","1234567890123456789012345"}) EXPECT_FALSE(vc::validPlayerName(name));
    for(unsigned version=3;version<=8;++version) EXPECT_TRUE(vc::decodePlayer(vc::test::legacyPlayer(version)).profiles.empty());
    state.profiles.resize(1);state.profiles[0].camera.position.x=std::numeric_limits<double>::infinity();EXPECT_THROW(vc::encodePlayer(state),std::runtime_error);
}
TEST(GameProtocol, HandshakeProfilePaletteAndVersionFailures) {
    const vc::netgame::Hello hello{123456,"Alice"};const auto encoded=vc::netgame::encode(hello);
    EXPECT_EQ(vc::netgame::messageType(encoded),vc::netgame::Message::Hello);
    const auto decoded=vc::netgame::decodeHello(encoded);EXPECT_EQ(decoded.name,hello.name);EXPECT_EQ(decoded.fingerprint,hello.fingerprint);
    auto wrong=encoded;wrong[9]=99;EXPECT_THROW(vc::netgame::decodeHello(wrong),std::runtime_error);
    auto extra=encoded;extra.push_back(0);EXPECT_THROW(vc::netgame::decodeHello(extra),std::runtime_error);
    const auto source=welcome();const auto restored=vc::netgame::decodeWelcome(vc::netgame::encode(source));
    EXPECT_EQ(restored.id,7u);EXPECT_EQ(restored.blocks,source.blocks);EXPECT_EQ(restored.profile.camera.position,source.profile.camera.position);
    EXPECT_EQ(restored.profile.hotbar,source.profile.hotbar);EXPECT_EQ(restored.radius,2u);
    auto invalid=source;invalid.blocks.push_back("voxelcraft:air");EXPECT_THROW(vc::netgame::encode(invalid),std::runtime_error);
    invalid=source;invalid.metadata.bounds.maxY=2000000;EXPECT_THROW(vc::netgame::encode(invalid),std::runtime_error);
    invalid=source;invalid.profile.name="bad name";EXPECT_THROW(vc::netgame::encode(invalid),std::runtime_error);
}
TEST(GameProtocol, MovementIntentHotbarAndControlsHaveStrictBounds) {
    vc::netgame::Input input;input.sequence=4;input.forward=1;input.right=-1;input.yaw=0.5;input.pitch=-0.4;
    input.flags=vc::netgame::Active|vc::netgame::Jump|vc::netgame::Destroy;input.slot=8;
    const auto decoded=vc::netgame::decodeInput(vc::netgame::encode(input));EXPECT_EQ(decoded.sequence,4u);EXPECT_EQ(decoded.forward,1);EXPECT_EQ(decoded.slot,8u);
    auto invalid=input;invalid.forward=1.001;EXPECT_THROW(vc::netgame::encode(invalid),std::runtime_error);
    invalid=input;invalid.pitch=std::numeric_limits<double>::quiet_NaN();EXPECT_THROW(vc::netgame::encode(invalid),std::runtime_error);
    invalid=input;invalid.flags=128;EXPECT_THROW(vc::netgame::encode(invalid),std::runtime_error);
    invalid=input;invalid.sequence=0;EXPECT_THROW(vc::netgame::encode(invalid),std::runtime_error);
    vc::netgame::Hotbar hotbar;hotbar.names[8]="voxelcraft:lamp";hotbar.slot=8;
    EXPECT_EQ(vc::netgame::decodeHotbar(vc::netgame::encode(hotbar)).names,hotbar.names);
    hotbar.names[0]="voxelcraft:air";EXPECT_THROW(vc::netgame::encode(hotbar),std::runtime_error);
    const auto save=vc::netgame::encodeControl(vc::netgame::Message::Save);EXPECT_NO_THROW(vc::netgame::decodeControl(save,vc::netgame::Message::Save));
    EXPECT_THROW(vc::netgame::decodeControl(save,vc::netgame::Message::Goodbye),std::runtime_error);
    EXPECT_EQ(vc::netgame::decodeText(vc::netgame::encodeText(vc::netgame::Message::Reject,"content mismatch"),vc::netgame::Message::Reject),"content mismatch");
}
TEST(GameProtocol, ActorAndPlayerSnapshotsValidateIdentitiesAndMotion) {
    vc::netgame::State state;state.tick=44;state.worldTime=899;
    const auto source=welcome();state.players.push_back({1,"Alice",source.profile.camera,false});
    state.players.push_back({2,"Bob",source.profile.camera,true});
    vc::SavedActor actor;actor.id=91;actor.kind="voxelcraft:grazer";actor.position={-1,2,-32};actor.health=8;state.actors.push_back(actor);
    const auto restored=vc::netgame::decodeState(vc::netgame::encode(state));EXPECT_EQ(restored.tick,44u);EXPECT_EQ(restored.worldTime,899);
    ASSERT_EQ(restored.players.size(),2u);EXPECT_EQ(restored.players[1].name,"Bob");EXPECT_TRUE(restored.players[1].flying);
    ASSERT_EQ(restored.actors.size(),1u);EXPECT_EQ(restored.actors[0].position,actor.position);EXPECT_EQ(restored.actors[0].id,91u);
    auto invalid=state;invalid.players[1].name="ALICE";EXPECT_THROW(vc::netgame::encode(invalid),std::runtime_error);
    invalid=state;invalid.actors[0].position.y=std::numeric_limits<double>::infinity();EXPECT_THROW(vc::netgame::encode(invalid),std::runtime_error);
    invalid=state;invalid.worldTime=1200;EXPECT_THROW(vc::netgame::encode(invalid),std::runtime_error);
}
TEST(GameProtocol, ChunkCompressionNegativeCoordinatesAndRuntimeIdRemapping) {
    vc::voxel::Chunk chunk({-1,-2,3});
    for(int y=0;y<32;++y) for(int z=0;z<32;++z) for(int x=0;x<32;++x) chunk.set({x,y,z},static_cast<vc::voxel::BlockId>((x+y+z)%3));
    const std::array<vc::voxel::BlockId,3> mapping{0,8,4};
    const auto restored=vc::netgame::decodeChunk(vc::netgame::encodeChunk(chunk,77,3),mapping);
    EXPECT_EQ(restored.chunk.position(),chunk.position());EXPECT_EQ(restored.revision,77u);
    for(int y=0;y<32;++y) for(int z=0;z<32;++z) for(int x=0;x<32;++x) ASSERT_EQ(restored.chunk.get({x,y,z}),mapping[static_cast<std::size_t>((x+y+z)%3)]);
    const vc::voxel::Chunk uniform({0,0,0},1);EXPECT_LE(vc::netgame::encodeChunk(uniform,1,3).size(),64u);
    const vc::voxel::Position p{-1,-2,3};EXPECT_EQ(vc::netgame::decodeUnload(vc::netgame::encodeUnload(p)),p);
}
TEST(GameProtocol, MalformedChunkRunsUnknownIdsAndTrailingBytesAreRejected) {
    const std::array<vc::voxel::BlockId,2> mapping{0,1};const vc::voxel::Chunk chunk({0,0,0},1);
    auto encoded=vc::netgame::encodeChunk(chunk,1,2);encoded.push_back(0);EXPECT_THROW(vc::netgame::decodeChunk(encoded,mapping),std::runtime_error);
    encoded=vc::netgame::encodeChunk(chunk,1,2);encoded.pop_back();EXPECT_THROW(vc::netgame::decodeChunk(encoded,mapping),std::runtime_error);
    vc::binary::Writer malformed;malformed.u32(static_cast<std::uint32_t>(vc::netgame::Message::Chunk));
    for(int i=0;i<3;++i) malformed.i64(0);
    malformed.u64(1);malformed.u16(1);malformed.u16(1);malformed.u32(1);malformed.u16(0);malformed.u16(32767);
    EXPECT_THROW(vc::netgame::decodeChunk(malformed.bytes,mapping),std::runtime_error);
    EXPECT_THROW(vc::netgame::encodeChunk(chunk,0,2),std::invalid_argument);
    EXPECT_THROW(vc::netgame::encodeChunk(chunk,1,1),std::runtime_error);
    EXPECT_THROW(vc::netgame::messageType({}),std::runtime_error);
}
TEST(GameProtocol, FingerprintIsStableAndDetectsRecipeAndHarvestChanges) {
    auto blocks=vc::builtinBlocks();vc::ModRuntime mods({},blocks);const auto items=vc::builtinItems(blocks);
    auto recipes=vc::builtinRecipes(items);vc::MiningRules mining(blocks,items);
    const auto original=vc::netgame::contentFingerprint(blocks,items,recipes,mining,mods);
    EXPECT_EQ(original,vc::netgame::contentFingerprint(blocks,items,recipes,mining,mods));
    recipes[0].count=3;EXPECT_NE(original,vc::netgame::contentFingerprint(blocks,items,recipes,mining,mods));
    recipes=vc::builtinRecipes(items);auto rule=mining.rule(blocks.require("voxelcraft:stone"));rule.seconds=2;mining.set(blocks.require("voxelcraft:stone"),rule,items);
    EXPECT_NE(original,vc::netgame::contentFingerprint(blocks,items,recipes,mining,mods));
}
TEST(PlayerProfiles, SurvivalStacksHealthAndLegacyEightProfilesMigrate) {
    vc::PlayerSaveState state;vc::PlayerProfile profile;profile.name="Alice";profile.health=7;
    profile.items[3]={"voxelcraft:stone_pickaxe",1,80};profile.items[9]={"missing:retained",64,0};state.profiles.push_back(profile);
    const auto decoded=vc::decodePlayer(vc::encodePlayer(state));ASSERT_EQ(decoded.profiles.size(),1u);
    EXPECT_EQ(decoded.profiles[0].health,7u);EXPECT_EQ(decoded.profiles[0].items,profile.items);
    const auto old=vc::decodePlayer(vc::test::legacyPlayer(8,state));ASSERT_EQ(old.profiles.size(),1u);
    EXPECT_EQ(old.profiles[0].health,20u);EXPECT_EQ(old.profiles[0].name,"Alice");
    EXPECT_EQ(old.profiles[0].hotbar,profile.hotbar);for(const auto& item:old.profiles[0].items) EXPECT_TRUE(item.identifier.empty());
    state.profiles[0].health=21;EXPECT_THROW(vc::encodePlayer(state),std::runtime_error);
    state.profiles[0].health=20;state.profiles[0].items[0]={"",1,0};EXPECT_THROW(vc::encodePlayer(state),std::runtime_error);
}
TEST(PlayerProfiles, MaximumNamedInventoriesFitWithinGamePayloadLimit) {
    vc::PlayerSaveState state;
    for(unsigned i=0;i<vc::maxSavedProfiles;++i) {
        vc::PlayerProfile profile;profile.name="Player_"+std::to_string(i);
        for(auto& name:profile.hotbar) name="mod:"+std::string(252,'a');
        for(auto& item:profile.items) item={"mod:"+std::string(252,'b'),64,0};
        state.profiles.push_back(profile);
    }
    const auto encoded=vc::encodePlayer(state);EXPECT_LT(encoded.size(),vc::voxel::maxGameDataBytes);
    EXPECT_EQ(vc::decodePlayer(encoded).profiles.size(),vc::maxSavedProfiles);
}
TEST(GameProtocol, SurvivalSnapshotsAndInventoryRequestsAreStrictValueMessages) {
    vc::netgame::State state;state.survival.emplace();state.survival->health=12;state.survival->selectedSlot=3;
    state.survival->items[3]={"voxelcraft:stone_pickaxe",1,100};state.survival->mining=true;state.survival->miningProgress=0.6;
    const auto decoded=vc::netgame::decodeState(vc::netgame::encode(state));ASSERT_TRUE(decoded.survival);
    EXPECT_EQ(decoded.survival->items,state.survival->items);EXPECT_EQ(decoded.survival->miningProgress,0.6);EXPECT_EQ(decoded.survival->health,12u);
    state.survival->miningProgress=1.01;EXPECT_THROW(vc::netgame::encode(state),std::runtime_error);
    state.survival->miningProgress=0.5;state.survival->mining=false;EXPECT_THROW(vc::netgame::encode(state),std::runtime_error);
    vc::InventoryAction action{vc::InventoryActionKind::Swap,9,3};
    const auto restored=vc::netgame::decodeInventory(vc::netgame::encode(action));EXPECT_EQ(restored.from,9u);EXPECT_EQ(restored.to,3u);EXPECT_EQ(restored.kind,action.kind);
    action.from=36;EXPECT_THROW(vc::netgame::encode(action),std::runtime_error);
    action={vc::InventoryActionKind::MoveToBackpack,9};EXPECT_THROW(vc::netgame::encode(action),std::runtime_error);
    action={vc::InventoryActionKind::Craft,0,2,1};EXPECT_THROW(vc::netgame::encode(action),std::runtime_error);
    action.kind=static_cast<vc::InventoryActionKind>(99);EXPECT_THROW(vc::netgame::encode(action),std::runtime_error);
}
