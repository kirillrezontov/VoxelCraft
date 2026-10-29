#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace vc {
struct Vertex {
    glm::vec3 position,color;
    glm::vec2 uv;
    glm::vec3 normal{};
    float opacity{1};
    glm::vec2 light{1,0}; // Direct sky visibility and propagated block light.
    glm::vec3 sway{}; // Optional local sinusoidal animation amplitude.
    glm::vec4 tileBounds{}; // xy atlas origin, zw tile extent; zero keeps ordinary atlas UVs.
};
// Local vertices stay small; the integer mesh origin is made camera-relative at draw time.
struct Mesh {
    glm::i64vec3 origin{};
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> translucentIndices;
};
struct RenderInstance {
    std::uint32_t model{};
    glm::dvec3 position{};
    float yaw{},phase{};
    glm::vec2 light{1,0};
};
}
