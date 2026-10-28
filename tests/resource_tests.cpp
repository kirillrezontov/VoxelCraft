#include "engine/voxel/generation.hpp"
#include "engine/voxel/storage.hpp"
#include "game/content/blocks.hpp"
#include <gtest/gtest.h>

TEST(Resources, VersionTwoTreesOresAndChunkSamplesAgree) {
    using namespace vc::voxel;
    const auto registry=vc::builtinBlocks();const TerrainGenerator legacy(42,{},registry,1),generator(42,{},registry,2);
    unsigned logs=0,leaves=0,coal=0,iron=0;
    for(const Position p:{Position{-1,0,-1},{0,0,0},{-1,1,0},{0,-1,-1}}) {
        const auto chunk=generator.generate(p);const auto origin=chunkOrigin(p);
        for(int y=0;y<32;++y) for(int z=0;z<32;++z) for(int x=0;x<32;++x) {
            const Position cell{origin.x+x,origin.y+y,origin.z+z};const auto block=chunk.get({x,y,z});
            ASSERT_EQ(block,generator.sample(cell));
            if(block==registry.require("voxelcraft:log")) ++logs;
            if(block==registry.require("voxelcraft:leaves")) ++leaves;
            if(block==registry.require("voxelcraft:coal_ore")) ++coal;
            if(block==registry.require("voxelcraft:iron_ore")) ++iron;
            EXPECT_EQ(generator.surfaceY(cell.x,cell.z),legacy.surfaceY(cell.x,cell.z));
        }
    }
    EXPECT_GT(logs,0u);EXPECT_GT(leaves,0u);EXPECT_GT(coal,0u);EXPECT_GT(iron,0u);
    EXPECT_THROW(TerrainGenerator(42,{},registry,4),std::invalid_argument);
}
TEST(Resources, PartialHeightLeavesNoTruncatedTreesOrOutOfBoundsBlocks) {
    using namespace vc::voxel;const auto registry=vc::builtinBlocks();const TerrainGenerator generator(42,{100,100},registry,2);
    const auto chunk=generator.generate({-1,3,0});
    for(int z=0;z<32;++z) for(int x=0;x<32;++x) {
        EXPECT_EQ(chunk.get({x,4,z}),registry.require("voxelcraft:grass"));
        EXPECT_EQ(chunk.get({x,3,z}),air);EXPECT_EQ(chunk.get({x,5,z}),air);
    }
}
