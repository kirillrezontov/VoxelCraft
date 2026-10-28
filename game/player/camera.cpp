#include "game/player/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vc {
glm::dvec3 Camera::forward() const {
    return {std::cos(pitch) * std::sin(yaw), std::sin(pitch), -std::cos(pitch) * std::cos(yaw)};
}
void Camera::update(const CameraInput& input, double seconds) {
    if(!std::isfinite(mouseSensitivity)||mouseSensitivity<=0) throw std::invalid_argument("Camera mouse sensitivity must be positive and finite");
    yaw = std::remainder(yaw + static_cast<double>(input.mouseX) * mouseSensitivity, 2.0 * 3.141592653589793);
    pitch = std::clamp(pitch + static_cast<double>(input.mouseY) * mouseSensitivity * (invertY?1:-1), -1.553343, 1.553343);
    const glm::dvec3 direction = forward();
    const auto right = glm::normalize(glm::cross(direction, glm::dvec3(0, 1, 0)));
    auto movement = direction * static_cast<double>(input.forward) + right * static_cast<double>(input.right)
                  + glm::dvec3(0, input.up, 0);
    const double length = glm::length(movement);
    if (length > 1.0) movement /= length;
    position += movement * 24.0 * std::clamp(seconds, 0.0, 0.1);
}
glm::mat4 Camera::viewProjection(float aspect) const {
    if (!(aspect > 0)||!std::isfinite(aspect)) throw std::invalid_argument("Camera aspect must be positive and finite");
    if(!std::isfinite(fieldOfView)||fieldOfView<=0||fieldOfView>=180) throw std::invalid_argument("Invalid camera field of view");
    auto projection = glm::perspectiveRH_ZO(glm::radians(fieldOfView), aspect, 0.1f, 1800.0f);
    projection[1][1] *= -1; // Vulkan's positive viewport Y points down.
    return projection * glm::lookAtRH(glm::vec3(0), glm::vec3(forward()), glm::vec3(0, 1, 0));
}
}
