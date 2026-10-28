#include "engine/renderer/frustum.hpp"
#include <algorithm>
#include <cmath>

namespace vc {
Frustum::Frustum(const glm::mat4& matrix) {
    const auto row = [&](int r) { return glm::dvec4(matrix[0][r], matrix[1][r], matrix[2][r], matrix[3][r]); };
    planes_ = {row(3)+row(0), row(3)-row(0), row(3)+row(1), row(3)-row(1), row(2), row(3)-row(2)};
}
bool Frustum::intersects(const MeshBounds& bounds, glm::dvec3 offset) const {
    const auto min = bounds.min + offset, max = bounds.max + offset;
    for (const auto& plane : planes_) {
        const glm::dvec3 support{plane.x >= 0 ? max.x : min.x,
            plane.y >= 0 ? max.y : min.y, plane.z >= 0 ? max.z : min.z};
        // Be conservative near clip planes: shader arithmetic uses single precision.
        const double margin = 1e-5 * (1 + glm::dot(glm::abs(glm::dvec3(plane)), glm::abs(support)) + std::abs(plane.w));
        if (glm::dot(glm::dvec3(plane), support) + plane.w < -margin) return false;
    }
    return true;
}
}
