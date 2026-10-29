#include "engine/renderer/texture.hpp"
#include "engine/voxel/mesher.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include <gtest/gtest.h>
#include <array>
#include <limits>

TEST(Atlas, LayoutAndUvValidation) {
    const vc::AtlasLayout layout(16, 2, 2, 1);
    EXPECT_EQ(layout.width(), 36u); EXPECT_EQ(layout.height(), 36u);
    EXPECT_EQ(layout.tileOrigin(3), glm::uvec2(19, 19));
    const auto start = layout.uv(3, {0, 0}), end = layout.uv(3, {1, 1});
    EXPECT_NEAR(start.x, 19.0f / 36, 1e-6); EXPECT_NEAR(start.y, 19.0f / 36, 1e-6);
    EXPECT_NEAR(end.x, 35.0f / 36, 1e-6); EXPECT_NEAR(end.y, 35.0f / 36, 1e-6);
    EXPECT_THROW(layout.uv(4, {0, 0}), std::out_of_range);
    EXPECT_THROW(layout.uv(0, {-0.01f, 1}), std::invalid_argument);
    EXPECT_THROW(layout.uv(0, {0, std::numeric_limits<float>::quiet_NaN()}), std::invalid_argument);
    EXPECT_THROW((vc::AtlasLayout(0, 1, 1)), std::invalid_argument);
    EXPECT_THROW((vc::AtlasLayout(16, 0, 1)), std::invalid_argument);
    EXPECT_THROW((vc::AtlasLayout(16, UINT32_MAX, UINT32_MAX)), std::invalid_argument);
}

TEST(Atlas, ImageValidation) {
    EXPECT_THROW((vc::ImageRgba{}.validate()), std::invalid_argument);
    EXPECT_THROW((vc::ImageRgba{2, 2, {0, 0, 0, 0}}.validate()), std::invalid_argument);
    EXPECT_THROW((vc::ImageRgba{UINT32_MAX, UINT32_MAX, {}}.validate()), std::invalid_argument);
    EXPECT_NO_THROW((vc::ImageRgba{1, 1, {255, 255, 255, 255}}.validate()));
}

TEST(Atlas, ContentAndEdgeGutters) {
    const auto image = vc::builtinAtlas(); const auto layout = vc::builtinAtlasLayout();
    EXPECT_EQ(image.pixels, vc::builtinAtlas().pixels);
    const auto pixel = [&](std::uint32_t x, std::uint32_t y) {
        const auto i = (static_cast<std::size_t>(y) * image.width + x) * 4;
        return std::array{image.pixels.at(i), image.pixels.at(i+1), image.pixels.at(i+2), image.pixels.at(i+3)};
    };
    for (vc::TextureTile tile = 0; tile < layout.tileCount(); ++tile) {
        const auto p = layout.tileOrigin(tile); const auto last = layout.tileSize() - 1;
        for (std::uint32_t i = 0; i < layout.tileSize(); ++i) {
            EXPECT_EQ(pixel(p.x-1, p.y+i), pixel(p.x, p.y+i));
            EXPECT_EQ(pixel(p.x+last+1, p.y+i), pixel(p.x+last, p.y+i));
            EXPECT_EQ(pixel(p.x+i, p.y-1), pixel(p.x+i, p.y));
            EXPECT_EQ(pixel(p.x+i, p.y+last+1), pixel(p.x+i, p.y+last));
        }
        EXPECT_EQ(pixel(p.x-1, p.y-1), pixel(p.x, p.y));
        EXPECT_EQ(pixel(p.x+last+1, p.y+last+1), pixel(p.x+last, p.y+last));
    }
    const auto white = layout.tileOrigin(vc::tiles::white);
    EXPECT_EQ(pixel(white.x, white.y), (std::array<std::uint8_t,4>{255,255,255,255}));
    const auto side = layout.tileOrigin(vc::tiles::grassSide);
    const auto top = pixel(side.x, side.y), bottom = pixel(side.x, side.y+15);
    EXPECT_GT(top[1], top[0]); EXPECT_GT(bottom[0], bottom[1]);
    for(std::size_t i=3;i<image.pixels.size();i+=4) { if(((i/4)%image.width)<17*layout.stride()) { EXPECT_EQ(image.pixels[i],255); } }
}

TEST(Atlas, CubeFaceMaterialsAndGrassOrientation) {
    const auto registry = vc::builtinBlocks(); const auto layout = vc::builtinAtlasLayout();
    vc::voxel::Chunk chunk({0,0,0}); chunk.set({0,0,0}, registry.require("voxelcraft:grass"));
    const auto mesh = vc::voxel::meshChunk(chunk, vc::voxel::World{}, registry, layout);
    ASSERT_EQ(mesh.vertices.size(), 24u);
    const auto& block = registry.definition(registry.require("voxelcraft:grass"));
    EXPECT_EQ(block.faceTextures[2], vc::tiles::grassTop);
    EXPECT_EQ(block.faceTextures[3], vc::tiles::dirt);
    for (std::size_t face = 0; face < 6; ++face) {
        const auto low = layout.uv(block.faceTextures[face], {0,0}), high = layout.uv(block.faceTextures[face], {1,1});
        for (std::size_t corner = 0; corner < 4; ++corner) {
            const auto& vertex = mesh.vertices[face*4+corner];
            EXPECT_GE(vertex.uv.x, low.x); EXPECT_LE(vertex.uv.x, high.x);
            EXPECT_GE(vertex.uv.y, low.y); EXPECT_LE(vertex.uv.y, high.y);
            if (face == 0 || face == 1 || face == 4 || face == 5) {
                EXPECT_FLOAT_EQ(vertex.uv.y, vertex.position.y == 1 ? low.y : high.y);
            }
        }
    }
    auto invalid = vc::builtinBlocks();
    const auto bad = invalid.add({"test:invalid", true, {1,1,1}, {100,100,100,100,100,100}});
    chunk.set({0,0,0}, bad);
    EXPECT_THROW(vc::voxel::meshChunk(chunk, vc::voxel::World{}, invalid, layout), std::out_of_range);
}
