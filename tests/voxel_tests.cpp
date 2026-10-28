#include "engine/voxel/world.hpp"
#include "engine/voxel/generation.hpp"
#include "engine/voxel/mesher.hpp"
#include "game/content/blocks.hpp"
#include "game/content/atlas.hpp"
#include "game/player/camera.hpp"
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <set>

using namespace vc::voxel;

TEST(Coordinates, NegativeBoundaries) {
    for (const auto value : {-65, -64, -33, -32, -31, -1, 0, 1, 31, 32, 33, 64}) {
        const auto expected = static_cast<std::int64_t>(std::floor(static_cast<double>(value) / 32));
        const auto chunk = chunkPosition({value, value, value});
        const auto local = localPosition({value, value, value});
        EXPECT_EQ(chunk, (Position{expected, expected, expected}));
        EXPECT_GE(local.x, 0); EXPECT_LT(local.x, chunkSize);
        const auto origin = chunkOrigin(chunk);
        EXPECT_EQ(origin.x + local.x, value); EXPECT_EQ(origin.y + local.y, value); EXPECT_EQ(origin.z + local.z, value);
    }
}
TEST(Coordinates, FullIntegerRange) {
    for (const auto value : {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}) {
        const auto origin = originAxis(chunkAxis(value));
        EXPECT_EQ(origin + localAxis(value), value);
    }
    EXPECT_THROW(originAxis(std::numeric_limits<std::int64_t>::max()), std::out_of_range);
    EXPECT_THROW(originAxis(std::numeric_limits<std::int64_t>::min()), std::out_of_range);
}
TEST(Chunk, IndexingAndBounds) {
    Chunk chunk({-1, 0, 1});
    std::set<std::size_t> indices;
    for (int y = 0; y < chunkSize; ++y) for (int z = 0; z < chunkSize; ++z) for (int x = 0; x < chunkSize; ++x) {
        const LocalPosition p{x, y, z}; indices.insert(blockIndex(p));
        EXPECT_EQ(chunk.get(p), air); chunk.set(p, static_cast<BlockId>(blockIndex(p)));
    }
    EXPECT_EQ(indices.size(), chunkVolume);
    for (int y = 0; y < chunkSize; ++y) for (int z = 0; z < chunkSize; ++z) for (int x = 0; x < chunkSize; ++x)
        EXPECT_EQ(chunk.get({x, y, z}), blockIndex({x, y, z}));
    EXPECT_THROW(chunk.get({-1, 0, 0}), std::out_of_range);
    EXPECT_THROW(chunk.set({0, 32, 0}, air), std::out_of_range);
    EXPECT_THROW(chunk.get({0, 0, 32}), std::out_of_range);
}
TEST(Registry, NamesAndUnknownIds) {
    auto registry = vc::builtinBlocks();
    EXPECT_EQ(registry.require("voxelcraft:air"), air);
    EXPECT_FALSE(registry.definition(air).opaque);
    EXPECT_TRUE(registry.definition(registry.require("voxelcraft:stone")).opaque);
    for (auto name : {"stone", ":stone", "vc:", "VC:stone", "vc:st one", "vc:a:b"}) EXPECT_FALSE(BlockRegistry::validIdentifier(name));
    EXPECT_TRUE(BlockRegistry::validIdentifier("example:blocks/stone_2"));
    EXPECT_THROW(registry.add({"voxelcraft:stone", true, {1, 1, 1}}), std::invalid_argument);
    EXPECT_THROW(registry.add({"invalid", true, {1, 1, 1}}), std::invalid_argument);
    EXPECT_FALSE(registry.find("missing:block").has_value());
    EXPECT_THROW(registry.require("missing:block"), std::invalid_argument);
    EXPECT_THROW(registry.definition(65535), std::out_of_range);
}
TEST(World, BoundsAndMissingChunks) {
    EXPECT_THROW((World({5, -5})), std::invalid_argument);
    World world({-5, 17});
    world.insert(Chunk({-1, 0, -1}, 1)); world.insert(Chunk({-1, -1, -1}, 1));
    EXPECT_EQ(world.block({-1, 0, -1}), 1);
    EXPECT_EQ(world.block({-32, -5, -32}), 1);
    EXPECT_EQ(world.block({-1, 17, -1}), 1);
    EXPECT_EQ(world.block({-1, 18, -1}), air);
    EXPECT_EQ(world.block({-1, -6, -1}), air);
    EXPECT_EQ(world.block({0, 0, 0}), air);
    EXPECT_THROW(world.insert(Chunk({0, 1, 0})), std::out_of_range);
}
TEST(Camera, MovementAndPitch) {
    vc::Camera a, b; a.position = b.position = {0, 0, 0}; a.yaw = b.yaw = 0; a.pitch = b.pitch = 0;
    a.update({1, 0, 0, 0, 0}, 0.1);
    for (int i = 0; i < 10; ++i) b.update({1, 0, 0, 0, 0}, 0.01);
    EXPECT_NEAR(glm::length(a.position - b.position), 0, 1e-10);
    EXPECT_NEAR(a.position.z, -2.4, 1e-10);
    a.position = {0, 0, 0}; a.update({1, 1, 1, 0, 0}, 0.1);
    EXPECT_NEAR(glm::length(a.position), 2.4, 1e-10);
    a.update({0, 0, 0, 0, 1e6f}, 0);
    EXPECT_LT(std::abs(a.pitch), glm::radians(90.0));
    EXPECT_NEAR(glm::length(a.forward()), 1.0, 1e-10);
    EXPECT_THROW(a.viewProjection(0), std::invalid_argument);
}

TEST(Camera, ProjectionAndCameraRelativeCoordinates) {
    vc::Camera camera; camera.yaw = camera.pitch = 0;
    const auto vp = camera.viewProjection(16.0f / 9.0f);
    const auto nearPoint = vp * glm::vec4(0, 0, -0.1f, 1);
    const auto farPoint = vp * glm::vec4(0, 0, -1800.0f, 1);
    EXPECT_NEAR(nearPoint.z / nearPoint.w, 0, 1e-6);
    EXPECT_NEAR(farPoint.z / farPoint.w, 1, 1e-6);
    EXPECT_LT((vp * glm::vec4(0, 1, -2, 1)).y, 0); // +Y appears above the viewport center.
    camera.position = {1e12, -1e12, 1e12};
    EXPECT_EQ(vp, camera.viewProjection(16.0f / 9.0f));
}

TEST(Terrain, DeterminismAndLayers) {
    const auto registry = vc::builtinBlocks();
    const TerrainGenerator a(12345, {}, registry), b(12345, {}, registry), other(54321, {}, registry);
    bool seedChangesTerrain = false;
    for (int z = -80; z < 80; z += 3) for (int x = -80; x < 80; x += 3) {
        const auto height = a.surfaceY(x, z);
        EXPECT_EQ(height, b.surfaceY(x, z));
        seedChangesTerrain |= height != other.surfaceY(x, z);
        EXPECT_EQ(a.sample({x, height, z}), registry.require("voxelcraft:grass"));
        EXPECT_EQ(a.sample({x, height - 1, z}), registry.require("voxelcraft:dirt"));
        EXPECT_EQ(a.sample({x, height - 3, z}), registry.require("voxelcraft:dirt"));
        EXPECT_EQ(a.sample({x, height - 4, z}), registry.require("voxelcraft:stone"));
        EXPECT_EQ(a.sample({x, height + 1, z}), air);
        EXPECT_LE(std::abs(height - a.surfaceY(x + 1, z)), 4);
        EXPECT_EQ(a.sample({x, -513, z}), air); EXPECT_EQ(a.sample({x, 512, z}), air);
    }
    EXPECT_TRUE(seedChangesTerrain);
}
TEST(Terrain, ChunkSeamsAndPartialHeightBounds) {
    const auto registry = vc::builtinBlocks();
    const TerrainGenerator generator(12345, {-5, 17}, registry);
    for (const Position p : {Position{-1, -1, -1}, Position{0, 0, 0}, Position{-1, 0, 0}, Position{0, 0, -1}}) {
        const auto chunk = generator.generate(p); const auto origin = chunkOrigin(p);
        for (int y = 0; y < 32; ++y) for (int z = 0; z < 32; ++z) for (int x = 0; x < 32; ++x)
            EXPECT_EQ(chunk.get({x, y, z}), generator.sample({origin.x + x, origin.y + y, origin.z + z}));
    }
    const TerrainGenerator high(0, {100, 100}, registry);
    EXPECT_EQ(high.surfaceY(0, 0), 100);
    EXPECT_EQ(high.sample({0, 100, 0}), registry.require("voxelcraft:grass"));
    EXPECT_EQ(high.sample({0, 99, 0}), air);
}
TEST(Mesher, EmptyAndSingleCubeWinding) {
    const auto registry = vc::builtinBlocks(); World world;
    Chunk chunk({-1, 0, 0});
    EXPECT_TRUE(meshChunk(chunk, world, registry, vc::builtinAtlasLayout()).indices.empty());
    chunk.set({0, 0, 0}, registry.require("voxelcraft:stone"));
    const auto mesh = meshChunk(chunk, world, registry, vc::builtinAtlasLayout());
    EXPECT_EQ(mesh.origin, glm::i64vec3(-32, 0, 0));
    ASSERT_EQ(mesh.vertices.size(), 24u); ASSERT_EQ(mesh.indices.size(), 36u);
    const std::array<glm::vec3, 6> normals{{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        for (std::size_t k = i; k < i + 3; ++k) ASSERT_LT(mesh.indices[k], mesh.vertices.size());
        const auto a = mesh.vertices[mesh.indices[i]].position, b = mesh.vertices[mesh.indices[i+1]].position, c = mesh.vertices[mesh.indices[i+2]].position;
        EXPECT_EQ(glm::cross(b - a, c - a), normals[i / 6]);
    }
}
TEST(Mesher, AdjacentCubesAndNegativeChunkSeam) {
    const auto registry = vc::builtinBlocks(); World world;
    Chunk a({-1, 0, 0}); a.set({31, 0, 0}, 1); a.set({30, 0, 0}, 1);
    EXPECT_EQ(meshChunk(a, world, registry, vc::builtinAtlasLayout()).indices.size(), 10u * 6);
    world.insert(std::move(a));
    Chunk b({0, 0, 0}); b.set({0, 0, 0}, 1); world.insert(std::move(b));
    EXPECT_EQ(meshChunk(world.chunks().at({-1, 0, 0}), world, registry, vc::builtinAtlasLayout()).indices.size(), 9u * 6);
    EXPECT_EQ(meshChunk(world.chunks().at({0, 0, 0}), world, registry, vc::builtinAtlasLayout()).indices.size(), 5u * 6);
}
TEST(Mesher, FullySolidChunkAndSixNeighbors) {
    const auto registry = vc::builtinBlocks(); World world;
    world.insert(Chunk({0, 0, 0}, 1));
    EXPECT_EQ(meshChunk(world.chunks().at({0, 0, 0}), world, registry, vc::builtinAtlasLayout()).indices.size(), 6u * 32 * 32 * 6);
    for (const Position p : {Position{1,0,0}, Position{-1,0,0}, Position{0,1,0}, Position{0,-1,0}, Position{0,0,1}, Position{0,0,-1}})
        world.insert(Chunk(p, 1));
    EXPECT_TRUE(meshChunk(world.chunks().at({0, 0, 0}), world, registry, vc::builtinAtlasLayout()).indices.empty());
}
TEST(Mesher, PartialVerticalChunkAndExtremeCoordinates) {
    const auto registry = vc::builtinBlocks(); World world({5, 5});
    Chunk chunk({0, 0, 0}, 1);
    EXPECT_EQ(meshChunk(chunk, world, registry, vc::builtinAtlasLayout()).indices.size(), (2u * 32 * 32 + 4u * 32) * 6);
    World wide;
    Chunk extreme({chunkAxis(INT64_MAX), 0, chunkAxis(INT64_MIN)});
    extreme.set({31, 0, 0}, 1);
    EXPECT_EQ(meshChunk(extreme, wide, registry, vc::builtinAtlasLayout()).indices.size(), 36u);
}
