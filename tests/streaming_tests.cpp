#include "engine/voxel/streaming.hpp"
#include "engine/voxel/storage.hpp"
#include "engine/voxel/mesher.hpp"
#include "engine/voxel/mesh_queue.hpp"
#include "game/content/blocks.hpp"
#include "game/content/atlas.hpp"
#include "tests/temp_directory.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <limits>
#include <algorithm>

namespace {
bool settle(vc::voxel::ChunkStreamer& streamer,vc::voxel::Position player) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    do {
        streamer.tick(player);
        if(streamer.settled()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(std::chrono::steady_clock::now()<deadline);
    return false;
}
}
TEST(PackedChunk, UniformAndWorstCaseDataRoundTripWithinRawSize) {
    using namespace vc::voxel;
    Chunk uniform({-1,0,1},65535);PackedChunk packed(uniform);EXPECT_EQ(packed.bytes(),4u);
    EXPECT_EQ(packed.unpack().get({31,31,31}),65535);
    EXPECT_EQ(packed.get({31,31,31}),65535);
    Chunk checkerboard({-1,0,-1});
    for(int y=0;y<32;++y) for(int z=0;z<32;++z) for(int x=0;x<32;++x) checkerboard.set({x,y,z},static_cast<BlockId>((x+y+z)%2+1));
    const PackedChunk worst(checkerboard);EXPECT_LE(worst.bytes(),chunkVolume*sizeof(BlockId));
    const auto unpacked=worst.unpack();EXPECT_EQ(unpacked.position(),checkerboard.position());
    for(int y=0;y<32;++y) for(int z=0;z<32;++z) for(int x=0;x<32;++x) {
        ASSERT_EQ(unpacked.get({x,y,z}),checkerboard.get({x,y,z}));ASSERT_EQ(worst.get({x,y,z}),checkerboard.get({x,y,z}));
    }
}
TEST(WorldResidency, RemovalInvalidatesGpuOriginAndResidentFaceNeighbors) {
    using namespace vc::voxel;
    World world;world.insert(Chunk({-1,0,0},1));world.insert(Chunk({0,0,0},1));world.insert(Chunk({1,0,0},1));
    (void)world.takeDirtyChunks();world.setBlock({0,1,1},0);EXPECT_TRUE(world.modified({0,0,0}));(void)world.takeDirtyChunks();
    auto removed=world.remove({0,0,0});ASSERT_TRUE(removed);EXPECT_FALSE(world.loaded({0,1,1}));
    EXPECT_FALSE(world.modified({0,0,0}));
    EXPECT_EQ(world.takeDirtyChunks(),(std::vector<Position>{{-1,0,0},{0,0,0},{1,0,0}}));
    EXPECT_FALSE(world.remove({0,0,0}));EXPECT_TRUE(world.takeDirtyChunks().empty());
}
TEST(Streaming, RegionIncludesPlayerSurfaceAndClipsExtremeBounds) {
    using namespace vc::voxel;
    const auto normal=streamingRegion({-1,15,-1},{},1);EXPECT_EQ(normal.size(),27u);
    EXPECT_EQ(normal.front(),(Position{-1,0,-1}));
    const auto high=streamingRegion({0,609,0},{607,700},1);
    EXPECT_NE(std::find(high.begin(),high.end(),Position{0,19,0}),high.end());
    EXPECT_NE(std::find(high.begin(),high.end(),Position{0,18,0}),high.end());
    const auto deep=streamingRegion({0,-400,0},{},1);EXPECT_LE(deep.size(),54u);
    EXPECT_NE(std::find(deep.begin(),deep.end(),Position{0,-13,0}),deep.end());
    const auto extreme=streamingRegion({std::numeric_limits<std::int64_t>::max(),0,std::numeric_limits<std::int64_t>::min()},{},1);
    EXPECT_EQ(extreme.size(),12u);
    for(const auto p:extreme) EXPECT_NO_THROW((void)chunkOrigin(p));
}
TEST(Streaming, EvictionRevisitAndPersistentSnapshotKeepEditsOnly) {
    using namespace vc::voxel;
    auto registry=vc::builtinBlocks();World world;const TerrainGenerator generator(42,{},registry);
    ChunkStreamer streamer(world,generator,1);ASSERT_TRUE(settle(streamer,{-1,20,-1}));
    ASSERT_EQ(world.chunks().size(),27u);
    const Position edited{-1,30,-1};ASSERT_TRUE(world.setBlock(edited,registry.require("voxelcraft:stone")));
    ASSERT_TRUE(settle(streamer,{512,20,512}));EXPECT_FALSE(world.loaded(edited));EXPECT_EQ(streamer.cachedCount(),1u);
    EXPECT_LE(world.chunks().size(),54u);EXPECT_LE(streamer.cachedBytes(),chunkVolume*sizeof(BlockId));
    auto snapshot=streamer.snapshot();ASSERT_EQ(snapshot.chunks().size(),1u);EXPECT_EQ(snapshot.block(edited),registry.require("voxelcraft:stone"));
    vc::test::TempDirectory temp;WorldStore store(temp.path());store.save(snapshot,registry,{42,{},1});
    const auto saved=store.load(registry);ASSERT_TRUE(saved);EXPECT_EQ(saved->world.block(edited),registry.require("voxelcraft:stone"));
    ASSERT_TRUE(settle(streamer,{-1,20,-1}));EXPECT_EQ(world.block(edited),registry.require("voxelcraft:stone"));
    EXPECT_EQ(world.block({-2,30,-1}),generator.sample({-2,30,-1}));
    world.setBlock(edited,air);ASSERT_TRUE(settle(streamer,{512,20,512}));ASSERT_TRUE(settle(streamer,{-1,20,-1}));
    EXPECT_EQ(world.block(edited),air);EXPECT_EQ(streamer.snapshot().block(edited),air);
}
TEST(Streaming, TeleportDiscardsStaleGenerationAndRetainsLegacySnapshots) {
    using namespace vc::voxel;
    const auto registry=vc::builtinBlocks();const TerrainGenerator generator(42,{},registry);World world;
    world.insert(Chunk({-10,0,-10},registry.require("voxelcraft:dirt")));
    const std::array retained{Position{-10,0,-10}};
    ChunkStreamer streamer(world,generator,1);streamer.retain(retained);
    streamer.tick({0,20,0});streamer.tick({1024,20,-1024});
    ASSERT_TRUE(settle(streamer,{1024,20,-1024}));
    const auto wanted=streamingRegion({1024,20,-1024},{},1);
    EXPECT_EQ(world.chunks().size(),wanted.size());
    for(const auto& [p,chunk]:world.chunks()) { (void)chunk;EXPECT_NE(std::find(wanted.begin(),wanted.end(),p),wanted.end()); }
    const auto snapshot=streamer.snapshot();EXPECT_EQ(snapshot.chunks().size(),1u);EXPECT_EQ(snapshot.block({-320,0,-320}),registry.require("voxelcraft:dirt"));
    EXPECT_EQ(streamer.pendingCount(),0u);
}
TEST(MeshQueue, NearFirstBudgetAndUnconditionalGpuRemoval) {
    using namespace vc::voxel;
    const auto registry=vc::builtinBlocks();World world;ChunkMeshQueue queue;
    world.insert(Chunk({0,0,0},registry.require("voxelcraft:stone")));
    world.insert(Chunk({-3,0,0},registry.require("voxelcraft:stone")));
    queue.collect(world);
    auto meshes=queue.rebuild(world,registry,vc::builtinAtlasLayout(),{0,20,0},1);
    ASSERT_EQ(meshes.size(),1u);EXPECT_EQ(meshes.front().origin,glm::i64vec3(0));EXPECT_EQ(queue.pendingCount(),1u);
    (void)world.remove({0,0,0});queue.collect(world);
    meshes=queue.rebuild(world,registry,vc::builtinAtlasLayout(),{0,20,0},1);
    ASSERT_EQ(meshes.size(),2u);EXPECT_TRUE(meshes.front().indices.empty());
    EXPECT_FALSE(meshes.back().indices.empty());EXPECT_EQ(queue.pendingCount(),0u);
}
TEST(Streaming, MultiplePlayersKeepUnionResidencyAndRetainEditsWhenTheLastLeaves) {
    using namespace vc::voxel;
    const auto registry=vc::builtinBlocks();const TerrainGenerator generator(42,{},registry,1);World world;
    ChunkStreamer streamer(world,generator,1);const std::array players{Position{-1,20,-1},Position{512,20,512}};
    const auto settleGroup=[&](std::span<const Position> group) {
        const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        do { streamer.tick(group);if(streamer.settled()) return true;std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        while(std::chrono::steady_clock::now()<end);
        return false;
    };
    ASSERT_TRUE(settleGroup(players));EXPECT_EQ(world.chunks().size(),54u);
    ASSERT_TRUE(world.setBlock({-1,30,-1},registry.require("voxelcraft:stone")));
    const std::array second{players[1]};ASSERT_TRUE(settleGroup(second));EXPECT_EQ(world.chunks().size(),27u);EXPECT_FALSE(world.loaded({-1,30,-1}));
    EXPECT_EQ(streamer.cachedCount(),1u);ASSERT_TRUE(settleGroup(players));EXPECT_EQ(world.block({-1,30,-1}),registry.require("voxelcraft:stone"));
    ASSERT_TRUE(settleGroup({}));EXPECT_TRUE(world.chunks().empty());EXPECT_EQ(streamer.snapshot().block({-1,30,-1}),registry.require("voxelcraft:stone"));
    EXPECT_EQ(streamer.sample({-1,30,-1}),registry.require("voxelcraft:stone"));
    EXPECT_EQ(streamer.sample({512,30,512}),generator.sample({512,30,512}));
    const std::array<Position,9> tooMany{};EXPECT_THROW(streamer.tick(tooMany),std::invalid_argument);
}
TEST(WorldResidency, DataRevisionsChangeOnEditsAndReplacementButNotDirtyNeighbors) {
    using namespace vc::voxel;
    World world;world.insert(Chunk({0,0,0}));const auto first=world.revision({0,0,0});EXPECT_NE(first,0u);
    world.insert(Chunk({1,0,0}));EXPECT_EQ(world.revision({0,0,0}),first);
    ASSERT_TRUE(world.setBlock({31,0,0},1));const auto changed=world.revision({0,0,0});EXPECT_GT(changed,first);
    const auto neighbor=world.revision({1,0,0});ASSERT_FALSE(world.setBlock({31,0,0},1));EXPECT_EQ(world.revision({0,0,0}),changed);
    (void)world.remove({0,0,0});EXPECT_EQ(world.revision({0,0,0}),0u);EXPECT_EQ(world.revision({1,0,0}),neighbor);
    world.insert(Chunk({0,0,0}));EXPECT_GT(world.revision({0,0,0}),changed);EXPECT_EQ(world.revision({1,0,0}),neighbor);
}
