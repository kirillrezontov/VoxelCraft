#pragma once
#include "game/network/protocol.hpp"
#include "game/network/identity.hpp"
#include <filesystem>
#include <memory>
#include <optional>

namespace vc::server {
struct HostConfig {
    std::optional<std::filesystem::path> worldDirectory;
    std::filesystem::path modsDirectory;
    std::optional<std::uint64_t> seed;
    std::optional<std::int64_t> minY,maxY;
    std::optional<std::uint32_t> generatorVersion;
    std::optional<bool> survival;
    std::string bindAddress{"127.0.0.1"};
    std::uint16_t port{25565};
    int radius{2};
    bool populate{true};
    bool trustedLan{}; // Explicit opt-in for unauthenticated non-loopback hosting.
    netgame::ProfileTokens profileTokens;
};
// Authoritative host. The same CPU world/player/mod/actor services run
// here without creating an SDL window or a Vulkan device.
class Host {
public:
    explicit Host(HostConfig config);
    ~Host();
    Host(const Host&)=delete;
    Host& operator=(const Host&)=delete;
    void tick(double seconds);
    std::uint64_t save(); // Zero for a temporary world.
    void shutdown(); // Call after a successful final save; best-effort bounded notification.
    bool ready() const;
    std::uint16_t port() const;
    std::size_t playerCount() const;
    std::uint64_t ticks() const;
    const voxel::World& world() const;
    const voxel::BlockRegistry& blocks() const;
    std::vector<PlayerProfile> profiles() const;
    std::vector<netgame::Peer> players() const;
    std::uint64_t fingerprint() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
