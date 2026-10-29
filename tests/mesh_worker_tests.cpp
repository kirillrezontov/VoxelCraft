#include "engine/voxel/mesh_worker.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <limits>

namespace {
void equal(const vc::Mesh& actual,const vc::Mesh& expected) {
    EXPECT_EQ(actual.origin,expected.origin);EXPECT_EQ(actual.indices,expected.indices);EXPECT_EQ(actual.translucentIndices,expected.translucentIndices);
    ASSERT_EQ(actual.vertices.size(),expected.vertices.size());
    for(std::size_t i=0;i<actual.vertices.size();++i) {
        const auto& a=actual.vertices[i];const auto& b=expected.vertices[i];
        ASSERT_EQ(a.position,b.position);ASSERT_EQ(a.color,b.color);ASSERT_EQ(a.uv,b.uv);ASSERT_EQ(a.normal,b.normal);
        ASSERT_EQ(a.opacity,b.opacity);ASSERT_EQ(a.light,b.light);ASSERT_EQ(a.sway,b.sway);ASSERT_EQ(a.tileBounds,b.tileBounds);
    }
}
std::optional<vc::voxel::CompletedMesh> await(vc::voxel::MeshWorker& worker) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    do { if(auto result=worker.takeReady()) return result;std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    while(std::chrono::steady_clock::now()<end);return {};
}
std::vector<vc::Mesh> settle(vc::voxel::BackgroundMeshQueue& queue,const vc::voxel::World& world,const vc::voxel::Lighting* lighting=nullptr) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);std::vector<vc::Mesh> result;
    do {
        auto ready=queue.update(world,{0,2,0},2,lighting);
        for(auto& mesh:ready) result.push_back(std::move(mesh));
        if(!queue.pendingCount()) return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(std::chrono::steady_clock::now()<end);
    throw std::runtime_error("Background mesh queue did not settle");
}
}
TEST(MeshWorker, ImmutableChunkNeighborAndLightingSnapshotsMatchSynchronousGeometry) {
    using namespace vc::voxel;
    const auto blocks=vc::builtinBlocks();const auto atlas=vc::builtinAtlasLayout();World world;
    for(int x=-1;x<=1;++x) for(int y=-1;y<=1;++y) for(int z=-1;z<=1;++z) world.insert(Chunk({x,y,z}));
    world.setBlock({31,1,1},blocks.require("voxelcraft:stone"));world.setBlock({32,1,1},blocks.require("voxelcraft:water"));
    world.setBlock({31,1,2},blocks.require("voxelcraft:lamp"));world.setBlock({1,35,1},blocks.require("voxelcraft:stone"));
    Lighting lighting;const auto changed=world.takeDirtyChunks();(void)lighting.refresh(world,blocks,changed);
    const auto expected=meshChunk(*world.findChunk({0,0,0}),world,blocks,atlas,&lighting);
    MeshWorker worker(blocks,atlas,1);ASSERT_TRUE(worker.request(world,{0,0,0},42,&lighting));
    // Mutate source data immediately while the worker can be reading its job.
    world.setBlock({31,1,1},air);(void)world.remove({1,0,0});
    const auto newDirty=world.takeDirtyChunks();(void)lighting.refresh(world,blocks,newDirty);
    const auto ready=await(worker);ASSERT_TRUE(ready);EXPECT_EQ(ready->ticket,42u);equal(ready->mesh,expected);
    EXPECT_EQ(worker.pendingCount(),0u);
}
TEST(MeshWorker, CapacityIncludesQueuedInFlightAndCompletedWork) {
    using namespace vc::voxel;
    const auto blocks=vc::builtinBlocks();World world;world.insert(Chunk({0,0,0},1));world.insert(Chunk({1,0,0},1));
    MeshWorker worker(blocks,vc::builtinAtlasLayout(),1);ASSERT_TRUE(worker.request(world,{0,0,0},1));
    EXPECT_FALSE(worker.request(world,{0,0,0},2));EXPECT_FALSE(worker.request(world,{1,0,0},3));EXPECT_EQ(worker.pendingCount(),1u);
    ASSERT_TRUE(await(worker));EXPECT_TRUE(worker.request(world,{1,0,0},4));ASSERT_TRUE(await(worker));
    EXPECT_FALSE(worker.request(world,{2,0,0},5));EXPECT_THROW(worker.request(world,{0,0,0},0),std::invalid_argument);
    EXPECT_THROW(worker.request(world,{std::numeric_limits<std::int64_t>::max(),0,0},1),std::out_of_range);
    EXPECT_THROW(MeshWorker(blocks,vc::builtinAtlasLayout(),0),std::invalid_argument);
    EXPECT_THROW(MeshWorker(blocks,vc::builtinAtlasLayout(),5),std::invalid_argument);
}
TEST(BackgroundMeshing, EditsAndNewNeighborSupersedeAlreadyQueuedGeometry) {
    using namespace vc::voxel;
    const auto blocks=vc::builtinBlocks();World world;world.insert(Chunk({0,0,0},blocks.require("voxelcraft:stone")));
    BackgroundMeshQueue queue(blocks,vc::builtinAtlasLayout(),1);queue.collect(world);EXPECT_TRUE(queue.update(world,{0,2,0}).empty());
    world.setBlock({1,1,1},air);world.insert(Chunk({1,0,0},blocks.require("voxelcraft:stone")));queue.collect(world);
    const auto meshes=settle(queue,world);ASSERT_EQ(meshes.size(),2u);
    for(const auto& mesh:meshes) {
        const Position p{mesh.origin.x/32,mesh.origin.y/32,mesh.origin.z/32};
        equal(mesh,meshChunk(*world.findChunk(p),world,blocks,vc::builtinAtlasLayout()));
    }
}
TEST(BackgroundMeshing, EvictionRemovesImmediatelyAndOldWorkCannotResurrectReinsertedChunks) {
    using namespace vc::voxel;
    const auto blocks=vc::builtinBlocks();World world;world.insert(Chunk({0,0,0},blocks.require("voxelcraft:stone")));
    BackgroundMeshQueue queue(blocks,vc::builtinAtlasLayout(),1);queue.collect(world);(void)queue.update(world,{0,2,0});
    (void)world.remove({0,0,0});queue.collect(world);const auto removal=queue.update(world,{0,2,0});
    ASSERT_EQ(removal.size(),1u);EXPECT_TRUE(removal[0].indices.empty());EXPECT_TRUE(removal[0].vertices.empty());
    world.insert(Chunk({0,0,0}));world.setBlock({2,2,2},blocks.require("voxelcraft:water"));queue.collect(world);
    const auto meshes=settle(queue,world);ASSERT_EQ(meshes.size(),1u);EXPECT_TRUE(meshes[0].indices.empty());EXPECT_FALSE(meshes[0].translucentIndices.empty());
    equal(meshes[0],meshChunk(*world.findChunk({0,0,0}),world,blocks,vc::builtinAtlasLayout()));
}
TEST(BackgroundMeshing, LightOnlyInvalidationDiscardsAJobWithUnchangedCenterData) {
    using namespace vc::voxel;
    const auto blocks=vc::builtinBlocks();World world;world.insert(Chunk({0,0,0}));world.insert(Chunk({1,0,0}));
    world.setBlock({31,1,1},blocks.require("voxelcraft:stone"));Lighting lighting;auto dirty=world.takeDirtyChunks();(void)lighting.refresh(world,blocks,dirty);
    BackgroundMeshQueue queue(blocks,vc::builtinAtlasLayout(),1);const std::array center{Position{0,0,0}};queue.invalidate(center);
    (void)queue.update(world,{0,2,0},1,&lighting);const auto revision=world.revision({0,0,0});
    world.setBlock({33,1,1},blocks.require("voxelcraft:lamp"));dirty=world.takeDirtyChunks();const auto lightDirty=lighting.refresh(world,blocks,dirty);
    EXPECT_EQ(world.revision({0,0,0}),revision);queue.invalidate(lightDirty);
    const auto meshes=settle(queue,world,&lighting);bool found=false;
    for(const auto& mesh:meshes) if(mesh.origin==glm::i64vec3(0)) { equal(mesh,meshChunk(*world.findChunk({0,0,0}),world,blocks,vc::builtinAtlasLayout(),&lighting));found=true; }
    EXPECT_TRUE(found);
}
TEST(BackgroundMeshing, NearFirstSchedulingAndSafeWorkerShutdown) {
    using namespace vc::voxel;
    const auto blocks=vc::builtinBlocks();World world;world.insert(Chunk({-3,0,0},1));world.insert(Chunk({0,0,0},1));
    BackgroundMeshQueue queue(blocks,vc::builtinAtlasLayout(),1);queue.collect(world);
    const auto meshes=settle(queue,world);ASSERT_EQ(meshes.size(),2u);EXPECT_EQ(meshes[0].origin,glm::i64vec3(0));
    EXPECT_THROW(queue.update(world,{0,0,0},0),std::invalid_argument);
    for(int i=0;i<6;++i) { MeshWorker worker(blocks,vc::builtinAtlasLayout());if(i%2==0) { EXPECT_TRUE(worker.request(world,{0,0,0},1)); } }
    World invalid;invalid.insert(Chunk({0,0,0},65535));MeshWorker worker(blocks,vc::builtinAtlasLayout());ASSERT_TRUE(worker.request(invalid,{0,0,0},1));
    EXPECT_THROW(await(worker),std::out_of_range);
}
