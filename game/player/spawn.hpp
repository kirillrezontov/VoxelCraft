#pragma once
#include "engine/voxel/generation.hpp"
#include "engine/voxel/collision.hpp"
#include <span>
#include <glm/vec3.hpp>

namespace vc {
namespace voxel { class ChunkStreamer; }
glm::dvec3 findSpawn(const voxel::TerrainGenerator& generator,const voxel::BlockRegistry& blocks,
    const voxel::World* edits=nullptr,std::span<const voxel::Aabb> occupied={});
glm::dvec3 findSpawn(const voxel::TerrainGenerator& generator,const voxel::BlockRegistry& blocks,
    const voxel::ChunkStreamer& streamer,std::span<const voxel::Aabb> occupied={});
}
