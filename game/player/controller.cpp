#include "game/player/controller.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vc {
voxel::Aabb PlayerController::bounds() const {
    return {camera.position - glm::dvec3(halfWidth, eyeHeight, halfWidth),
        camera.position + glm::dvec3(halfWidth, height - eyeHeight, halfWidth)};
}
void PlayerController::update(const PlayerInput& input, double seconds, const voxel::World& world, const voxel::BlockRegistry& registry) {
    landingSpeed=0;
    if (input.toggleFlight && (flying == false || !voxel::collides(world, registry, bounds()))) {
        flying = !flying; grounded = false; velocity_ = {}; accumulated_ = 0; jumpBuffer_ = 0;
    }
    if (flying) { swimming=false;camera.update(input.movement, seconds); return; }
    camera.update({0,0,0,input.movement.mouseX,input.movement.mouseY}, 0);
    glm::dvec3 movement{std::sin(camera.yaw) * input.movement.forward + std::cos(camera.yaw) * input.movement.right, 0,
        -std::cos(camera.yaw) * input.movement.forward + std::sin(camera.yaw) * input.movement.right};
    if (glm::length(movement) > 1) movement = glm::normalize(movement);
    if (!input.active) jumpBuffer_ = 0;
    else if (input.jump) jumpBuffer_ = 0.12;
    accumulated_ += std::clamp(seconds, 0.0, 0.1);
    constexpr double step = 1.0 / 120.0;
    while (accumulated_ + 1e-12 >= step) {
        accumulated_ = std::max(0.0, accumulated_ - step);
        const auto fluid=[&](glm::dvec3 p) {
            for(int axis=0;axis<3;++axis) if(!std::isfinite(p[axis])||std::abs(p[axis])>1e15)
                throw std::invalid_argument("Player is outside supported physics coordinates");
            const voxel::Position cell{static_cast<std::int64_t>(std::floor(p.x)),static_cast<std::int64_t>(std::floor(p.y)),static_cast<std::int64_t>(std::floor(p.z))};
            return registry.definition(world.block(cell)).fluid;
        };
        swimming=fluid({camera.position.x,bounds().min.y+0.1,camera.position.z});
        if (jumpBuffer_ > 0 && grounded&&!swimming) { velocity_.y = 8.2; grounded = false; jumpBuffer_ = 0; }
        jumpBuffer_ = std::max(0.0, jumpBuffer_ - step);
        velocity_.x = movement.x * (swimming?2.5:5); velocity_.z = movement.z * (swimming?2.5:5);
        const double previousVertical=velocity_.y;
        if(swimming) {
            if(input.active&&input.movement.up>0) velocity_.y=fluid(camera.position)?3.5:8.2;
            else if(input.active&&input.movement.up<0) velocity_.y=-3.5;
            else velocity_.y=std::max(-3.0,velocity_.y-4*step);
            jumpBuffer_=0;
        } else velocity_.y = std::max(-55.0, velocity_.y - 24.0 * step);
        const auto moved = voxel::moveAndCollide(world, registry, bounds(), velocity_ * step);
        camera.position += moved.applied;
        // A sweep ending exactly on a floor may report no clipping this step.
        // Probe a tiny distance below the feet to keep grounding stable on contact.
        grounded = velocity_.y <= 0 && ((moved.blocked.y&&!moved.unavailable.y) ||
            voxel::collidesKnownSolid(world, registry, {bounds().min - glm::dvec3(0,1e-6,0), bounds().max - glm::dvec3(0,1e-6,0)}));
        if (moved.blocked.y) {
            if(moved.unavailable.y) velocity_.y=previousVertical;
            else { if(velocity_.y<0) landingSpeed=std::max(landingSpeed,-velocity_.y);velocity_.y=0; }
        }
    }
}
}
