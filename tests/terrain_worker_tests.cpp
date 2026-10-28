#include "engine/voxel/terrain_worker.hpp"
#include "game/content/blocks.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <limits>

namespace {
std::optional<vc::voxel::Chunk> await(vc::voxel::TerrainWorker& worker) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
        if (auto chunk = worker.takeReady()) return chunk;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    return std::nullopt;
}
}
TEST(TerrainWorker, BoundedDeduplicatedResultsMatchSynchronousGeneration) {
    const auto registry = vc::builtinBlocks();
    const vc::voxel::TerrainGenerator generator(42,{},registry);
    vc::voxel::TerrainWorker worker(generator,2);
    EXPECT_TRUE(worker.request({-1,0,-1}));
    EXPECT_FALSE(worker.request({-1,0,-1}));
    EXPECT_TRUE(worker.request({0,0,0}));
    EXPECT_FALSE(worker.request({1,0,0}));
    EXPECT_EQ(worker.pendingCount(),2u);
    for (int i=0;i<2;++i) {
        auto result=await(worker); ASSERT_TRUE(result);
        const auto expected=generator.generate(result->position());
        for(int y=0;y<32;++y) for(int z=0;z<32;++z) for(int x=0;x<32;++x)
            ASSERT_EQ(result->get({x,y,z}),expected.get({x,y,z}));
    }
    EXPECT_EQ(worker.pendingCount(),0u);
    EXPECT_FALSE(worker.takeReady());
    EXPECT_TRUE(worker.request({-1,0,-1}));
    ASSERT_TRUE(await(worker));
}
TEST(TerrainWorker, ShutdownIdleOrWithPendingWorkAndInvalidRequests) {
    const auto registry=vc::builtinBlocks();
    const vc::voxel::TerrainGenerator generator(1,{},registry);
    EXPECT_THROW(vc::voxel::TerrainWorker(generator,0),std::invalid_argument);
    EXPECT_THROW(vc::voxel::TerrainWorker(generator,129),std::invalid_argument);
    for(int n=0;n<8;++n) {
        vc::voxel::TerrainWorker worker(generator);
        EXPECT_THROW(worker.request({std::numeric_limits<std::int64_t>::max(),0,0}),std::out_of_range);
        if(n%2==0) {
            for(int x=0;x<8;++x) EXPECT_TRUE(worker.request({x,0,0}));
        }
    }
}
