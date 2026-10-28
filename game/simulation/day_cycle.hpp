#pragma once
#include <glm/glm.hpp>
#include <string>

namespace vc {
struct Daylight { glm::vec3 skyColor,sunDirection;float sunlight{},ambient{}; };
class DayCycle {
public:
    static constexpr double length=1200;
    explicit DayCycle(double seconds=300);
    void advance(double seconds);
    double seconds() const { return seconds_; }
    Daylight daylight() const;
    std::string clock() const;
private:
    double seconds_{};
};
}
