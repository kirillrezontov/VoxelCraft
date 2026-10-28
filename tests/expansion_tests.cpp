#include "engine/voxel/generation.hpp"
#include "engine/voxel/streaming.hpp"
#include "game/content/blocks.hpp"
#include "game/simulation/day_cycle.hpp"
#include "game/player/save_state.hpp"
#include <gtest/gtest.h>
#include <limits>
#include <algorithm>

TEST(TerrainExpansion, FrozenVersionFixturesPreserveLegacyAndExpandedContent) {
    using namespace vc::voxel;
    const auto blocks=vc::builtinBlocks();const TerrainGenerator one(42,{},blocks,1),two(42,{},blocks,2),three(42,{},blocks,3);
    for(const auto entry:{Position{0,-2,0},{-1,-2,-1},{-32,15,31},{1000,17,-1000}}) {
        EXPECT_EQ(one.surfaceY(entry.x,entry.z),entry.y);EXPECT_EQ(two.surfaceY(entry.x,entry.z),entry.y);
    }
    EXPECT_EQ(three.biomeAt(0,0),Biome::Plains);EXPECT_EQ(three.biomeAt(32,-32),Biome::Forest);
    EXPECT_EQ(three.biomeAt(-96,-96),Biome::Desert);EXPECT_EQ(three.biomeAt(-96,64),Biome::Mountains);
    EXPECT_EQ(three.surfaceY(0,0),-2);EXPECT_EQ(three.sample({0,3,0}),blocks.require("voxelcraft:water"));
    EXPECT_EQ(three.sample({-96,16,-96}),blocks.require("voxelcraft:sand"));
    EXPECT_EQ(three.sample({-160,74,-32}),blocks.require("voxelcraft:snow"));
    EXPECT_EQ(three.sample({19,21,212}),blocks.require("voxelcraft:lamp")); // Ruin reward, global coordinates.
    EXPECT_EQ(three.sample({-12,-64,-5}),air);EXPECT_EQ(one.sample({-12,-64,-5}),blocks.require("voxelcraft:stone"));
}
TEST(TerrainExpansion, ChunkSeamsPartialBoundsAndBottomLayers) {
    using namespace vc::voxel;
    const auto blocks=vc::builtinBlocks();const TerrainGenerator generator(42,{},blocks,3),partial(42,{-5,17},blocks,3);
    for(const auto p:{Position{-1,0,-1},{0,-2,-1},{0,0,6}}) {
        const auto chunk=generator.generate(p);const auto origin=chunkOrigin(p);
        for(int y=0;y<32;++y) for(int z=0;z<32;++z) for(int x=0;x<32;++x)
            ASSERT_EQ(chunk.get({x,y,z}),generator.sample({origin.x+x,origin.y+y,origin.z+z}));
    }
    const auto boundary=partial.generate({-1,-1,0});
    for(int y=0;y<32;++y) for(int z=0;z<32;z+=3) for(int x=0;x<32;x+=3) {
        EXPECT_EQ(boundary.get({x,y,z}),partial.sample({-32+x,-32+y,z}));
        if(y<27) { EXPECT_EQ(boundary.get({x,y,z}),air); }
    }
    for(int x=-16;x<16;++x) for(int z=-16;z<16;++z) {
        EXPECT_TRUE(blocks.definition(generator.sample({x,-512,z})).solid);
        EXPECT_TRUE(blocks.definition(generator.sample({x,-511,z})).solid);
    }
    const auto region=streamingRegion({0,20,0},{},1,3);EXPECT_EQ(region.size(),54u);
    EXPECT_NE(std::find(region.begin(),region.end(),Position{0,4,0}),region.end());
}
TEST(DayCycle, NoonNightWrapAndSaveTime) {
    vc::DayCycle day;EXPECT_EQ(day.clock(),"12:00");const auto noon=day.daylight();
    day.advance(600);EXPECT_EQ(day.clock(),"00:00");const auto night=day.daylight();
    EXPECT_GT(noon.sunlight,0.99f);EXPECT_EQ(night.sunlight,0);EXPECT_GT(noon.ambient,night.ambient);
    EXPECT_GT(noon.skyColor.b,night.skyColor.b*10);EXPECT_NEAR(glm::length(night.sunDirection),1,1e-6);
    day.advance(600);EXPECT_EQ(day.clock(),"12:00");EXPECT_DOUBLE_EQ(day.seconds(),300);
    vc::PlayerSaveState state;state.worldTime=900;EXPECT_EQ(vc::decodePlayer(vc::encodePlayer(state)).worldTime,900);
    EXPECT_THROW(day.advance(std::numeric_limits<double>::infinity()),std::invalid_argument);
    EXPECT_THROW(vc::DayCycle(-1),std::invalid_argument);
}
