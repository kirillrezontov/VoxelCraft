#pragma once
#include "engine/voxel/mesher.hpp"

namespace vc::examples {
// Source module: visible unit-face geometry with an original warm color policy.
// No world mutation, renderer API or runtime callback is involved.
struct WarmReferenceMesher {
    static Mesh build(const voxel::Chunk& chunk,const voxel::World& world,const voxel::BlockRegistry& blocks,
        const AtlasLayout& atlas,const voxel::Lighting* lighting) {
        auto result=voxel::CulledChunkMesher::build(chunk,world,blocks,atlas,lighting);
        for(auto& vertex:result.vertices) vertex.color*=glm::vec3(1.0f,0.72f,0.46f);
        return result;
    }
};
}
