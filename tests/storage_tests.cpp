#include "engine/voxel/storage.hpp"
#include "engine/voxel/generation.hpp"
#include "engine/core/binary.hpp"
#include "engine/core/paths.hpp"
#include "game/content/blocks.hpp"
#include "tests/temp_directory.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <limits>

namespace {
void compare(const vc::voxel::World& a,const vc::voxel::World& b) {
    ASSERT_EQ(a.chunks().size(),b.chunks().size());
    for(const auto& [p,chunk]:a.chunks()) {
        const auto* other=b.findChunk(p);ASSERT_NE(other,nullptr);
        for(int y=0;y<32;++y) for(int z=0;z<32;++z) for(int x=0;x<32;++x)
            ASSERT_EQ(chunk.get({x,y,z}),other->get({x,y,z}));
    }
}
void write(const std::filesystem::path& path,const vc::binary::Bytes& bytes) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    ASSERT_TRUE(out);
}
}
TEST(Storage, RegionCoordinatesAndSnapshotRoundTrip) {
    using namespace vc::voxel;
    EXPECT_EQ(regionPosition({-1,-4,-5}),(Position{-1,-1,-2}));
    EXPECT_EQ(regionPosition({3,4,7}),(Position{0,1,1}));
    vc::test::TempDirectory temp;WorldStore store(temp.path());
    auto registry=vc::builtinBlocks();
    EXPECT_FALSE(store.load(registry));
    const WorldMetadata metadata{std::numeric_limits<std::uint64_t>::max(),{-35,12},1};
    const TerrainGenerator generator(metadata.seed,metadata.bounds,registry);
    World world(metadata.bounds);
    for(const Position p: {Position{-5,-2,4},{-1,-1,-1},{0,0,0},{4,0,-5}}) world.insert(generator.generate(p));
    world.setBlock({-1,-1,-1},registry.require("voxelcraft:dirt"));
    EXPECT_EQ(store.save(world,registry,metadata,std::string("player\0data",11)),1u);
    auto loaded=store.load(registry);ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->metadata.seed,metadata.seed);EXPECT_EQ(loaded->metadata.bounds.minY,-35);
    EXPECT_EQ(loaded->metadata.bounds.maxY,12);EXPECT_EQ(loaded->gameData,std::string("player\0data",11));
    compare(world,loaded->world);
    world.setBlock({0,0,0},registry.require("voxelcraft:stone"));
    EXPECT_EQ(store.save(world,registry,metadata,"second"),2u);
    EXPECT_EQ(store.save(world,registry,metadata,"third"),3u);
    EXPECT_FALSE(std::filesystem::exists(temp.path()/"snapshots/1"));
    EXPECT_TRUE(std::filesystem::exists(temp.path()/"snapshots/2/world.vcw"));
    loaded=store.load(registry);ASSERT_TRUE(loaded);EXPECT_EQ(loaded->revision,3u);
    EXPECT_EQ(loaded->gameData,"third");compare(world,loaded->world);
}

TEST(Storage, NamespacedPaletteSurvivesReorderedAndMissingContent) {
    using namespace vc::voxel;
    vc::test::TempDirectory temp;WorldStore store(temp.path());
    BlockRegistry original;
    const auto custom=original.add({"example:marble",true,{0.2f,0.3f,0.4f}});
    const auto other=original.add({"example:other",true,{1,1,1}});
    World world;Chunk chunk({-1,0,0},custom);chunk.set({31,0,0},other);world.insert(std::move(chunk));
    store.save(world,original,{});
    BlockRegistry reordered;const auto newOther=reordered.add({"example:other",true,{1,1,1}});
    auto archive=store.load(reordered);ASSERT_TRUE(archive);
    EXPECT_EQ(archive->world.block({-1,0,0}),newOther);
    const auto restored=reordered.require("example:marble");
    EXPECT_NE(custom,restored);EXPECT_EQ(archive->world.block({-2,0,0}),restored);
    EXPECT_TRUE(reordered.definition(restored).solid);
    EXPECT_EQ(reordered.definition(restored).color,glm::vec3(1,0,1));
    store.save(archive->world,reordered,archive->metadata);
    BlockRegistry restoredRegistry;restoredRegistry.add({"example:marble",true,{1,1,1}});
    const auto next=store.load(restoredRegistry);ASSERT_TRUE(next);
    EXPECT_EQ(restoredRegistry.definition(next->world.block({-2,0,0})).identifier,"example:marble");
}

TEST(Storage, UniformCompressionLocksAndUncommittedDirectories) {
    using namespace vc::voxel;
    vc::test::TempDirectory temp;
    {
        WorldStore store(temp.path());
        EXPECT_THROW(WorldStore another(temp.path()),std::runtime_error);
        auto registry=vc::builtinBlocks();World world;
        world.insert(Chunk({0,0,0},registry.require("voxelcraft:stone")));
        store.save(world,registry,{});
        EXPECT_LT(std::filesystem::file_size(temp.path()/"snapshots/1/r.0.0.0.vcr"),256u);
        std::filesystem::create_directory(temp.path()/"snapshots/.writing-2");
        std::ofstream(temp.path()/"snapshots/.writing-2/world.vcw")<<"incomplete";
        EXPECT_EQ(store.load(registry)->revision,1u);
        EXPECT_EQ(store.save(world,registry,{}),2u);
    }
    EXPECT_TRUE(std::filesystem::exists(temp.path()/".session.lock"));
    WorldStore reopened(temp.path());
    auto registry=vc::builtinBlocks();EXPECT_EQ(reopened.load(registry)->revision,2u);
}

TEST(Storage, CorruptionAndFailedSaveDoNotReplacePreviousSnapshot) {
    using namespace vc::voxel;
    vc::test::TempDirectory temp;WorldStore store(temp.path());
    auto registry=vc::builtinBlocks();World world;world.insert(Chunk({0,0,0}));
    store.save(world,registry,{},"kept");
    auto invalid=WorldMetadata{};invalid.bounds={-1,1};
    EXPECT_THROW(store.save(world,registry,invalid),std::invalid_argument);
    world.setBlock({0,0,0},std::numeric_limits<BlockId>::max());
    EXPECT_THROW(store.save(world,registry,{}),std::out_of_range);
    EXPECT_FALSE(std::filesystem::exists(temp.path()/"snapshots/2"));
    EXPECT_EQ(store.load(registry)->gameData,"kept");
    const auto region=temp.path()/"snapshots/1/r.0.0.0.vcr";
    std::ofstream(region,std::ios::binary|std::ios::app).put('x');
    EXPECT_THROW(store.load(registry),std::runtime_error);
}

TEST(Storage, RejectsChecksummedUnsupportedFormatsAndMalformedRegions) {
    using namespace vc::voxel;
    vc::test::TempDirectory temp;WorldStore store(temp.path());
    auto registry=vc::builtinBlocks();World world;world.insert(Chunk({0,0,0}));store.save(world,registry,{});
    vc::binary::Writer out;out.raw("VCREGION");out.u32(1);out.i64(0);out.i64(0);out.i64(0);
    out.u32(1);out.text("voxelcraft:air");out.u32(1);out.i64(0);out.i64(0);out.i64(0);
    out.u32(1);out.u16(0);out.u16(0);out.seal(); // Zero-length run, valid checksum.
    write(temp.path()/"snapshots/1/r.0.0.0.vcr",out.bytes);
    EXPECT_THROW(store.load(registry),std::runtime_error);
    vc::binary::Writer manifest;manifest.raw("VCWORLD1");manifest.u32(99);manifest.seal();
    write(temp.path()/"snapshots/1/world.vcw",manifest.bytes);
    EXPECT_THROW(store.load(registry),std::runtime_error);
}

TEST(Storage, EmptyWorldAndGameDataLimits) {
    vc::test::TempDirectory temp;vc::voxel::WorldStore store(temp.path());
    auto registry=vc::builtinBlocks();vc::voxel::World world;
    EXPECT_THROW(store.save(world,registry,{},std::string(65537,'x')),std::runtime_error);
    store.save(world,registry,{});
    const auto archive=store.load(registry);ASSERT_TRUE(archive);EXPECT_TRUE(archive->world.chunks().empty());
}
TEST(Storage, UnicodeWorldPathsUseUtf8AtTextBoundaries) {
    vc::test::TempDirectory temp;
    const std::u8string name=u8"world-\u0442\u0435\u0441\u0442-\u4e16\u754c";
    const auto path=temp.path()/std::filesystem::path(name);
    const auto text=vc::pathText(path);
    EXPECT_EQ(vc::utf8Path(text),path);
    vc::voxel::WorldStore store(path);auto registry=vc::builtinBlocks();vc::voxel::World world;
    world.insert(vc::voxel::Chunk({-1,0,0},registry.require("voxelcraft:stone")));
    store.save(world,registry,{});const auto loaded=store.load(registry);ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->world.block({-1,0,0}),registry.require("voxelcraft:stone"));
}
