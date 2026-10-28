#pragma once
#include "game/gameplay/items.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace vc {
// Stable save identity, separate from EnTT's process-local handles.
struct SavedActor {
    std::uint64_t id{};
    std::string kind;
    glm::dvec3 position{},velocity{}; // Feet position.
    double yaw{},age{},phase{};
    unsigned health{};
    SavedItemStack item;
};
inline constexpr std::size_t maxSavedActors=128;
void validateActors(const std::vector<SavedActor>& actors);
}
