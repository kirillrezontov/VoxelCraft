#pragma once
#include "engine/network/channel.hpp"
#include "engine/voxel/storage.hpp"
#include "game/player/profile.hpp"
#include "game/simulation/entity_data.hpp"
#include "game/network/content_fingerprint.hpp"
#include "game/commands/commands.hpp"

namespace vc::netgame {
inline constexpr std::uint32_t protocolVersion=4;
inline constexpr std::size_t maxPlayers=8,maxNetworkBlocks=8192;
enum class Message : std::uint32_t { Hello=1,Welcome,Input,Hotbar,Chunk,Unload,State,Notice,Reject,Save,Goodbye,Inventory,Command,CommandReply };
enum InputFlags : std::uint32_t { Active=1,Jump=2,ToggleFlight=4,Destroy=8,Place=16,AttackHeld=32,Respawn=64 };
struct Hello { std::uint64_t fingerprint{};std::string name;std::string identityToken{}; };
struct Welcome {
    std::uint64_t id{},tick{};
    voxel::WorldMetadata metadata;
    std::uint32_t radius{2};
    double worldTime{300};
    PlayerProfile profile;
    bool survival{};
    std::vector<std::string> blocks;
};
struct CommandRequest { std::uint64_t id{};std::string text; };
struct CommandReply { std::uint64_t id{};commands::Result result; };
struct Input {
    std::uint64_t sequence{};
    double yaw{},pitch{},forward{},right{},up{};
    std::uint32_t flags{},slot{};
};
struct Hotbar { std::array<std::string,9> names;std::uint32_t slot{}; };
struct Peer {
    std::uint64_t id{};
    std::string name;
    Camera camera;
    bool flying{};
    unsigned health{20};
};
struct SurvivalSnapshot {
    unsigned health{20};
    std::uint32_t selectedSlot{};
    std::array<SavedItemStack,SurvivalInventory::slotCount> items;
    double miningProgress{};
    bool mining{};
};
struct State {
    std::uint64_t tick{};
    double worldTime{300};
    std::vector<Peer> players;
    std::vector<SavedActor> actors;
    std::optional<SurvivalSnapshot> survival;
    std::array<std::string,9> hotbar{};
    std::uint32_t selectedSlot{};
    std::uint64_t poseRevision{};
    commands::Permission permission{commands::Permission::Player};
};
struct ChunkUpdate { voxel::Chunk chunk;std::uint64_t revision{}; };
Message messageType(std::span<const std::uint8_t> bytes);
binary::Bytes encode(const CommandRequest& message);
binary::Bytes encode(const CommandReply& message);
CommandRequest decodeCommand(std::span<const std::uint8_t> bytes);
CommandReply decodeCommandReply(std::span<const std::uint8_t> bytes);
binary::Bytes encode(const Hello& message);
binary::Bytes encode(const Welcome& message);
binary::Bytes encode(const Input& message);
binary::Bytes encode(const Hotbar& message);
binary::Bytes encode(const State& message);
binary::Bytes encode(const InventoryAction& message);
Hello decodeHello(std::span<const std::uint8_t> bytes);
Welcome decodeWelcome(std::span<const std::uint8_t> bytes);
Input decodeInput(std::span<const std::uint8_t> bytes);
Hotbar decodeHotbar(std::span<const std::uint8_t> bytes);
State decodeState(std::span<const std::uint8_t> bytes);
InventoryAction decodeInventory(std::span<const std::uint8_t> bytes);
binary::Bytes encodeChunk(const voxel::Chunk& chunk,std::uint64_t revision,std::size_t blockCount);
ChunkUpdate decodeChunk(std::span<const std::uint8_t> bytes,std::span<const voxel::BlockId> mapping);
binary::Bytes encodeUnload(voxel::Position chunk);
voxel::Position decodeUnload(std::span<const std::uint8_t> bytes);
binary::Bytes encodeText(Message type,std::string_view text);
std::string decodeText(std::span<const std::uint8_t> bytes,Message type);
binary::Bytes encodeControl(Message type);
void decodeControl(std::span<const std::uint8_t> bytes,Message type);
}
