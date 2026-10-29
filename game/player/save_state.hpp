#pragma once
#include "game/player/camera.hpp"
#include "game/player/profile.hpp"
#include "game/gameplay/items.hpp"
#include "game/simulation/entity_data.hpp"
#include "game/modding/save_data.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace vc {
struct PlayerSaveState {
    Camera camera;
    bool flying{};
    std::array<std::string,9> hotbar{"voxelcraft:grass","voxelcraft:dirt","voxelcraft:stone"};
    std::uint32_t selectedSlot{};
    bool survival{};
    unsigned health{20};
    std::array<SavedItemStack,SurvivalInventory::slotCount> items;
    double worldTime{300};
    std::vector<SavedActor> actors;
    std::vector<SavedMod> mods;
    std::vector<PlayerProfile> profiles;
    std::optional<glm::dvec3> spawnPoint{};
};
// Explicit versioned encoding inside the world's transactional game-data payload.
std::string encodePlayer(const PlayerSaveState& player);
PlayerSaveState decodePlayer(std::string_view data);
}
