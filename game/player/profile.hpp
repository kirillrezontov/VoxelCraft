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
};
bool validPlayerName(std::string_view name);
std::string playerNameKey(std::string_view name);
void validatePlayerPose(const Camera& camera);
void validateHotbar(const std::array<std::string,9>& hotbar,std::uint32_t selectedSlot);
void validateProfile(const PlayerProfile& profile);
void validateProfiles(const std::vector<PlayerProfile>& profiles);
void writeProfile(binary::Writer& out,const PlayerProfile& profile);
PlayerProfile readProfile(binary::Reader& in,bool survivalFields=true);
}
