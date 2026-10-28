#pragma once
#include "engine/voxel/raycast.hpp"
#include "engine/voxel/collision.hpp"

namespace vc {
bool destroyBlock(voxel::World& world, const voxel::RayHit& hit);
bool placeBlock(voxel::World& world, const voxel::BlockRegistry& registry, const voxel::RayHit& hit,
    voxel::BlockId block, const voxel::Aabb& player);
}
