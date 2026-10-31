#include "engine/voxel/persistence.hpp"
#include "game/content/blocks.hpp"
#include "tests/temp_directory.hpp"
#include <gtest/gtest.h>
#include <fstream>

TEST(WorldPersistence, LoadIsExcludedUntilSaveResultIsConsumedAndSnapshotsOwnTheirValues) {
    using namespace vc::voxel;
    vc::test::TempDirectory temp;
    WorldPersistence persistence(temp.path());
    auto blocks = vc::builtinBlocks();
    EXPECT_FALSE(persistence.load(blocks));
    World world;
    world.insert(Chunk({-1,0,0}, blocks.require("voxelcraft:stone")));
    ASSERT_TRUE(persistence.requestSave(world, blocks, {}, "original"));
    EXPECT_THROW(persistence.load(blocks), std::logic_error);
    EXPECT_FALSE(persistence.requestSave(world, blocks, {}, "replacement"));
    world.setBlock({-1,1,1}, blocks.require("voxelcraft:lamp"));
    const auto result = persistence.waitForSave();
    ASSERT_FALSE(result.failure);
    EXPECT_EQ(result.revision, 1u);
    const auto loaded = persistence.load(blocks);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->gameData, "original");
    EXPECT_EQ(loaded->world.block({-1,1,1}), blocks.require("voxelcraft:stone"));
    EXPECT_FALSE(persistence.takeSaveResult());
    EXPECT_THROW(persistence.waitForSave(), std::logic_error);
}

TEST(WorldPersistence, DestructionFinishesSaveBeforeReleasingExclusiveDirectoryLock) {
    using namespace vc::voxel;
    vc::test::TempDirectory temp;
    auto blocks = vc::builtinBlocks();
    for (int i = 0; i < 3; ++i) {
        WorldPersistence persistence(temp.path());
        EXPECT_THROW(WorldPersistence duplicate(temp.path()), std::runtime_error);
        ASSERT_TRUE(persistence.requestSave(World{}, blocks, {}, "shutdown"));
        // No explicit wait: the service owns the required destruction ordering.
    }
    WorldPersistence reopened(temp.path());
    const auto loaded = reopened.load(blocks);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->revision, 3u);
    EXPECT_EQ(loaded->gameData, "shutdown");
}

TEST(WorldPersistence, FailedSaveReportsToCallerAndCanBeRetried) {
    using namespace vc::voxel;
    vc::test::TempDirectory temp;
    WorldPersistence persistence(temp.path());
    auto blocks = vc::builtinBlocks();
    { std::ofstream file(temp.path()/"snapshots"); file << "block directory creation"; }
    ASSERT_TRUE(persistence.requestSave(World{}, blocks, {}, "kept"));
    const auto failed = persistence.waitForSave();
    ASSERT_TRUE(failed.failure);
    EXPECT_THROW(failed.rethrowIfFailed(), std::exception);
    std::filesystem::remove(temp.path()/"snapshots");
    ASSERT_TRUE(persistence.requestSave(World{}, blocks, {}, "kept"));
    EXPECT_NO_THROW(persistence.waitForSave().rethrowIfFailed());
    const auto loaded = persistence.load(blocks);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->gameData, "kept");
}
