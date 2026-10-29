#pragma once
#include "game/player/camera.hpp"
#include "game/gameplay/items.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vc::binary { class Writer;class Reader; }
namespace vc {
inline constexpr std::size_t maxSavedProfiles=32;
struct PlayerProfile {
    std::string name;
    Camera camera;
    bool flying{};
    std::array<std::string,9> hotbar{"voxelcraft:grass","voxelcraft:dirt","voxelcraft:stone"};
    std::uint32_t selectedSlot{};
    unsigned health{20};
    std::array<SavedItemStack,SurvivalInventory::slotCount> items;
    std::optional<bool> survivalMode{}; // Unset in pre-v10 saves: inherit the saved world default.
    std::optional<glm::dvec3> spawnPoint{}; // Feet, not eye coordinates.
};
bool validPlayerName(std::string_view name);
std::string playerNameKey(std::string_view name);
void validatePlayerPose(const Camera& camera);
void validateHotbar(const std::array<std::string,9>& hotbar,std::uint32_t selectedSlot);
void validateProfile(const PlayerProfile& profile);
void validateProfiles(const std::vector<PlayerProfile>& profiles);
void validateSpawnPoint(const std::optional<glm::dvec3>& spawn);
void writeSpawnPoint(binary::Writer& out,const std::optional<glm::dvec3>& spawn);
std::optional<glm::dvec3> readSpawnPoint(binary::Reader& in);
void writeProfile(binary::Writer& out,const PlayerProfile& profile,bool extendedFields=true);
PlayerProfile readProfile(binary::Reader& in,bool survivalFields=true,bool extendedFields=false);
}
