#pragma once
#include "engine/voxel/terrain_worker.hpp"
#include <span>

namespace vc::voxel {
// Compact process-local cache for edited chunks outside residency. Runtime IDs
// are safe here; persistent snapshots still resolve namespaced palettes.
class PackedChunk {
public:
    explicit PackedChunk(const Chunk& chunk);
    Chunk unpack() const;
    std::size_t bytes() const { return runs_.size()*sizeof(Run)+raw_.size()*sizeof(BlockId); }
private:
    struct Run { BlockId block;std::uint16_t length; };
    Position position_;
    std::vector<Run> runs_;
    std::vector<BlockId> raw_;
};
// Player-near vertical chunks plus the generator's surface band. Sorted near-first.
std::vector<Position> streamingRegion(Position player,WorldBounds bounds,int radius,std::uint32_t generationVersion=1);
class ChunkStreamer {
public:
    ChunkStreamer(World& world,TerrainGenerator generator,int radius);
    // Retain chunks loaded from an existing save, including legacy full snapshots.
    void retain(std::span<const Position> chunks);
    // Main-thread ownership: only generation happens on the worker. Inserts at
    // most integrationBudget chunks per tick; unloaded meshes are dirtied by World.
    void tick(Position player,std::size_t integrationBudget=2);
    World snapshot() const;
    bool settled() const;
    std::size_t wantedCount() const { return wanted_.size(); }
    std::size_t cachedCount() const { return retained_.size(); }
    std::size_t cachedBytes() const;
    std::size_t pendingCount() const { return worker_.pendingCount(); }
private:
    World& world_;
    int radius_;
    std::uint32_t generationVersion_;
    TerrainWorker worker_;
    std::optional<Position> center_;
    std::vector<Position> ordered_;
    std::set<Position> wanted_;
    std::map<Position,PackedChunk> retained_;
};
}
