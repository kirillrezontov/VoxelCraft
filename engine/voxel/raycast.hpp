#pragma once
#include "engine/voxel/world.hpp"
#include <glm/vec3.hpp>
#include <optional>

namespace vc::voxel {
struct RayHit {
    Position block;
    LocalPosition normal; // Outward face normal; zero when starting inside a block.
    double distance{};
};
// Amanatides-Woo grid traversal. Distances are world units, independent of direction length.
// Edge/corner crossings step all tied axes; placement chooses X, then Y, then Z's face.
std::optional<RayHit> raycast(const World& world,glm::dvec3 origin,glm::dvec3 direction,double reach=6.0,
    const BlockRegistry* skipFluids=nullptr);
}
