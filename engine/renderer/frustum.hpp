#pragma once
#include <array>
#include <glm/glm.hpp>

namespace vc {
// Bounds and planes are camera-relative, matching the renderer's small GPU floats.
struct MeshBounds { glm::dvec3 min{}, max{}; };
class Frustum {
public:
    // Vulkan clip space: -w <= x,y <= w and 0 <= z <= w.
    explicit Frustum(const glm::mat4& viewProjection);
    bool intersects(const MeshBounds& bounds, glm::dvec3 offset = {}) const;
private:
    std::array<glm::dvec4, 6> planes_;
};
}
