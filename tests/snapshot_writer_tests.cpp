#include "engine/voxel/snapshot_writer.hpp"
#include "game/content/blocks.hpp"
#include "tests/temp_directory.hpp"
#include <gtest/gtest.h>
#include <fstream>

TEST(SnapshotWriter, OwnedSnapshotIsAtomicAndResultBoundsAcceptedWork) {
    vc::test::TempDirectory temp;vc::voxel::WorldStore store(temp.path());auto blocks=vc::builtinBlocks();vc::voxel::World world;
    world.insert(vc::voxel::Chunk({-1,0,0},blocks.require("voxelcraft:stone")));
    vc::voxel::SnapshotWriter writer(store);ASSERT_TRUE(writer.request(world,blocks,{},"first"));
    EXPECT_TRUE(writer.busy());EXPECT_FALSE(writer.request(world,blocks,{},"should not overwrite"));
    world.setBlock({-1,1,1},blocks.require("voxelcraft:lamp"));
    const auto first=writer.wait();EXPECT_FALSE(first.failure);EXPECT_EQ(first.revision,1u);EXPECT_FALSE(writer.busy());
    const auto archive=store.load(blocks);ASSERT_TRUE(archive);EXPECT_EQ(archive->gameData,"first");
    EXPECT_EQ(archive->world.block({-1,1,1}),blocks.require("voxelcraft:stone"));
    EXPECT_TRUE(writer.request(world,blocks,{},"second"));const auto second=writer.wait();EXPECT_FALSE(second.failure);EXPECT_EQ(second.revision,2u);
    const auto newer=store.load(blocks);ASSERT_TRUE(newer);EXPECT_EQ(newer->gameData,"second");EXPECT_EQ(newer->world.block({-1,1,1}),blocks.require("voxelcraft:lamp"));
    EXPECT_THROW(writer.wait(),std::logic_error);EXPECT_FALSE(writer.takeReady());
}
TEST(SnapshotWriter, FailedIoReportsToOwnerAndCanRetryWithoutLosingTheWorld) {
    vc::test::TempDirectory temp;vc::voxel::WorldStore store(temp.path());auto blocks=vc::builtinBlocks();vc::voxel::World world;
    world.insert(vc::voxel::Chunk({0,0,0},blocks.require("voxelcraft:stone")));
    { std::ofstream file(temp.path()/"snapshots");file<<"block directory creation"; }
    vc::voxel::SnapshotWriter writer(store);ASSERT_TRUE(writer.request(world,blocks,{},"preserve me"));
    const auto failed=writer.wait();EXPECT_TRUE(failed.failure);EXPECT_EQ(failed.revision,0u);EXPECT_THROW(failed.rethrowIfFailed(),std::exception);
    EXPECT_FALSE(writer.busy());std::filesystem::remove(temp.path()/"snapshots");ASSERT_TRUE(writer.request(world,blocks,{},"preserve me"));
    const auto retried=writer.wait();EXPECT_NO_THROW(retried.rethrowIfFailed());EXPECT_EQ(retried.revision,1u);
    const auto archive=store.load(blocks);ASSERT_TRUE(archive);EXPECT_EQ(archive->gameData,"preserve me");EXPECT_EQ(archive->world.chunks().size(),1u);
}
TEST(SnapshotWriter, DestructionCompletesAcceptedSaveAndPreservesSessionLockLifetime) {
    vc::test::TempDirectory temp;vc::voxel::WorldStore store(temp.path());auto blocks=vc::builtinBlocks();
    {
        vc::voxel::SnapshotWriter writer(store);ASSERT_TRUE(writer.request(vc::voxel::World{},blocks,{},"flushed before store destruction"));
    }
    const auto archive=store.load(blocks);ASSERT_TRUE(archive);EXPECT_EQ(archive->gameData,"flushed before store destruction");
    EXPECT_THROW(vc::voxel::WorldStore second(temp.path()),std::runtime_error);
}
