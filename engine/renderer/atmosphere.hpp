#pragma once
#include <glm/glm.hpp>

namespace vc {
struct Atmosphere {
    glm::vec3 skyColor{0.35f,0.60f,0.85f};
    glm::vec3 sunDirection{0.4f,0.85f,0.3f};
    float sunlight{1},ambient{0.25f},fogStart{1200},fogEnd{1800};
    bool horizontalFog{};
};
}
