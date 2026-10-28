#pragma once
#include "game/player/camera.hpp"
#include "engine/voxel/collision.hpp"

namespace vc {
struct PlayerInput { CameraInput movement; bool jump{}, toggleFlight{}; bool active{true}; };
class PlayerController {
public:
    static constexpr double eyeHeight = 1.62, height = 1.8, halfWidth = 0.3;
    Camera camera;
    bool flying{}, grounded{},swimming{};
    double landingSpeed{};
    voxel::Aabb bounds() const;
    void update(const PlayerInput& input, double seconds, const voxel::World& world, const voxel::BlockRegistry& registry);
private:
    glm::dvec3 velocity_{};
    double accumulated_{};
    double jumpBuffer_{};
};
}
