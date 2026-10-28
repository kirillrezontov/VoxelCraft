#include "engine/voxel/collision.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vc::voxel {
namespace {
constexpr double skin = 1e-7;
void validate(const Aabb& box) {
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(box.min[axis]) || !std::isfinite(box.max[axis]) || box.min[axis] >= box.max[axis] ||
            box.max[axis] - box.min[axis] > 8 || std::abs(box.min[axis]) > 1e15 || std::abs(box.max[axis]) > 1e15)
            throw std::invalid_argument("Collision box must be finite, positive, at most 8 units per axis, and within physics coordinate range");
    }
}
template<class Function> bool visitSolid(const World& world, const BlockRegistry& registry, const Aabb& range, Function visitor) {
    const Position low{static_cast<std::int64_t>(std::floor(range.min.x)), static_cast<std::int64_t>(std::floor(range.min.y)), static_cast<std::int64_t>(std::floor(range.min.z))};
    const Position high{static_cast<std::int64_t>(std::floor(std::nextafter(range.max.x, -INFINITY))),
        static_cast<std::int64_t>(std::floor(std::nextafter(range.max.y, -INFINITY))),
        static_cast<std::int64_t>(std::floor(std::nextafter(range.max.z, -INFINITY)))};
    for (auto y = low.y; y <= high.y; ++y) for (auto z = low.z; z <= high.z; ++z) for (auto x = low.x; x <= high.x; ++x) {
        const Position p{x,y,z};
        if (y > world.bounds().maxY) continue;
        if (y >= world.bounds().minY && world.loaded(p) && !registry.definition(world.block(p)).solid) continue;
        const glm::dvec3 origin(static_cast<double>(x), static_cast<double>(y), static_cast<double>(z));
        if (visitor(Aabb{origin, origin + 1.0},y>=world.bounds().minY&&!world.loaded(p))) return true;
    }
    return false;
}
}
bool Aabb::intersects(const Aabb& other) const {
    return min.x < other.max.x && max.x > other.min.x && min.y < other.max.y && max.y > other.min.y && min.z < other.max.z && max.z > other.min.z;
}
bool collides(const World& world, const BlockRegistry& registry, const Aabb& box) {
    validate(box);
    return visitSolid(world, registry, box, [&](const Aabb& block,bool) { return box.intersects(block); });
}
bool collidesKnownSolid(const World& world,const BlockRegistry& registry,const Aabb& box) {
    validate(box);
    return visitSolid(world,registry,box,[&](const Aabb& block,bool unavailable){return !unavailable&&box.intersects(block);});
}
CollisionMove moveAndCollide(const World& world, const BlockRegistry& registry, Aabb box, glm::dvec3 movement) {
    validate(box);
    for (int axis = 0; axis < 3; ++axis)
        if (!std::isfinite(movement[axis]) || std::abs(movement[axis]) > 64) throw std::invalid_argument("Collision movement must be finite and at most 64 units per axis");
    if (collides(world, registry, box)) return {{}, {true,true,true},{true,true,true}};
    CollisionMove result;
    for (const int axis : {1,0,2}) {
        const double requested = movement[axis];
        if (requested == 0) continue;
        double allowed = requested;
        Aabb swept = box;
        swept.min[axis] += std::min(0.0, requested); swept.max[axis] += std::max(0.0, requested);
        visitSolid(world, registry, swept, [&](const Aabb& block,bool unavailable) {
            for (int other = 0; other < 3; ++other) {
                if (other != axis && (box.max[other] <= block.min[other] || box.min[other] >= block.max[other])) return false;
            }
            double candidate=requested;
            if (requested > 0 && block.min[axis] >= box.max[axis])
                candidate = std::min(candidate, std::max(0.0, block.min[axis] - box.max[axis] - skin));
            if (requested < 0 && block.max[axis] <= box.min[axis])
                candidate = std::max(candidate, std::min(0.0, block.max[axis] - box.min[axis] + skin));
            if(std::abs(candidate)<std::abs(allowed)) { allowed=candidate;result.unavailable[axis]=unavailable; }
            else if(candidate==allowed&&candidate!=requested&&!unavailable) result.unavailable[axis]=false;
            return false;
        });
        result.applied[axis] = allowed; result.blocked[axis] = allowed != requested;
        box.min[axis] += allowed; box.max[axis] += allowed;
    }
    return result;
}
}
