#include "game/content/creature_models.hpp"

namespace vc {
namespace {
void box(Mesh& mesh,glm::vec3 low,glm::vec3 high,glm::vec3 color,glm::vec3 sway={}) {
    constexpr std::array<std::array<glm::vec3,4>,6> corners{{
        {{{1,0,0},{1,1,0},{1,1,1},{1,0,1}}},{{{0,0,1},{0,1,1},{0,1,0},{0,0,0}}},
        {{{0,1,1},{1,1,1},{1,1,0},{0,1,0}}},{{{0,0,0},{1,0,0},{1,0,1},{0,0,1}}},
        {{{1,0,1},{1,1,1},{0,1,1},{0,0,1}}},{{{0,0,0},{0,1,0},{1,1,0},{1,0,0}}}
    }};
    constexpr std::array<glm::vec3,6> normals{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
    for(std::size_t face=0;face<corners.size();++face) {
        const auto first=static_cast<std::uint32_t>(mesh.vertices.size());
        for(const auto corner:corners[face]) mesh.vertices.push_back({low+corner*(high-low),color,{},normals[face],1,{1,0},sway});
        for(const auto index:{0u,1u,2u,2u,3u,0u}) mesh.indices.push_back(first+index);
    }
}
}
std::array<Mesh,3> builtinCreatureModels() {
    std::array<Mesh,3> models;auto& grazer=models[0];auto& stalker=models[1];auto& drop=models[2];
    const glm::vec3 coat{0.71f,0.51f,0.28f},muzzle{0.3f,0.19f,0.1f};
    box(grazer,{-0.32f,0.42f,-0.35f},{0.32f,1.05f,0.35f},coat);
    box(grazer,{-0.24f,0.85f,-0.58f},{0.24f,1.35f,-0.22f},coat*1.15f);
    box(grazer,{-0.2f,0.87f,-0.65f},{0.2f,1.05f,-0.57f},muzzle);
    for(const float x:{-1.0f,1.0f}) {
        box(grazer,{x*0.19f-0.05f,1.34f,-0.44f},{x*0.19f+0.05f,1.55f,-0.34f},{0.88f,0.8f,0.6f});
        box(grazer,{x*0.19f-0.04f,1.13f,-0.59f},{x*0.19f+0.04f,1.22f,-0.58f},{0.04f,0.025f,0.01f});
        for(const float z:{-1.0f,1.0f}) box(grazer,{x*0.2f-0.09f,0,z*0.23f-0.09f},{x*0.2f+0.09f,0.48f,z*0.23f+0.09f},muzzle,{0,0,x*z*0.14f});
    }
    const glm::vec3 skin{0.23f,0.34f,0.29f},cloth{0.14f,0.2f,0.24f};
    box(stalker,{-0.26f,0.55f,-0.17f},{0.26f,1.14f,0.17f},cloth);
    box(stalker,{-0.23f,1.12f,-0.23f},{0.23f,1.55f,0.23f},skin);
    for(const float side:{-1.0f,1.0f}) {
        box(stalker,{side*0.14f-0.09f,0,-0.13f},{side*0.14f+0.09f,0.58f,0.13f},cloth*0.6f,{0,0,side*0.16f});
        box(stalker,{side*0.34f-0.07f,0.65f,-0.23f},{side*0.34f+0.07f,1.12f,0.04f},skin,{0,0,-side*0.12f});
        box(stalker,{side*0.11f-0.055f,1.33f,-0.242f},{side*0.11f+0.055f,1.39f,-0.23f},{0.9f,0.17f,0.04f});
    }
    box(drop,{-0.16f,0,-0.16f},{0.16f,0.32f,0.16f},{0.93f,0.57f,0.13f},{0,0.035f,0});
    box(drop,{-0.17f,0.12f,-0.17f},{0.17f,0.2f,0.17f},{0.5f,0.24f,0.05f},{0,0.035f,0});
    return models;
}
Mesh playerAvatarModel() {
    Mesh model;const glm::vec3 skin{0.73f,0.54f,0.39f},shirt{0.13f,0.43f,0.8f},pants{0.12f,0.17f,0.27f};
    box(model,{-0.24f,0.6f,-0.15f},{0.24f,1.35f,0.15f},shirt);
    box(model,{-0.22f,1.35f,-0.22f},{0.22f,1.8f,0.22f},skin);
    box(model,{-0.23f,1.67f,-0.23f},{0.23f,1.82f,0.23f},{0.14f,0.09f,0.06f});
    for(const float side:{-1.0f,1.0f}) {
        box(model,{side*0.13f-0.1f,0,-0.13f},{side*0.13f+0.1f,0.62f,0.13f},pants,{0,0,side*0.14f});
        box(model,{side*0.33f-0.08f,0.7f,-0.12f},{side*0.33f+0.08f,1.35f,0.12f},shirt,{0,0,-side*0.13f});
        box(model,{side*0.33f-0.08f,0.62f,-0.12f},{side*0.33f+0.08f,0.8f,0.12f},skin,{0,0,-side*0.13f});
        box(model,{side*0.095f-0.04f,1.56f,-0.232f},{side*0.095f+0.04f,1.63f,-0.22f},{0.025f,0.035f,0.05f});
    }
    return model;
}
}
