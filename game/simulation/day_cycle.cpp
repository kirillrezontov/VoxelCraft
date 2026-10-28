#include "game/simulation/day_cycle.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace vc {
DayCycle::DayCycle(double seconds) {
    if(!std::isfinite(seconds)||seconds<0) throw std::invalid_argument("World time must be finite and nonnegative");
    seconds_=std::fmod(seconds,length);
}
void DayCycle::advance(double seconds) {
    if(!std::isfinite(seconds)||seconds<0) throw std::invalid_argument("World time delta must be finite and nonnegative");
    seconds_=std::fmod(seconds_+seconds,length);
}
Daylight DayCycle::daylight() const {
    const double angle=seconds_/length*6.283185307179586;
    const float height=static_cast<float>(std::sin(angle));
    float day=std::clamp((height+0.15f)/0.4f,0.0f,1.0f);day=day*day*(3-2*day);
    const float horizon=std::max(0.0f,1-std::abs(height)*4)*day;
    const auto sky=glm::mix(glm::vec3(0.002f,0.004f,0.012f),glm::vec3(0.26f,0.5f,0.82f),day)+glm::vec3(0.16f,0.035f,0)*horizon;
    return {sky,glm::normalize(glm::vec3(static_cast<float>(std::cos(angle))*0.8f,height,0.3f)),
        std::max(0.0f,height),0.07f+0.18f*day};
}
std::string DayCycle::clock() const {
    const int minutes=(static_cast<int>(seconds_/length*1440)+360)%1440;
    std::ostringstream out;out<<std::setfill('0')<<std::setw(2)<<minutes/60<<':'<<std::setw(2)<<minutes%60;return out.str();
}
}
