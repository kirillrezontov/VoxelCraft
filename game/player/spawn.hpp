#pragma once
#include "engine/voxel/generation.hpp"
#include <glm/vec3.hpp>

namespace vc {
glm::dvec3 findSpawn(const voxel::TerrainGenerator& generator,const voxel::BlockRegistry& blocks,const voxel::World* edits=nullptr);
}
