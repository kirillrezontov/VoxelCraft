#pragma once
#include "engine/voxel/mesher.hpp"
#include <set>
#include <span>

namespace vc::voxel {
// Main-thread mesh work is bounded per frame, with camera-near chunks first.
// Removal messages do not consume the CPU meshing budget.
template<ChunkMesher Mesher>
class BasicChunkMeshQueue {
public:
    void collect(World& world);
    void invalidate(std::span<const Position> chunks);
    std::vector<Mesh> rebuild(const World& world,const BlockRegistry& registry,const AtlasLayout& atlas,
        Position player,std::size_t budget=2,const Lighting* lighting=nullptr);
    std::size_t pendingCount() const { return pending_.size(); }
private:
    std::set<Position> pending_;
};
using ChunkMeshQueue = BasicChunkMeshQueue<GreedyChunkMesher>;
extern template class BasicChunkMeshQueue<GreedyChunkMesher>;
extern template class BasicChunkMeshQueue<CulledChunkMesher>;
}
#include "engine/voxel/mesh_queue_impl.hpp"
