#include "engine/voxel/terrain_worker.hpp"
#include "examples/core_modules/layered_generator.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <stdexcept>

namespace {
template<vc::voxel::ChunkGenerator Generator>
std::optional<vc::voxel::Chunk> await(vc::voxel::BasicTerrainWorker<Generator>& worker) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
        if (auto chunk = worker.takeReady()) return chunk;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    throw std::runtime_error("Core generator worker timed out");
}
struct InvalidGenerator { int generate(vc::voxel::Position) const { return 0; } };
struct ThrowingGenerator {
    vc::voxel::Chunk generate(vc::voxel::Position) const { throw std::runtime_error("generation failed"); }
};
struct MisplacedGenerator {
    vc::voxel::Chunk generate(vc::voxel::Position) const { return vc::voxel::Chunk({9, 0, 0}); }
};
struct MoveOnlyGenerator {
    std::unique_ptr<int> value = std::make_unique<int>(7);
    vc::voxel::Chunk generate(vc::voxel::Position p) const {
        return vc::voxel::Chunk(p, static_cast<vc::voxel::BlockId>(*value));
    }
};
static_assert(vc::voxel::ChunkGenerator<vc::examples::LayeredGenerator>);
static_assert(vc::voxel::ChunkGenerator<vc::voxel::TerrainGenerator>);
static_assert(!vc::voxel::ChunkGenerator<InvalidGenerator>);
static_assert(!std::is_polymorphic_v<vc::voxel::TerrainWorker>);
}

TEST(CoreModule, AlternateGeneratorUsesUnchangedBoundedWorkerAndWorld) {
    using namespace vc::voxel;
    BlockRegistry blocks;
    const auto material = blocks.add({"example:layer", true, {0.4f, 0.6f, 0.8f}});
    const WorldBounds bounds{-17, 13};
    vc::examples::LayeredGenerator generator(bounds, material, -3);
    BasicTerrainWorker<vc::examples::LayeredGenerator> worker(generator, 2);
    EXPECT_TRUE(worker.request({-1, -1, -1}));
    EXPECT_FALSE(worker.request({-1, -1, -1}));
    EXPECT_TRUE(worker.request({-1, 0, -1}));
    EXPECT_FALSE(worker.request({0, 0, 0}));
    World world(bounds);
    for (int i = 0; i < 2; ++i) world.insert(std::move(*await(worker)));
    EXPECT_EQ(worker.pendingCount(), 0u);
    EXPECT_EQ(worker.metrics().completed, 2u);
    for (int y = -32; y < 32; ++y)
        EXPECT_EQ(world.block({-1, y, -1}), y >= -17 && y <= -3 ? material : air);
    EXPECT_TRUE(worker.request({-1, -1, -1}));
    EXPECT_EQ(await(worker)->position(), (Position{-1, -1, -1}));
}

TEST(CoreModule, OwnsMoveOnlyPolicyAndReportsFailuresOnCaller) {
    vc::voxel::BasicTerrainWorker<MoveOnlyGenerator> worker(MoveOnlyGenerator{}, 1);
    ASSERT_TRUE(worker.request({0, 0, 0}));
    EXPECT_EQ(await(worker)->get({0, 0, 0}), 7);
    vc::voxel::BasicTerrainWorker<ThrowingGenerator> failing(ThrowingGenerator{});
    ASSERT_TRUE(failing.request({0, 0, 0}));
    EXPECT_THROW(await(failing), std::runtime_error);
    EXPECT_THROW(failing.request({1, 0, 0}), std::runtime_error);
    vc::voxel::BasicTerrainWorker<MisplacedGenerator> misplaced(MisplacedGenerator{});
    ASSERT_TRUE(misplaced.request({0, 0, 0}));
    EXPECT_THROW(await(misplaced), std::runtime_error);
}
