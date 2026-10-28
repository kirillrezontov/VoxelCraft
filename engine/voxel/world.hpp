#pragma once
#include "engine/voxel/coordinates.hpp"
#include "engine/voxel/registry.hpp"
#include <array>
#include <map>
#include <set>
#include <vector>

namespace vc::voxel {
class Chunk {
public:
    explicit Chunk(Position position, BlockId fill = air) : position_(position) {
        (void)chunkOrigin(position); blocks_.fill(fill);
    }
    Position position() const { return position_; }
    BlockId get(LocalPosition p) const { return blocks_[blockIndex(p)]; }
    void set(LocalPosition p, BlockId id) { blocks_[blockIndex(p)] = id; }
private:
    Position position_;
    std::array<BlockId, chunkVolume> blocks_{};
};
class World {
public:
    explicit World(WorldBounds bounds = {}) : bounds_(bounds) { bounds_.validate(); }
    const WorldBounds& bounds() const { return bounds_; }
    const std::map<Position, Chunk>& chunks() const { return chunks_; }
    void insert(Chunk chunk);
    std::optional<Chunk> remove(Position chunk);
    BlockId block(Position p) const;
    bool loaded(Position p) const;
    const Chunk* findChunk(Position chunk) const;
    // Only resident cells can be changed; false means unchanged or unavailable.
    bool setBlock(Position p, BlockId id);
    bool modified(Position chunk) const { return modified_.contains(chunk); }
    std::vector<Position> takeDirtyChunks();
private:
    void dirtyNeighbor(Position chunk);
    WorldBounds bounds_;
    std::map<Position, Chunk> chunks_;
    std::set<Position> dirty_;
    std::set<Position> modified_;
};
}
