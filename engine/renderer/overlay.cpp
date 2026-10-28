#include "engine/renderer/overlay.hpp"
#include <array>

namespace vc {
namespace {
void box(Mesh& mesh, glm::vec3 low, glm::vec3 high) {
    constexpr std::array<std::array<glm::vec3,4>,6> corners{{
        {{{1,0,0},{1,1,0},{1,1,1},{1,0,1}}}, {{{0,0,1},{0,1,1},{0,1,0},{0,0,0}}},
        {{{0,1,1},{1,1,1},{1,1,0},{0,1,0}}}, {{{0,0,0},{1,0,0},{1,0,1},{0,0,1}}},
        {{{1,0,1},{1,1,1},{0,1,1},{0,0,1}}}, {{{0,0,0},{0,1,0},{1,1,0},{1,0,0}}}
    }};
    for(const auto& face:corners) {
        const auto start=static_cast<std::uint32_t>(mesh.vertices.size());
        for(const auto corner:face) mesh.vertices.push_back({low+corner*(high-low),{1,0.72f,0.08f},{}});
        for(const auto index:{0u,1u,2u,2u,3u,0u}) mesh.indices.push_back(start+index);
    }
}
}
Mesh selectionOutline() {
    Mesh mesh;
    for(int axis=0;axis<3;++axis) for(int a=0;a<=1;++a) for(int b=0;b<=1;++b) {
        glm::vec3 low{},high{};
        low[axis]=-0.01f;high[axis]=1.01f;
        low[(axis+1)%3]=static_cast<float>(a)-0.01f;high[(axis+1)%3]=static_cast<float>(a)+0.01f;
        low[(axis+2)%3]=static_cast<float>(b)-0.01f;high[(axis+2)%3]=static_cast<float>(b)+0.01f;
        box(mesh,low,high);
    }
    return mesh;
}
Mesh crosshairMesh() {
    Mesh mesh;
    for(const auto extent:{glm::vec2(6,1),glm::vec2(1,6)}) {
        const auto start=static_cast<std::uint32_t>(mesh.vertices.size());
        for(const auto p:{glm::vec2(-extent.x,-extent.y),glm::vec2(extent.x,-extent.y),extent,glm::vec2(-extent.x,extent.y)})
            mesh.vertices.push_back({{p,0},{1,1,1},{}});
        // Both windings keep this screen-space primitive independent of viewport Y conventions.
        for(const auto index:{0u,1u,2u,2u,3u,0u,2u,1u,0u,0u,3u,2u}) mesh.indices.push_back(start+index);
    }
    return mesh;
}
Mesh progressBarMesh(glm::vec3 color) {
    Mesh mesh;
    for(const auto p:{glm::vec2(0,0),glm::vec2(100,0),glm::vec2(100,4),glm::vec2(0,4)}) mesh.vertices.push_back({{p,0},color,{}});
    mesh.indices={0,1,2,2,3,0};return mesh;
}
}
