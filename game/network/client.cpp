#include "game/network/client.hpp"
#include <chrono>
#include <map>
#include <utility>
#include <stdexcept>

namespace vc::netgame {
struct RemoteSession::Impl {
    voxel::BlockRegistry& blocks;
    net::FramedConnection channel;
    std::string name;
    std::optional<Welcome> joined;
    State latest;
    voxel::World world;
    std::vector<voxel::BlockId> mapping;
    std::map<voxel::Position,std::uint64_t> revisions;
    std::vector<std::string> notices;
    std::uint64_t sequence{};
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now(),lastReceived=started;
    Impl(std::string host,std::uint16_t port,std::string player,std::uint64_t fingerprint,voxel::BlockRegistry& registry,std::string identityToken)
        :blocks(registry),channel(net::TcpStream::connect(host,port)),name(std::move(player)) {
        if(!channel.queue(encode(Hello{fingerprint,name,std::move(identityToken)}))) throw std::runtime_error("Cannot queue multiplayer handshake");
    }
    void queue(const binary::Bytes& message) {
        if(!joined||!channel.open()) throw std::runtime_error("Multiplayer session is not connected");
        if(!channel.queue(message)) throw std::runtime_error("Client command queue exceeded");
    }
    void receive(const binary::Bytes& message) {
        lastReceived=std::chrono::steady_clock::now();
        const auto type=messageType(message);
        if(type==Message::Reject) throw std::runtime_error("Server rejected connection: "+decodeText(message,type));
        if(type==Message::Welcome) {
            if(joined) throw std::runtime_error("Duplicate server welcome");
            auto welcome=decodeWelcome(message);
            if(playerNameKey(welcome.profile.name)!=playerNameKey(name)) throw std::runtime_error("Server returned another player's profile");
            auto resolved=blocks;std::vector<voxel::BlockId> palette;palette.reserve(welcome.blocks.size());
            for(const auto& key:welcome.blocks) {
                auto id=resolved.find(key);if(!id) id=resolved.add({key,true,{1,0,1},{},true});palette.push_back(*id);
            }
            blocks=std::move(resolved);mapping=std::move(palette);world=voxel::World(welcome.metadata.bounds);
            latest.tick=welcome.tick;latest.worldTime=welcome.worldTime;joined=std::move(welcome);return;
        }
        if(!joined) throw std::runtime_error("Received gameplay data before server welcome");
        switch(type) {
        case Message::Chunk: {
            auto update=decodeChunk(message,mapping);const auto p=update.chunk.position();const auto known=revisions.find(p);
            if(known==revisions.end()&&revisions.size()>=2048) throw std::runtime_error("Server exceeded client chunk residency limit");
            if(known==revisions.end()||known->second<update.revision) { world.insert(std::move(update.chunk));revisions[p]=update.revision; }
            break;
        }
        case Message::Unload: { const auto p=decodeUnload(message);(void)world.remove(p);revisions.erase(p);break; }
        case Message::State: {
            auto state=decodeState(message);if(state.tick<latest.tick) throw std::runtime_error("Server state tick moved backward");
            if(state.survival.has_value()!=joined->survival) throw std::runtime_error("Server state game mode changed");
            bool own=false;for(const auto& peer:state.players) if(peer.id==joined->id) own=true;
            if(!own) throw std::runtime_error("Server state omitted the connected player");
            latest=std::move(state);break;
        }
        case Message::Goodbye: {
            const auto reason=decodeText(message,type);channel.close();throw std::runtime_error("Server disconnected: "+reason);
        }
        case Message::Notice:
            if(notices.size()<64) notices.push_back(decodeText(message,type));else (void)decodeText(message,type);
            break;
        default: throw std::runtime_error("Server sent an unexpected gameplay packet");
        }
    }
    const Peer* self() const {
        if(!joined) return nullptr;
        for(const auto& peer:latest.players) if(peer.id==joined->id) return &peer;
        return nullptr;
    }
    void pump() {
        channel.pump();while(auto message=channel.take()) receive(*message);
        if(!joined&&channel.open()&&std::chrono::steady_clock::now()-started>std::chrono::seconds(30))
            throw std::runtime_error("Server welcome timed out");
        if(joined&&channel.open()&&std::chrono::steady_clock::now()-lastReceived>std::chrono::seconds(15)) {
            channel.close();throw std::runtime_error("Server stopped responding for 15 seconds");
        }
    }
};
RemoteSession::RemoteSession(std::string host,std::uint16_t port,std::string name,std::uint64_t fingerprint,voxel::BlockRegistry& blocks,std::string identityToken)
    :impl_(std::make_unique<Impl>(std::move(host),port,std::move(name),fingerprint,blocks,std::move(identityToken))) {}
RemoteSession::~RemoteSession()=default;
void RemoteSession::pump() { impl_->pump(); }
void RemoteSession::input(Input intent) { intent.sequence=++impl_->sequence;impl_->queue(encode(intent)); }
void RemoteSession::hotbar(const Hotbar& hotbar) { impl_->queue(encode(hotbar)); }
void RemoteSession::inventory(const InventoryAction& action) { impl_->queue(encode(action)); }
void RemoteSession::requestSave() { impl_->queue(encodeControl(Message::Save)); }
void RemoteSession::disconnect() noexcept {
    try { if(impl_->channel.open()&&impl_->joined) { (void)impl_->channel.queue(encodeControl(Message::Goodbye));impl_->channel.pump(); } } catch(...) {}
    impl_->channel.close();
}
std::uint64_t RemoteSession::sentBytes() const { return impl_->channel.sentBytes(); }
std::uint64_t RemoteSession::receivedBytes() const { return impl_->channel.receivedBytes(); }
bool RemoteSession::open() const { return impl_->channel.open(); }
const Welcome* RemoteSession::welcome() const { return impl_->joined?&*impl_->joined:nullptr; }
const State& RemoteSession::state() const { return impl_->latest; }
const Camera& RemoteSession::camera() const {
    if(const auto* self=impl_->self()) return self->camera;
    if(impl_->joined) return impl_->joined->profile.camera;
    throw std::logic_error("Server camera is not available before welcome");
}
bool RemoteSession::flying() const {
    if(const auto* self=impl_->self()) return self->flying;
    return impl_->joined&&impl_->joined->profile.flying;
}
const voxel::World& RemoteSession::world() const { return impl_->world; }
std::vector<voxel::Position> RemoteSession::takeDirtyChunks() { return impl_->world.takeDirtyChunks(); }
std::vector<std::string> RemoteSession::takeNotices() { return std::exchange(impl_->notices,{}); }
}
