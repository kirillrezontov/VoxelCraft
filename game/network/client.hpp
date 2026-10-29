#pragma once
#include "game/network/protocol.hpp"
#include <memory>

namespace vc::netgame {
// CPU network session: server-owned terrain/state mirrored for collision queries
// and rendering. Client input contains intent, never an authoritative position.
class RemoteSession {
public:
    RemoteSession(std::string host,std::uint16_t port,std::string name,std::uint64_t fingerprint,voxel::BlockRegistry& blocks,std::string identityToken={});
    ~RemoteSession();
    RemoteSession(const RemoteSession&)=delete;
    RemoteSession& operator=(const RemoteSession&)=delete;
    void pump();
    void input(Input intent); // Assigns a monotonic sequence before queuing.
    void hotbar(const Hotbar& hotbar);
    void inventory(const InventoryAction& action);
    void requestSave();
    void disconnect() noexcept;
    std::uint64_t sentBytes() const;
    std::uint64_t receivedBytes() const;
    bool open() const;
    const Welcome* welcome() const;
    const State& state() const;
    const Camera& camera() const;
    bool flying() const;
    const voxel::World& world() const;
    std::vector<voxel::Position> takeDirtyChunks();
    std::vector<std::string> takeNotices();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
