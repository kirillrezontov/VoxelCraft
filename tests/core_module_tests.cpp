#include "engine/voxel/mesh_queue.hpp"
#include "engine/voxel/mesh_worker.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <stdexcept>

namespace {
using namespace vc::voxel;
// An out-of-engine module, proving queues accept policies beyond the two builtins.
struct ReferenceModule {
    static vc::Mesh build(const Chunk& c, const World& w, const BlockRegistry& r,
        const vc::AtlasLayout& a, const Lighting* l) {
        return CulledChunkMesher::build(c,w,r,a,l);
    }
};
struct WrongResult { static int build(const Chunk&,const World&,const BlockRegistry&,const vc::AtlasLayout&,const Lighting*); };
static_assert(ChunkMesher<ReferenceModule> && ChunkMesher<GreedyChunkMesher> && ChunkMesher<CulledChunkMesher>);
static_assert(!ChunkMesher<WrongResult>);

template<ChunkMesher Module>
std::vector<vc::Mesh> settle(BasicBackgroundMeshQueue<Module>& queue, const World& world) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::vector<vc::Mesh> result;
    while (queue.pendingCount()) {
        auto ready = queue.update(world, {0,2,0});
        for (auto& mesh : ready) result.push_back(std::move(mesh));
        if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("Module mesh queue timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return result;
}
}

TEST(CoreModules, ReferencePolicyRunsThroughSynchronousAndBackgroundScheduling) {
    using namespace vc::voxel;
    const auto blocks = vc::builtinBlocks();
    const auto atlas = vc::builtinAtlasLayout();
    World world;
    world.insert(Chunk({-1,0,0}, blocks.require("voxelcraft:stone")));
    BasicChunkMeshQueue<ReferenceModule> direct;
    direct.collect(world);
    const auto expected = direct.rebuild(world,blocks,atlas,{0,2,0});
    ASSERT_EQ(expected.size(),1u);
    EXPECT_EQ(expected[0].vertices.size(),6u*32*32*4);
    EXPECT_EQ(GreedyChunkMesher::build(*world.findChunk({-1,0,0}),world,blocks,atlas,nullptr).vertices.size(),24u);
    BasicBackgroundMeshQueue<ReferenceModule> queue(blocks,atlas,1);
    const std::array chunks{Position{-1,0,0}};
    queue.invalidate(chunks);
    auto meshes = settle(queue,world);
    ASSERT_EQ(meshes.size(),1u);
    EXPECT_EQ(meshes[0].origin,expected[0].origin);
    EXPECT_EQ(meshes[0].indices,expected[0].indices);
    ASSERT_EQ(meshes[0].vertices.size(),expected[0].vertices.size());
    for (std::size_t i=0; i<meshes[0].vertices.size(); ++i) {
        EXPECT_EQ(meshes[0].vertices[i].position,expected[0].vertices[i].position);
        EXPECT_EQ(meshes[0].vertices[i].uv,expected[0].vertices[i].uv);
        EXPECT_EQ(meshes[0].vertices[i].light,expected[0].vertices[i].light);
    }
    // The same policy participates in edit supersession and immediate eviction.
    world.setBlock({-1,1,1},air);
    queue.collect(world);
    (void)queue.update(world,{0,2,0});
    (void)world.remove({-1,0,0});
    queue.collect(world);
    meshes = queue.update(world,{0,2,0});
    ASSERT_EQ(meshes.size(),1u);
    EXPECT_TRUE(meshes[0].vertices.empty());
    EXPECT_TRUE(settle(queue,world).empty());
}

namespace {
struct FailingModule {
    static vc::Mesh build(const Chunk&,const World&,const BlockRegistry&,const vc::AtlasLayout&,const Lighting*) {
        throw std::runtime_error("module failure");
    }
};
}
TEST(CoreModules, AlternativeWorkerContainsModuleExceptionAndJoins) {
    using namespace vc::voxel;
    const auto blocks = vc::builtinBlocks();
    World world;
    world.insert(Chunk({0,0,0}));
    BasicBackgroundMeshQueue<FailingModule> queue(blocks,vc::builtinAtlasLayout());
    queue.collect(world);
    try {
        (void)settle(queue,world);
        FAIL() << "Expected module failure";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "module failure");
    }
}
