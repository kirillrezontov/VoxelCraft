#pragma once
#include "engine/renderer/mesh.hpp"
#include "engine/voxel/world.hpp"
#include <concepts>

namespace vc::voxel {
class Lighting;
// All chunks must be inserted before meshing, so boundary faces see their neighbors.
Mesh meshChunk(const Chunk& chunk,const World& world,const BlockRegistry& registry,const AtlasLayout& atlas,const Lighting* lighting=nullptr,bool greedy=true);

// A stateless, compile-time CPU module. It receives immutable job-owned values,
// returns an owning mesh, and must not retain references after build returns.
template<class T>
concept ChunkMesher = requires(const Chunk& chunk, const World& world,
    const BlockRegistry& registry, const AtlasLayout& atlas, const Lighting* lighting) {
    { T::build(chunk, world, registry, atlas, lighting) } -> std::same_as<Mesh>;
};
struct GreedyChunkMesher {
    static Mesh build(const Chunk& chunk, const World& world, const BlockRegistry& registry,
        const AtlasLayout& atlas, const Lighting* lighting) {
        return meshChunk(chunk, world, registry, atlas, lighting, true);
    }
};
struct CulledChunkMesher {
    static Mesh build(const Chunk& chunk, const World& world, const BlockRegistry& registry,
        const AtlasLayout& atlas, const Lighting* lighting) {
        return meshChunk(chunk, world, registry, atlas, lighting, false);
    }
};
}
