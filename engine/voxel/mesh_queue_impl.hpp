#pragma once
#include <algorithm>
#include <stdexcept>

namespace vc::voxel {
template<ChunkMesher Mesher>
void BasicChunkMeshQueue<Mesher>::collect(World& world) {
    const auto dirty=world.takeDirtyChunks();pending_.insert(dirty.begin(),dirty.end());
}
template<ChunkMesher Mesher>
void BasicChunkMeshQueue<Mesher>::invalidate(std::span<const Position> chunks) { pending_.insert(chunks.begin(),chunks.end()); }
template<ChunkMesher Mesher>
std::vector<Mesh> BasicChunkMeshQueue<Mesher>::rebuild(const World& world,const BlockRegistry& registry,const AtlasLayout& atlas,
        Position player,std::size_t budget,const Lighting* lighting) {
    if(!budget||budget>128) throw std::invalid_argument("Mesh rebuild budget must be 1..128");
    const auto center=chunkPosition(player);
    const auto distance=[&](Position p) {
        const auto x=static_cast<double>(p.x)-static_cast<double>(center.x),y=static_cast<double>(p.y)-static_cast<double>(center.y),
            z=static_cast<double>(p.z)-static_cast<double>(center.z);
        return x*x+y*y+z*z;
    };
    std::vector<Position> ordered(pending_.begin(),pending_.end());
    std::sort(ordered.begin(),ordered.end(),[&](Position a,Position b) {
        const auto da=distance(a),db=distance(b);return da==db?a<b:da<db;
    });
    std::vector<Mesh> result;
    std::size_t rebuilt=0;
    for(const auto p:ordered) {
        if(const auto* chunk=world.findChunk(p)) {
            if(rebuilt>=budget) continue;
            result.push_back(Mesher::build(*chunk,world,registry,atlas,lighting));++rebuilt;
        } else {
            Mesh empty;const auto origin=chunkOrigin(p);empty.origin={origin.x,origin.y,origin.z};result.push_back(std::move(empty));
        }
        pending_.erase(p);
    }
    return result;
}
}
