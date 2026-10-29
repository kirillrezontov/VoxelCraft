#include "engine/voxel/world.hpp"
#include <stdexcept>
#include <limits>
#include <utility>

namespace vc::voxel {
void World::insert(Chunk chunk) {
    const auto position = chunk.position();
    const auto origin = chunkOrigin(position);
    if (origin.y > bounds_.maxY || origin.y + chunkSize - 1 < bounds_.minY)
        throw std::out_of_range("Chunk is entirely outside world height bounds");
    if(nextRevision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("World chunk revision exhausted");
    chunk.revision_=++nextRevision_;
    chunks_.insert_or_assign(position, std::move(chunk));
    dirtyNeighbor(position);
    for (const LocalPosition direction : {LocalPosition{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}}) {
        if (const auto adjacent = offsetPosition(position, direction)) dirtyNeighbor(*adjacent);
    }
}
std::optional<Chunk> World::remove(Position position) {
    auto node=chunks_.extract(position);
    if(node.empty()) return std::nullopt;
    modified_.erase(position);dirty_.insert(position);
    for(const LocalPosition direction:{LocalPosition{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}})
        if(const auto adjacent=offsetPosition(position,direction)) dirtyNeighbor(*adjacent);
    return std::move(node.mapped());
}
BlockId World::block(Position p) const {
    if (!bounds_.contains(p.y)) return air;
    const auto it = chunks_.find(chunkPosition(p));
    return it == chunks_.end() ? air : it->second.get(localPosition(p));
}
bool World::loaded(Position p) const { return bounds_.contains(p.y) && chunks_.contains(chunkPosition(p)); }
const Chunk* World::findChunk(Position chunk) const {
    const auto it = chunks_.find(chunk);
    return it == chunks_.end() ? nullptr : &it->second;
}
std::uint64_t World::revision(Position chunk) const { const auto* found=findChunk(chunk);return found?found->revision():0; }
void World::dirtyNeighbor(Position chunk) { if (chunks_.contains(chunk)) dirty_.insert(chunk); }
bool World::setBlock(Position p, BlockId id) {
    if (!bounds_.contains(p.y)) return false;
    const auto position = chunkPosition(p);
    const auto it = chunks_.find(position);
    if (it == chunks_.end()) return false;
    const auto local = localPosition(p);
    if (it->second.get(local) == id) return false;
    if(nextRevision_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("World chunk revision exhausted");
    it->second.set(local, id);
    it->second.revision_=++nextRevision_;
    modified_.insert(position);
    dirtyNeighbor(position);
    for (const LocalPosition direction : {LocalPosition{local.x == 0 ? -1 : local.x == chunkSize - 1 ? 1 : 0,0,0},
        {0,local.y == 0 ? -1 : local.y == chunkSize - 1 ? 1 : 0,0}, {0,0,local.z == 0 ? -1 : local.z == chunkSize - 1 ? 1 : 0}}) {
        if (const auto adjacent = offsetPosition(position, direction)) dirtyNeighbor(*adjacent);
    }
    return true;
}
std::vector<Position> World::takeDirtyChunks() {
    std::vector<Position> result(dirty_.begin(), dirty_.end()); dirty_.clear(); return result;
}
}
