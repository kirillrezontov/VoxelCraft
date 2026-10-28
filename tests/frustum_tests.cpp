#include "engine/renderer/frustum.hpp"
#include "game/player/camera.hpp"
#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>

TEST(Frustum, VulkanClipPlanesAndTouchingBounds) {
    const vc::Frustum frustum(glm::mat4(1));
    EXPECT_TRUE(frustum.intersects({{-0.2,-0.2,0.2},{0.2,0.2,0.8}}));
    EXPECT_FALSE(frustum.intersects({{-0.2,-0.2,-0.4},{0.2,0.2,-0.1}}));
    for (const auto offset : {glm::dvec3{3,0,0}, {-3,0,0}, {0,3,0}, {0,-3,0}, {0,0,3}, {0,0,-3}})
        EXPECT_FALSE(frustum.intersects({{0,0,0.2},{0.1,0.1,0.3}},offset));
    EXPECT_TRUE(frustum.intersects({{1,0,0},{2,1,1}}));
    EXPECT_TRUE(frustum.intersects({{0,0,-1},{1,1,0}}));
    EXPECT_TRUE(frustum.intersects({{0,0,1},{1,1,2}}));
    EXPECT_TRUE(frustum.intersects({{-5,-5,-5},{5,5,5}}));
}

TEST(Frustum, PerspectiveNearFarSidesAndBehindCamera) {
    vc::Camera camera; camera.yaw = camera.pitch = 0;
    const vc::Frustum frustum(camera.viewProjection(1));
    const vc::MeshBounds box{{-0.5,-0.5,-0.5},{0.5,0.5,0.5}};
    EXPECT_TRUE(frustum.intersects(box,{0,0,-10}));
    EXPECT_TRUE(frustum.intersects(box,{0,0,0})); // Straddles the eye/near plane.
    EXPECT_FALSE(frustum.intersects(box,{0,0,10}));
    EXPECT_FALSE(frustum.intersects(box,{0,0,-1900}));
    EXPECT_FALSE(frustum.intersects(box,{100,0,-10}));
    EXPECT_FALSE(frustum.intersects(box,{-100,0,-10}));
    EXPECT_FALSE(frustum.intersects(box,{0,100,-10}));
    EXPECT_FALSE(frustum.intersects(box,{0,-100,-10}));
}

TEST(Frustum, CameraRotationAndRemoteOriginRemainRelative) {
    vc::Camera camera; camera.position={1e12,20,-1e12}; camera.yaw=glm::radians(90.0); camera.pitch=0;
    const vc::Frustum frustum(camera.viewProjection(16.0f/9.0f));
    const vc::MeshBounds box{{0,0,0},{32,32,32}};
    const glm::i64vec3 visibleOrigin{1000000000032LL,0,-1000000000000LL};
    EXPECT_TRUE(frustum.intersects(box,glm::dvec3(visibleOrigin)-camera.position));
    EXPECT_FALSE(frustum.intersects(box,{-100,0,0}));
}
