#pragma once
#include <glm/glm.hpp>

namespace vc {
struct CameraInput { float forward{}, right{}, up{}, mouseX{}, mouseY{}; };
class Camera {
public:
    glm::dvec3 position{80.0, 65.0, 100.0};
    double yaw{-0.65}, pitch{-0.38};
    double mouseSensitivity{0.0025};
    float fieldOfView{75};
    bool invertY{};
    void update(const CameraInput& input, double seconds);
    glm::dvec3 forward() const;
    glm::mat4 viewProjection(float aspect) const;
};
}
