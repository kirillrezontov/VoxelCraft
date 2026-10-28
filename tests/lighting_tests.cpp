#include "engine/voxel/lighting.hpp"
#include "engine/voxel/mesher.hpp"
#include "engine/voxel/raycast.hpp"
#include "game/content/blocks.hpp"
#include "game/content/atlas.hpp"
#include "game/player/controller.hpp"
#include "game/gameplay/interaction.hpp"
#include <gtest/gtest.h>

TEST(Lighting, SkyRoofRemovalAndCaveMeshExposure) {
    using namespace vc::voxel;
    const auto registry=vc::builtinBlocks();World world;Chunk chunk({0,0,0});const auto stone=registry.require("voxelcraft:stone");
    for(int x=0;x<32;++x) for(int z=0;z<32;++z) { chunk.set({x,0,z},stone);chunk.set({x,8,z},stone); }
    world.insert(std::move(chunk));Lighting lighting;
    const auto dirty=world.takeDirtyChunks();EXPECT_FALSE(lighting.refresh(world,registry,dirty).empty());
    EXPECT_EQ(lighting.sky({5,7,5}),0);EXPECT_EQ(lighting.sky({5,9,5}),1);
    const auto mesh=meshChunk(*world.findChunk({0,0,0}),world,registry,vc::builtinAtlasLayout(),&lighting);
    bool caveDark=false,surfaceBright=false;
    for(const auto& vertex:mesh.vertices) {
        if(vertex.position.y==8&&vertex.normal.y==-1&&vertex.light.x==0) caveDark=true;
        if(vertex.position.y==9&&vertex.normal.y==1&&vertex.light.x==1) surfaceBright=true;
    }
    EXPECT_TRUE(caveDark);EXPECT_TRUE(surfaceBright);
    world.setBlock({5,8,5},air);const auto edits=world.takeDirtyChunks();lighting.refresh(world,registry,edits);
    EXPECT_EQ(lighting.sky({5,7,5}),1);EXPECT_EQ(lighting.sky({6,7,5}),0);
}
TEST(Lighting, LampPropagationRespectsWallsNegativeSeamsAndRemoval) {
    using namespace vc::voxel;
    const auto registry=vc::builtinBlocks();World world;
    for(int x=-1;x<=0;++x) world.insert(Chunk({x,0,0}));
    const auto stone=registry.require("voxelcraft:stone"),lamp=registry.require("voxelcraft:lamp");
    for(int y=0;y<32;++y) for(int z=0;z<32;++z) world.setBlock({0,y,z},stone);
    world.setBlock({-1,10,10},lamp);Lighting lighting;auto dirty=world.takeDirtyChunks();lighting.refresh(world,registry,dirty);
    EXPECT_EQ(lighting.block({-1,10,10}),1);EXPECT_NEAR(lighting.block({-2,10,10}),14.0/15,1e-6);EXPECT_EQ(lighting.block({1,10,10}),0);
    world.setBlock({0,10,10},air);dirty=world.takeDirtyChunks();EXPECT_FALSE(lighting.refresh(world,registry,dirty).empty());
    EXPECT_NEAR(lighting.block({1,10,10}),13.0/15,1e-6);
    world.setBlock({-1,10,10},air);dirty=world.takeDirtyChunks();lighting.refresh(world,registry,dirty);EXPECT_EQ(lighting.block({1,10,10}),0);
}
TEST(Water, TransparentMeshHidesInternalFacesAndSupportsOpaqueReplacement) {
    using namespace vc::voxel;
    const auto registry=vc::builtinBlocks();const auto water=registry.require("voxelcraft:water"),stone=registry.require("voxelcraft:stone");
    World world;Chunk chunk({0,0,0});chunk.set({1,1,1},water);chunk.set({2,1,1},water);world.insert(std::move(chunk));
    const auto mesh=meshChunk(*world.findChunk({0,0,0}),world,registry,vc::builtinAtlasLayout());
    EXPECT_TRUE(mesh.indices.empty());EXPECT_EQ(mesh.translucentIndices.size(),60u);
    for(const auto& vertex:mesh.vertices) EXPECT_FLOAT_EQ(vertex.opacity,0.65f);
    world.setBlock({1,0,1},stone);
    const auto picked=raycast(world,{1.5,3.5,1.5},{0,-1,0},6,&registry);ASSERT_TRUE(picked);EXPECT_EQ(picked->block,(Position{1,0,1}));
    const auto fluidPicked=raycast(world,{1.5,3.5,1.5},{0,-1,0});ASSERT_TRUE(fluidPicked);EXPECT_EQ(fluidPicked->block,(Position{1,1,1}));
    EXPECT_TRUE(vc::placeBlock(world,registry,*picked,stone,{{10,10,10},{11,12,11}}));EXPECT_EQ(world.block({1,1,1}),stone);
}
TEST(Water, SwimmingMovesSlowerAscendsAndCushionsFalls) {
    using namespace vc::voxel;
    const auto registry=vc::builtinBlocks();World world;Chunk chunk({0,0,0});
    for(int x=0;x<32;++x) for(int z=0;z<32;++z) {
        chunk.set({x,0,z},registry.require("voxelcraft:stone"));
        for(int y=1;y<=4;++y) chunk.set({x,y,z},registry.require("voxelcraft:water"));
    }
    world.insert(std::move(chunk));vc::PlayerController player;player.camera.position={10.5,3.62,10.5};player.camera.yaw=0;
    player.update({{1,0,0,0,0}},0.1,world,registry);EXPECT_TRUE(player.swimming);EXPECT_NEAR(player.camera.position.z,10.25,1e-8);
    for(int frame=0;frame<60;++frame) player.update({{0,0,1,0,0}},1.0/60,world,registry);
    EXPECT_GT(player.bounds().min.y,4.5);
    player=vc::PlayerController{};player.camera.position={10.5,30,10.5};double worst=0;
    for(int frame=0;frame<240;++frame) { player.update({},1.0/60,world,registry);worst=std::max(worst,player.landingSpeed); }
    EXPECT_LT(worst,4);EXPECT_TRUE(player.swimming);
}
