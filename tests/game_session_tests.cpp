#include "client/game_session.hpp"
#include "engine/voxel/snapshot_writer.hpp"
#include "tests/temp_directory.hpp"
#include <gtest/gtest.h>
#include <vector>

namespace {
struct Owner {
    std::vector<int>& events;int value;
    ~Owner() { events.push_back(value); }
};
vc::client::GameSession suspended(std::vector<int>& events) {
    Owner owner{events,1};
    Owner borrower{events,2};
    co_await std::suspend_always{};
    co_return vc::client::SessionExit::Menu;
}
vc::client::GameSession fails() {
    co_await std::suspend_always{};
    throw std::runtime_error("session failed");
    co_return vc::client::SessionExit::Menu;
}
vc::client::GameSession saveThenSuspend(const std::filesystem::path& path) {
    vc::voxel::WorldStore store(path);
    vc::voxel::SnapshotWriter writer(store);
    vc::voxel::World world;vc::voxel::BlockRegistry blocks;
    if(!writer.request(std::move(world),std::move(blocks),{})) throw std::runtime_error("save rejected");
    co_await std::suspend_always{};
    co_return vc::client::SessionExit::Menu;
}
}
TEST(GameSession, CancellationAndMovePreserveUniqueOwnershipAndReverseDestruction) {
    std::vector<int> events;
    auto session=suspended(events);
    EXPECT_TRUE(events.empty()); // Initial suspension has no side effects.
    session.resume();ASSERT_FALSE(session.done());
    auto moved=std::move(session);EXPECT_TRUE(session.done());
    EXPECT_THROW(moved.result(),std::logic_error);
    moved.reset();EXPECT_EQ(events,(std::vector<int>{2,1}));
    moved.reset();EXPECT_EQ(events.size(),2u);
}
TEST(GameSession, FailureIsReportedToApplicationAndCompletionCanBeObserved) {
    auto session=fails();session.resume();EXPECT_FALSE(session.done());
    session.resume();EXPECT_TRUE(session.done());EXPECT_THROW(session.result(),std::runtime_error);
    std::vector<int> events;auto complete=suspended(events);complete.resume();complete.resume();
    EXPECT_EQ(complete.result(),vc::client::SessionExit::Menu);
    EXPECT_EQ(events,(std::vector<int>{2,1}));
}
TEST(GameSession, CancellationCompletesAcceptedSaveBeforeReleasingStoreLock) {
    vc::test::TempDirectory directory;
    auto session=saveThenSuspend(directory.path());session.resume();
    EXPECT_THROW(vc::voxel::WorldStore(directory.path()),std::runtime_error);
    session.reset();
    vc::voxel::WorldStore reopened(directory.path());vc::voxel::BlockRegistry blocks;
    const auto archive=reopened.load(blocks);ASSERT_TRUE(archive);EXPECT_EQ(archive->revision,1u);
}
