#include "engine/voxel/raycast.hpp"
#include "engine/voxel/mesher.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include "game/gameplay/interaction.hpp"
#include "game/player/controller.hpp"
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <set>

using namespace vc::voxel;

namespace {
World emptyWorld() {
    World world;
    for (int x = -1; x <= 1; ++x) for (int y = -1; y <= 1; ++y) for (int z = -1; z <= 1; ++z)
        world.insert(Chunk({x,y,z}));
    (void)world.takeDirtyChunks();
    return world;
}
}
TEST(Raycast, SixDirectionsAndNormals) {
    auto world = emptyWorld();
    for (const auto step : {LocalPosition{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}}) {
        const Position target{step.x*3, step.y*3, step.z*3};
        ASSERT_TRUE(world.setBlock(target, 1));
        const auto hit = raycast(world, {0.5,0.5,0.5}, {step.x,step.y,step.z});
        ASSERT_TRUE(hit); EXPECT_EQ(hit->block, target);
        EXPECT_EQ(hit->normal, (LocalPosition{-step.x,-step.y,-step.z}));
        EXPECT_DOUBLE_EQ(hit->distance, 2.5);
        EXPECT_FALSE(raycast(world, {0.5,0.5,0.5}, {step.x,step.y,step.z}, 2.49));
        ASSERT_TRUE(world.setBlock(target, air));
    }
}
TEST(Raycast, StartingCellsAndGridPlanes) {
    auto world = emptyWorld(); world.setBlock({0,0,0},1); world.setBlock({-1,0,0},2);
    auto hit = raycast(world, {0.5,0.5,0.5}, {1,0,0});
    ASSERT_TRUE(hit); EXPECT_EQ(hit->normal, LocalPosition{}); EXPECT_EQ(hit->distance, 0);
    hit = raycast(world, {0,0.5,0.5}, {-1,0,0});
    ASSERT_TRUE(hit); EXPECT_EQ(hit->block, (Position{-1,0,0})); EXPECT_EQ(hit->normal, (LocalPosition{1,0,0}));
    hit = raycast(world, {0,0.5,0.5}, {1,0,0});
    ASSERT_TRUE(hit); EXPECT_EQ(hit->block, (Position{0,0,0}));
}
TEST(Raycast, NearestHitNegativeChunkSeamsAndDirectionScale) {
    auto world = emptyWorld(); world.setBlock({-32,0,0},1); world.setBlock({-31,0,0},1);
    for (const double length : {1e-300, 1.0, 100.0}) {
        const auto hit = raycast(world, {-28.5,0.5,0.5}, {-length,0,0});
        ASSERT_TRUE(hit); EXPECT_EQ(hit->block, (Position{-31,0,0})); EXPECT_DOUBLE_EQ(hit->distance, 1.5);
    }
}
TEST(Raycast, CornerCrossingsDoNotHitEdgeOnlyCells) {
    auto world = emptyWorld();
    world.setBlock({1,0,0},1); world.setBlock({0,1,0},1); world.setBlock({0,0,1},1); world.setBlock({2,2,2},1);
    const auto hit = raycast(world, {0.5,0.5,0.5}, {1,1,1});
    ASSERT_TRUE(hit); EXPECT_EQ(hit->block, (Position{2,2,2}));
    EXPECT_NEAR(hit->distance, 1.5*std::sqrt(3.0), 1e-12);
    EXPECT_EQ(hit->normal, (LocalPosition{-1,0,0}));
}
TEST(Raycast, InvalidInputAndEmptyWorld) {
    World world;
    EXPECT_FALSE(raycast(world,{0,0,0},{1,0,0}));
    EXPECT_THROW(raycast(world,{0,0,0},{0,0,0}), std::invalid_argument);
    EXPECT_THROW(raycast(world,{0,0,0},{1,0,0},-1), std::invalid_argument);
    EXPECT_THROW(raycast(world,{0,0,0},{1,0,0},1025), std::invalid_argument);
    EXPECT_THROW(raycast(world,{std::numeric_limits<double>::infinity(),0,0},{1,0,0}), std::invalid_argument);
    EXPECT_THROW(raycast(world,{std::ldexp(1.0,63),0,0},{1,0,0}), std::invalid_argument);
    EXPECT_THROW(raycast(world,{0,0,0},{std::numeric_limits<double>::quiet_NaN(),0,0}), std::invalid_argument);
    EXPECT_FALSE(raycast(world,{-std::ldexp(1.0,63),0.5,0.5},{-1,0,0}));
}
TEST(WorldEdits, DirtyChunksAtInteriorFacesAndCorners) {
    auto world = emptyWorld();
    EXPECT_TRUE(world.setBlock({4,5,6},1));
    EXPECT_EQ(world.takeDirtyChunks(), (std::vector<Position>{{0,0,0}}));
    EXPECT_FALSE(world.setBlock({4,5,6},1)); EXPECT_TRUE(world.takeDirtyChunks().empty());
    EXPECT_TRUE(world.setBlock({0,0,0},1));
    auto dirty = world.takeDirtyChunks();
    EXPECT_EQ((std::set<Position>(dirty.begin(),dirty.end())), (std::set<Position>{{0,0,0},{-1,0,0},{0,-1,0},{0,0,-1}}));
    EXPECT_TRUE(world.setBlock({-1,-1,-1},1));
    dirty = world.takeDirtyChunks();
    EXPECT_EQ((std::set<Position>(dirty.begin(),dirty.end())), (std::set<Position>{{-1,-1,-1},{0,-1,-1},{-1,0,-1},{-1,-1,0}}));
    EXPECT_FALSE(world.setBlock({1000,0,0},1)); EXPECT_FALSE(world.setBlock({0,512,0},1));
    EXPECT_TRUE(world.takeDirtyChunks().empty());
}
TEST(WorldEdits, RemovedBoundaryFaceReappearsInNeighborMesh) {
    const auto registry = vc::builtinBlocks(); World world;
    world.insert(Chunk({-1,0,0})); world.insert(Chunk({0,0,0})); (void)world.takeDirtyChunks();
    world.setBlock({-1,0,0},1); world.setBlock({0,0,0},1); (void)world.takeDirtyChunks();
    auto mesh = meshChunk(*world.findChunk({-1,0,0}), world, registry, vc::builtinAtlasLayout());
    EXPECT_EQ(mesh.indices.size(),30u);
    world.setBlock({0,0,0},air);
    EXPECT_EQ(world.takeDirtyChunks(), (std::vector<Position>{{-1,0,0},{0,0,0}}));
    mesh = meshChunk(*world.findChunk({-1,0,0}), world, registry, vc::builtinAtlasLayout());
    EXPECT_EQ(mesh.indices.size(),36u);
    EXPECT_TRUE(meshChunk(*world.findChunk({0,0,0}), world, registry, vc::builtinAtlasLayout()).indices.empty());
}

TEST(Editing, PlacementDestructionAndPlayerOverlap) {
    auto world = emptyWorld(); const auto registry = vc::builtinBlocks();
    world.setBlock({0,0,0},1); (void)world.takeDirtyChunks();
    const RayHit hit{{0,0,0},{1,0,0},1};
    const Aabb farPlayer{{10,1,10},{10.6,2.8,10.6}};
    EXPECT_TRUE(vc::placeBlock(world, registry, hit, registry.require("voxelcraft:dirt"), farPlayer));
    EXPECT_EQ(world.block({1,0,0}), registry.require("voxelcraft:dirt"));
    EXPECT_FALSE(vc::placeBlock(world, registry, hit, 1, farPlayer));
    EXPECT_TRUE(vc::destroyBlock(world, {{1,0,0},{-1,0,0},0}));
    EXPECT_FALSE(vc::destroyBlock(world, {{1,0,0},{-1,0,0},0}));
    EXPECT_FALSE(vc::placeBlock(world, registry, hit, 1, {{1.1,0,0},{1.7,1.8,0.6}}));
    EXPECT_EQ(world.block({1,0,0}), air);
    EXPECT_FALSE(vc::placeBlock(world, registry, {{0,0,0},{},0}, 1, farPlayer));
    EXPECT_FALSE(vc::placeBlock(world, registry, {{0,0,0},{INT_MIN,0,0},0}, 1, farPlayer));
    EXPECT_FALSE(vc::placeBlock(world, registry, hit, air, farPlayer));
    EXPECT_THROW(vc::placeBlock(world, registry, hit, 65535, farPlayer), std::out_of_range);
    EXPECT_TRUE(vc::placeBlock(world, registry, hit, 1, {{2,0,0},{2.6,1.8,0.6}})); // Touching is allowed.
}
TEST(Editing, HeightAndResidencyBoundaries) {
    const auto registry = vc::builtinBlocks(); World world({0,0}); world.insert(Chunk({0,0,0}));
    world.setBlock({0,0,0},1);
    const Aabb player{{10,5,10},{10.6,6.8,10.6}};
    EXPECT_FALSE(vc::placeBlock(world, registry, {{0,0,0},{0,1,0},0},1,player));
    EXPECT_FALSE(vc::placeBlock(world, registry, {{0,0,0},{-1,0,0},0},1,player));
    EXPECT_FALSE(vc::destroyBlock(world, {{0,-1,0},{0,1,0},0}));
}
TEST(Collision, SweptWallsAndSlidingWithoutTunneling) {
    auto world = emptyWorld(); const auto registry = vc::builtinBlocks();
    world.setBlock({3,1,0},1); world.setBlock({3,2,0},1);
    const Aabb box{{0.2,1,0.2},{0.8,2.8,0.8}};
    const auto moved = moveAndCollide(world,registry,box,{10,0,0.5});
    EXPECT_TRUE(moved.blocked.x); EXPECT_FALSE(moved.blocked.z);
    EXPECT_NEAR(moved.applied.x,2.2,1e-6); EXPECT_DOUBLE_EQ(moved.applied.z,0.5);
    EXPECT_FALSE(collides(world,registry,{box.min+moved.applied,box.max+moved.applied}));
    world.setBlock({-3,1,0},1); world.setBlock({-3,2,0},1);
    const auto negative = moveAndCollide(world,registry,box,{-10,0,0});
    EXPECT_TRUE(negative.blocked.x); EXPECT_NEAR(negative.applied.x,-2.2,1e-6);
}
TEST(Collision, FloorCeilingAndIndependentSolidity) {
    auto world = emptyWorld(); auto registry = vc::builtinBlocks();
    world.setBlock({0,0,0},1); world.setBlock({0,4,0},1);
    const Aabb box{{0.2,1,0.2},{0.8,2.8,0.8}};
    EXPECT_FALSE(collides(world,registry,box));
    EXPECT_TRUE(moveAndCollide(world,registry,box,{0,-20,0}).blocked.y);
    const auto up = moveAndCollide(world,registry,box,{0,10,0});
    EXPECT_TRUE(up.blocked.y); EXPECT_NEAR(up.applied.y,1.2,1e-6);
    const auto glass = registry.add({"test:glass",false,{1,1,1},{},true});
    world.setBlock({0,1,0},glass); EXPECT_TRUE(collides(world,registry,box));
    const auto decoration = registry.add({"test:decoration",true,{1,1,1},{},false});
    world.setBlock({0,1,0},decoration); EXPECT_FALSE(collides(world,registry,box));
    EXPECT_FALSE(registry.definition(air).solid);
}
TEST(Collision, MissingChunksBoundsAndInvalidInputs) {
    const auto registry = vc::builtinBlocks(); World world({0,31}); world.insert(Chunk({0,0,0}));
    EXPECT_TRUE(collides(world,registry,{{-0.5,1,0.2},{0.1,2.8,0.8}}));
    EXPECT_TRUE(collides(world,registry,{{0.2,-0.1,0.2},{0.8,1.7,0.8}}));
    EXPECT_FALSE(collides(world,registry,{{0.2,32,0.2},{0.8,33.8,0.8}}));
    EXPECT_THROW(collides(world,registry,{{0,0,0},{0,1,1}}),std::invalid_argument);
    EXPECT_THROW(moveAndCollide(world,registry,{{0,1,0},{0.6,2.8,0.6}},{100,0,0}),std::invalid_argument);
}
TEST(Player, GravityJumpAndFrameRateIndependentWalking) {
    auto world = emptyWorld(); const auto registry = vc::builtinBlocks();
    for (int x=-8;x<=8;++x) for (int z=-8;z<=8;++z) world.setBlock({x,0,z},1);
    vc::PlayerController a,b;
    a.camera.position=b.camera.position={0.5,3.62,0.5}; a.camera.yaw=b.camera.yaw=0;
    for(int i=0;i<120;++i) a.update({},1.0/120,world,registry);
    EXPECT_TRUE(a.grounded); EXPECT_NEAR(a.bounds().min.y,1,1e-6);
    b=a;
    for(int i=0;i<60;++i) a.update({{1,0,0,0,0},false,false},1.0/60,world,registry);
    for(int i=0;i<30;++i) b.update({{1,0,0,0,0},false,false},1.0/30,world,registry);
    EXPECT_NEAR(glm::length(a.camera.position-b.camera.position),0,1e-10);
    EXPECT_NEAR(a.camera.position.z,-4.5,1e-9);
    a.update({{},true,false},1.0/120,world,registry);
    EXPECT_FALSE(a.grounded); EXPECT_GT(a.bounds().min.y,1.05);
    double highest=a.bounds().min.y;
    for(int i=0;i<120;++i) { a.update({},1.0/120,world,registry); highest=std::max(highest,a.bounds().min.y); }
    EXPECT_GT(highest,2.2); EXPECT_TRUE(a.grounded); EXPECT_NEAR(a.bounds().min.y,1,1e-6);
}
TEST(Player, CannotLeaveFlightInsideTerrain) {
    auto world=emptyWorld();const auto registry=vc::builtinBlocks();
    vc::PlayerController player; player.flying=true;player.camera.position={0.5,2.62,0.5};
    world.setBlock({0,1,0},1);
    player.update({{},false,true},0,world,registry); EXPECT_TRUE(player.flying);
    world.setBlock({0,1,0},air);
    player.update({{},false,true},0,world,registry); EXPECT_FALSE(player.flying);
}

TEST(Player, JumpJustBeforeLandingIsBufferedAndDoesNotRepeat) {
    auto world=emptyWorld();const auto registry=vc::builtinBlocks();
    for(int x=-2;x<=2;++x) for(int z=-2;z<=2;++z) world.setBlock({x,0,z},1);
    vc::PlayerController player; player.camera.position={0.5,2.67,0.5};
    player.update({{},true,false},1.0/120,world,registry);
    for(int i=0;i<15;++i) player.update({},1.0/120,world,registry);
    EXPECT_GT(player.bounds().min.y,1.15); EXPECT_FALSE(player.grounded);
    for(int i=0;i<160;++i) player.update({},1.0/120,world,registry);
    EXPECT_TRUE(player.grounded); EXPECT_NEAR(player.bounds().min.y,1,1e-6);
    for(int i=0;i<30;++i) player.update({},1.0/120,world,registry);
    EXPECT_NEAR(player.bounds().min.y,1,1e-6);
}
TEST(Player, ReleaseCaptureCancelsBufferedJump) {
    auto world=emptyWorld();const auto registry=vc::builtinBlocks();world.setBlock({0,0,0},1);
    vc::PlayerController player;player.camera.position={0.5,2.67,0.5};
    player.update({{},true,false},1.0/120,world,registry);
    for(int i=0;i<30;++i) player.update({{},false,false,false},1.0/120,world,registry);
    EXPECT_TRUE(player.grounded);EXPECT_NEAR(player.bounds().min.y,1,1e-6);
}
TEST(Player, WalkOffLedgeThenJumpCannotJumpInAir) {
    auto world=emptyWorld();const auto registry=vc::builtinBlocks();world.setBlock({0,0,0},1);
    vc::PlayerController player;player.camera.position={0.5,2.62,0.5};player.camera.yaw=0;
    for(int i=0;i<30;++i) player.update({{0,1,0,0,0},false,false},1.0/120,world,registry);
    EXPECT_FALSE(player.grounded);
    const double before=player.camera.position.y;
    player.update({{},true,false},1.0/120,world,registry);
    EXPECT_LT(player.camera.position.y,before);
}
