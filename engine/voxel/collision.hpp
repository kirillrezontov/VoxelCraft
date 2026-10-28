#pragma once
#include "engine/voxel/world.hpp"
#include <glm/glm.hpp>

namespace vc::voxel {
struct Aabb {
    glm::dvec3 min, max;
    bool intersects(const Aabb& other) const;
};
struct CollisionMove { glm::dvec3 applied{}; glm::bvec3 blocked{false},unavailable{false}; };
// Missing resident data blocks walking; above maxY is open, below minY is closed.
bool collides(const World& world, const BlockRegistry& registry, const Aabb& box);
// Used for grounding/impact decisions: unknown streamed data is not a real floor.
bool collidesKnownSolid(const World& world,const BlockRegistry& registry,const Aabb& box);
// Sweeps each axis (Y, X, Z) and slides along contacts, without tunneling.
CollisionMove moveAndCollide(const World& world, const BlockRegistry& registry, Aabb box, glm::dvec3 movement);
}
