#include "game/content/materials.hpp"
#include "game/content/atlas.hpp"
#include "game/content/png.hpp"
#include "tests/temp_directory.hpp"
#include <gtest/gtest.h>
#include <array>
#include <fstream>

namespace {
vc::ImageRgba tileImage() {
    vc::ImageRgba image{16,16,std::vector<std::uint8_t>(16*16*4)};
    for(std::size_t y=0;y<16;++y) for(std::size_t x=0;x<16;++x) {
        const auto offset=(y*16+x)*4;image.pixels[offset]=static_cast<std::uint8_t>(x*16);
        image.pixels[offset+1]=static_cast<std::uint8_t>(y*16);image.pixels[offset+2]=123;image.pixels[offset+3]=255;
    }
    return image;
}
std::array<std::uint8_t,4> pixel(const vc::ImageRgba& image,std::uint32_t x,std::uint32_t y) {
    const auto offset=(static_cast<std::size_t>(y)*image.width+x)*4;
    return {image.pixels.at(offset),image.pixels.at(offset+1),image.pixels.at(offset+2),image.pixels.at(offset+3)};
}
}
TEST(ContentAtlas, BuiltinPixelsAndAliasesRemainExact) {
    const vc::ContentAtlas atlas;EXPECT_EQ(atlas.image().pixels,vc::builtinAtlas().pixels);
    EXPECT_EQ(atlas.layout().width(),vc::builtinAtlasLayout().width());EXPECT_EQ(atlas.size(),23u);
    EXPECT_EQ(atlas.require("stone"),vc::tiles::stone);EXPECT_EQ(atlas.require("voxelcraft:stone"),vc::tiles::stone);
    EXPECT_FALSE(atlas.find("missing:texture"));EXPECT_THROW(atlas.require("missing:texture"),std::invalid_argument);
}
TEST(ContentAtlas, AppendedTilesRepackAcrossRowsAndPreserveGutters) {
    vc::ContentAtlas atlas;const auto source=tileImage();
    for(int i=0;i<18;++i) atlas.add("test:tile_"+std::to_string(i),source);
    const auto layout=atlas.layout();const auto image=atlas.image();image.validate();
    EXPECT_EQ(atlas.size(),41u);EXPECT_EQ(layout.width(),32*18u);EXPECT_EQ(layout.height(),2*18u);
    const auto origin=layout.tileOrigin(atlas.require("test:tile_17"));
    for(std::uint32_t x=0;x<16;++x) for(std::uint32_t y=0;y<16;++y)
        EXPECT_EQ(pixel(image,origin.x+x,origin.y+y),pixel(source,x,y));
    for(std::uint32_t i=0;i<16;++i) {
        EXPECT_EQ(pixel(image,origin.x-1,origin.y+i),pixel(source,0,i));
        EXPECT_EQ(pixel(image,origin.x+16,origin.y+i),pixel(source,15,i));
        EXPECT_EQ(pixel(image,origin.x+i,origin.y-1),pixel(source,i,0));
        EXPECT_EQ(pixel(image,origin.x+i,origin.y+16),pixel(source,i,15));
    }
    EXPECT_EQ(pixel(image,origin.x-1,origin.y-1),pixel(source,0,0));
    EXPECT_EQ(pixel(image,origin.x+16,origin.y+16),pixel(source,15,15));
    const auto builtins=vc::builtinAtlas();const auto base=vc::builtinAtlasLayout();
    const auto stone=layout.tileOrigin(vc::tiles::stone),original=base.tileOrigin(vc::tiles::stone);
    EXPECT_EQ(pixel(image,stone.x,stone.y),pixel(builtins,original.x,original.y));
}
TEST(ContentAtlas, InvalidDuplicateAndOverflowDefinitionsDoNotMutateAtlas) {
    vc::ContentAtlas atlas;const auto image=tileImage();
    EXPECT_THROW(atlas.add("missing_namespace",image),std::invalid_argument);
    EXPECT_THROW(atlas.add("voxelcraft:stone",image),std::invalid_argument);
    EXPECT_THROW(atlas.add("test:bad",{1,1,{1,2,3,4}}),std::invalid_argument);EXPECT_EQ(atlas.size(),23u);
    for(std::size_t i=atlas.size();i<vc::ContentAtlas::maxTiles;++i) atlas.add("test:tile_"+std::to_string(i),image);
    EXPECT_THROW(atlas.add("test:overflow",image),std::runtime_error);EXPECT_EQ(atlas.size(),vc::ContentAtlas::maxTiles);
    EXPECT_FALSE(atlas.find("test:overflow"));
}
TEST(PngTiles, DecodeAndBoundsRejectMalformedOrExcessiveImagesWithoutLeakingBudget) {
    const std::filesystem::path data=VC_TEST_DATA_DIR;const auto decoded=vc::loadPngTile(data/"tile.png");
    EXPECT_EQ(decoded.width,16u);EXPECT_EQ(decoded.height,16u);decoded.validate();
    EXPECT_EQ(pixel(decoded,0,0),(std::array<std::uint8_t,4>{26,40,49,255}));
    EXPECT_NE(pixel(decoded,5,5),pixel(decoded,0,0));
    EXPECT_THROW(vc::loadPngTile(data/"oversized.png"),std::runtime_error);
    for(int attempt=0;attempt<3;++attempt) {
        EXPECT_THROW(vc::loadPngTile(data/"inflate-bomb.png"),std::runtime_error);
        EXPECT_EQ(vc::loadPngTile(data/"tile.png").pixels,decoded.pixels);
    }
    vc::test::TempDirectory temp;
    { std::ofstream output(temp.path()/"invalid.png",std::ios::binary);output<<"this is not PNG"; }
    EXPECT_THROW(vc::loadPngTile(temp.path()/"invalid.png"),std::runtime_error);
    { std::ofstream output(temp.path()/"invalid.png",std::ios::binary);output<<std::string(131073,'x'); }
    EXPECT_THROW(vc::loadPngTile(temp.path()/"invalid.png"),std::runtime_error);
}
