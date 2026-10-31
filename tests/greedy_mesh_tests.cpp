#include "engine/voxel/mesher.hpp"
#include "engine/voxel/lighting.hpp"
#include "game/content/blocks.hpp"
#include "game/content/atlas.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <limits>

namespace {
using Surface=std::map<std::array<int,8>,std::array<float,6>>;
Surface surfaces(const vc::Mesh& mesh,const vc::AtlasLayout& atlas) {
    Surface result;
    for(std::size_t base=0;base<mesh.vertices.size();base+=4) {
        const auto& first=mesh.vertices.at(base);auto low=first.position,high=low;auto uv=first.uv;
        for(std::size_t corner=0;corner<4;++corner) {
            const auto& v=mesh.vertices.at(base+corner);low=glm::min(low,v.position);high=glm::max(high,v.position);uv=glm::min(uv,v.uv);
            EXPECT_EQ(v.light,first.light);EXPECT_EQ(v.normal,first.normal);EXPECT_EQ(v.tileBounds,first.tileBounds);
        }
        if(first.tileBounds.z>0) uv=glm::vec2(first.tileBounds);
        const auto delta=high-low;
        EXPECT_GT(glm::dot(glm::cross(mesh.vertices.at(base+1).position-first.position,
            mesh.vertices.at(base+2).position-first.position),first.normal),0);
        for(int x=0;x<std::max(1,static_cast<int>(delta.x));++x)
            for(int y=0;y<std::max(1,static_cast<int>(delta.y));++y)
                for(int z=0;z<std::max(1,static_cast<int>(delta.z));++z) {
                    const std::array key{static_cast<int>(first.normal.x),static_cast<int>(first.normal.y),static_cast<int>(first.normal.z),
                        static_cast<int>(low.x)+x,static_cast<int>(low.y)+y,static_cast<int>(low.z)+z,
                        static_cast<int>(std::lround(uv.x*static_cast<float>(atlas.width()))),static_cast<int>(std::lround(uv.y*static_cast<float>(atlas.height())))};
                    const std::array value{first.color.x,first.color.y,first.color.z,first.light.x,first.light.y,first.opacity};
                    EXPECT_TRUE(result.emplace(key,value).second)<<"Overlapping faces";
                }
    }
    return result;
}
}
TEST(GreedyMeshing, SolidChunkBecomesSixCorrectlyWoundTiledQuads) {
    const auto blocks=vc::builtinBlocks();const auto atlas=vc::builtinAtlasLayout();vc::voxel::World world;
    world.insert(vc::voxel::Chunk({-1,0,0},blocks.require("voxelcraft:grass")));
    const auto& chunk=*world.findChunk({-1,0,0});const auto greedy=vc::voxel::meshChunk(chunk,world,blocks,atlas);
    const auto reference=vc::voxel::meshChunk(chunk,world,blocks,atlas,nullptr,false);
    EXPECT_EQ(greedy.vertices.size(),24u);EXPECT_EQ(greedy.indices.size(),36u);EXPECT_EQ(greedy.origin,reference.origin);
    EXPECT_EQ(surfaces(greedy,atlas),surfaces(reference,atlas));
    constexpr std::array<glm::vec2,4> expected{{{0,32},{0,0},{32,0},{32,32}}};
    for(std::size_t side=0;side<6;++side) for(std::size_t corner=0;corner<4;++corner) {
        const auto& v=greedy.vertices.at(side*4+corner);EXPECT_EQ(v.uv,expected[corner]);
        const auto low=atlas.uv(blocks.definition(chunk.get({0,0,0})).faceTextures[side],{0,0});
        EXPECT_EQ(glm::vec2(v.tileBounds),low);
        EXPECT_EQ(glm::vec2(v.tileBounds.z,v.tileBounds.w),atlas.uv(blocks.definition(chunk.get({0,0,0})).faceTextures[side],{1,1})-low);
    }
}
TEST(GreedyMeshing, MaterialsLightingTransparentFacesAndNegativeSeamsMatchReference) {
    const auto blocks=vc::builtinBlocks();const auto atlas=vc::builtinAtlasLayout();vc::voxel::World world({-7,19});
    const std::array ids{blocks.require("voxelcraft:stone"),blocks.require("voxelcraft:grass"),blocks.require("voxelcraft:water"),blocks.require("voxelcraft:leaves"),blocks.require("voxelcraft:lamp")};
    for(const vc::voxel::Position p:{vc::voxel::Position{-1,-1,0},{0,-1,0},{-1,0,0},{0,0,0}}) {
        vc::voxel::Chunk chunk(p);
        for(int y=0;y<32;++y) for(int x=0;x<32;++x) for(int z=0;z<32;++z)
            if(y<3||(x>27&&z<5&&y<12)) chunk.set({x,y,z},ids[static_cast<std::size_t>((x/4+z/4+y/3)%5)]);
        world.insert(std::move(chunk));
    }
    vc::voxel::Lighting light;const auto dirty=world.takeDirtyChunks();(void)light.refresh(world,blocks,dirty);
    for(const auto& [p,chunk]:world.chunks()) {
        (void)p;const auto greedy=vc::voxel::meshChunk(chunk,world,blocks,atlas,&light),reference=vc::voxel::meshChunk(chunk,world,blocks,atlas,&light,false);
        EXPECT_EQ(greedy.translucentIndices.size(),reference.translucentIndices.size());
        EXPECT_EQ(surfaces(greedy,atlas),surfaces(reference,atlas));
        EXPECT_LE(greedy.vertices.size(),reference.vertices.size());
    }
    // A boundary edit must produce the same new visible unit surfaces.
    ASSERT_TRUE(world.setBlock({-1,1,0},vc::voxel::air));
    const auto& changed=*world.findChunk({-1,0,0});
    EXPECT_EQ(surfaces(vc::voxel::meshChunk(changed,world,blocks,atlas),atlas),surfaces(vc::voxel::meshChunk(changed,world,blocks,atlas,nullptr,false),atlas));
}

TEST(GreedyMeshing, ExtremeOriginsAndPartialHeightStayRepresentable) {
    using namespace vc::voxel;
    const auto blocks=vc::builtinBlocks();const auto atlas=vc::builtinAtlasLayout();World world({5,5});
    Chunk chunk({chunkAxis(std::numeric_limits<std::int64_t>::max()),0,chunkAxis(std::numeric_limits<std::int64_t>::min())},blocks.require("voxelcraft:stone"));
    const auto greedy=meshChunk(chunk,world,blocks,atlas),reference=meshChunk(chunk,world,blocks,atlas,nullptr,false);
    EXPECT_EQ(greedy.origin,reference.origin);EXPECT_EQ(greedy.vertices.size(),24u);
    EXPECT_EQ(surfaces(greedy,atlas),surfaces(reference,atlas));
}
