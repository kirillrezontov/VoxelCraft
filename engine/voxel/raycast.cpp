#include "engine/voxel/raycast.hpp"
#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace vc::voxel {
std::optional<RayHit> raycast(const World& world,glm::dvec3 origin,glm::dvec3 direction,double reach,const BlockRegistry* skipFluids) {
    if (!std::isfinite(reach) || reach < 0 || reach > 1024) throw std::invalid_argument("Ray reach must be finite and inside [0,1024]");
    const double integerLimit = std::ldexp(1.0, 63);
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(origin[axis]) || origin[axis] < -integerLimit || origin[axis] >= integerLimit || !std::isfinite(direction[axis]))
            throw std::invalid_argument("Ray coordinates must be finite and representable");
    }
    const double scale = std::max({std::abs(direction.x), std::abs(direction.y), std::abs(direction.z)});
    if (scale == 0) throw std::invalid_argument("Ray direction must be nonzero");
    direction /= scale; direction /= glm::length(direction);
    Position cell{static_cast<std::int64_t>(std::floor(origin.x)), static_cast<std::int64_t>(std::floor(origin.y)), static_cast<std::int64_t>(std::floor(origin.z))};
    const LocalPosition step{direction.x > 0 ? 1 : direction.x < 0 ? -1 : 0,
        direction.y > 0 ? 1 : direction.y < 0 ? -1 : 0, direction.z > 0 ? 1 : direction.z < 0 ? -1 : 0};
    const glm::ivec3 steps{step.x, step.y, step.z};
    const double infinity = std::numeric_limits<double>::infinity();
    glm::dvec3 next{}, interval{};
    LocalPosition normal{};
    for (int axis = 0; axis < 3; ++axis) {
        double fraction = origin[axis] - std::floor(origin[axis]);
        if (steps[axis] < 0 && fraction == 0) {
            LocalPosition delta{};
            if (axis == 0) delta.x = -1; else if (axis == 1) delta.y = -1; else delta.z = -1;
            const auto shifted = offsetPosition(cell, delta);
            if (!shifted) return std::nullopt;
            cell = *shifted; fraction = 1;
            if (normal == LocalPosition{}) {
                if (axis == 0) normal.x = 1; else if (axis == 1) normal.y = 1; else normal.z = 1;
            }
        }
        interval[axis] = steps[axis] == 0 ? infinity : 1.0 / std::abs(direction[axis]);
        next[axis] = steps[axis] == 0 ? infinity : (steps[axis] > 0 ? 1 - fraction : fraction) * interval[axis];
    }
    double distance = 0;
    while (true) {
        if (world.loaded(cell)&&world.block(cell)!=air&&(!skipFluids||!skipFluids->definition(world.block(cell)).fluid)) return RayHit{cell,normal,distance};
        distance = std::min({next[0], next[1], next[2]});
        if (distance > reach) return std::nullopt;
        LocalPosition delta{}; normal = {};
        for (int axis = 0; axis < 3; ++axis) {
            if (next[axis] != distance) continue;
            if (axis == 0) delta.x = steps[axis]; else if (axis == 1) delta.y = steps[axis]; else delta.z = steps[axis];
            if (normal == LocalPosition{}) {
                if (axis == 0) normal.x = -steps[axis]; else if (axis == 1) normal.y = -steps[axis]; else normal.z = -steps[axis];
            }
            next[axis] += interval[axis];
        }
        const auto shifted = offsetPosition(cell, delta);
        if (!shifted) return std::nullopt;
        cell = *shifted;
    }
}
}
