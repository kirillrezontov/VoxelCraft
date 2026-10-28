#pragma once
#include "engine/voxel/world.hpp"

namespace vc::voxel {
enum class Biome { Plains,Forest,Desert,Mountains };
const char* biomeName(Biome biome);
// Version 1 terrain remains readable; version 2 adds deterministic trees and ores.
class TerrainGenerator {
public:
    TerrainGenerator(std::uint64_t seed, WorldBounds bounds, const BlockRegistry& registry,std::uint32_t version=1);
    std::int64_t surfaceY(std::int64_t x, std::int64_t z) const;
    BlockId sample(Position p) const;
    Chunk generate(Position chunk) const;
    std::uint32_t version() const { return version_; }
    Biome biomeAt(std::int64_t x,std::int64_t z) const;
private:
    std::uint64_t seed_;
    WorldBounds bounds_;
    BlockId stone_, dirt_, grass_;
    std::uint32_t version_;
    BlockId log_{},leaves_{},coalOre_{},ironOre_{};
    BlockId sand_{},sandstone_{},snow_{},water_{},cobblestone_{},lamp_{};
    struct TreeColumn { std::int64_t ground{};int dx{},dz{},height{};bool present{}; };
    struct RuinColumn { std::int64_t ground{};int dx{},dz{};std::uint64_t style{};bool present{}; };
    double noise(std::int64_t x,std::int64_t z,int scale,std::uint64_t salt=0) const;
    double noise3(Position p,int scale,std::uint64_t salt) const;
    BlockId columnBlock(std::int64_t y, std::int64_t surface) const;
    TreeColumn treeColumn(std::int64_t x,std::int64_t z) const;
    BlockId treeBlock(std::int64_t y,const TreeColumn& tree) const;
    BlockId resourceBlock(Position p) const;
    bool caveEntrance(std::int64_t x,std::int64_t z) const;
    bool caveAir(Position p,std::int64_t surface,bool entrance) const;
    RuinColumn ruinColumn(std::int64_t x,std::int64_t z) const;
    BlockId ruinBlock(std::int64_t y,std::int64_t surface,const RuinColumn& ruin) const;
    BlockId expandedSample(Position p,std::int64_t surface,Biome biome,bool entrance,const TreeColumn& tree,const RuinColumn& ruin) const;
};
}
