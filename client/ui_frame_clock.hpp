#pragma once
#include <algorithm>
#include <chrono>
#include <optional>
namespace vc::client {
// UI-thread animation time, independent of VSync and number of layout flushes.
class UiFrameClock {
public:
    using Clock = std::chrono::steady_clock;
    float advance(Clock::time_point now = Clock::now()) {
        const float delta = previous_ ? std::chrono::duration<float>(now-*previous_).count() : 0.f;
        previous_=now;
        // Resume without applying a long hidden/minimized interval in one update.
        return std::clamp(delta,0.f,.1f);
    }
    void reset() { previous_.reset(); }
private:
    std::optional<Clock::time_point> previous_;
};
}
