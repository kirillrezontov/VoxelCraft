#include "game/gameplay/interaction.hpp"
#include <cmath>

namespace vc {
bool destroyBlock(voxel::World& world, const voxel::RayHit& hit) {
    return world.loaded(hit.block) && world.block(hit.block) != voxel::air && world.setBlock(hit.block, voxel::air);
}
bool placeBlock(voxel::World& world, const voxel::BlockRegistry& registry, const voxel::RayHit& hit,
        voxel::BlockId block, const voxel::Aabb& player) {
    (void)registry.definition(block); // Reject invalid runtime IDs before touching storage.
    const auto normal = hit.normal;
    if (normal.x < -1 || normal.x > 1 || normal.y < -1 || normal.y > 1 || normal.z < -1 || normal.z > 1) return false;
    if (block == voxel::air || std::abs(normal.x) + std::abs(normal.y) + std::abs(normal.z) != 1 || world.block(hit.block) == voxel::air) return false;
    const auto position = voxel::offsetPosition(hit.block, normal);
    if (!position || !world.loaded(*position)) return false;
    const auto existing=world.block(*position);
    if(existing!=voxel::air&&!registry.definition(existing).fluid) return false;
    const glm::dvec3 origin(static_cast<double>(position->x), static_cast<double>(position->y), static_cast<double>(position->z));
    if (player.intersects({origin, origin + 1.0})) return false;
    return world.setBlock(*position, block);
}
}
