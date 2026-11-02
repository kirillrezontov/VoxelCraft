#include "game/network/protocol.hpp"
#include "game/network/identity.hpp"
#include "engine/core/utf8.hpp"
#include "game/simulation/day_cycle.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace vc::netgame {
namespace {
binary::Writer packet(Message type) { binary::Writer out;out.u32(static_cast<std::uint32_t>(type));return out; }
binary::Reader reader(std::span<const std::uint8_t> bytes,Message expected) {
    if(bytes.size()>net::maxFrameBytes) throw std::runtime_error("Gameplay packet exceeds limit");
    binary::Reader in(bytes);if(in.u32()!=static_cast<std::uint32_t>(expected)) throw std::runtime_error("Unexpected gameplay packet type");return in;
}
void time(double seconds) {
    if(!std::isfinite(seconds)||seconds<0||seconds>=DayCycle::length) throw std::runtime_error("Invalid server world time");
}
void pose(binary::Writer& out,const Camera& camera) {
    validatePlayerPose(camera);for(int axis=0;axis<3;++axis) out.f64(camera.position[axis]);out.f64(camera.yaw);out.f64(camera.pitch);
}
Camera pose(binary::Reader& in) {
    Camera camera;for(int axis=0;axis<3;++axis) camera.position[axis]=in.f64();camera.yaw=in.f64();camera.pitch=in.f64();validatePlayerPose(camera);return camera;
}
void position(binary::Writer& out,voxel::Position p) { (void)voxel::chunkOrigin(p);out.i64(p.x);out.i64(p.y);out.i64(p.z); }
voxel::Position position(binary::Reader& in) { const voxel::Position p{in.i64(),in.i64(),in.i64()};(void)voxel::chunkOrigin(p);return p; }
voxel::LocalPosition local(std::size_t index) { return {static_cast<int>(index%32),static_cast<int>(index/1024),static_cast<int>((index/32)%32)}; }
void validWelcome(const Welcome& message) {
    if(!message.id||message.radius<1||message.radius>6||message.metadata.generatorVersion<1||message.metadata.generatorVersion>voxel::terrainGenerationVersion)
        throw std::runtime_error("Invalid server welcome metadata");
    message.metadata.bounds.validate();
    if(message.metadata.bounds.minY< -1048576||message.metadata.bounds.maxY>1048575) throw std::runtime_error("Server world height exceeds client range");
    time(message.worldTime);validateProfile(message.profile);
    if(message.blocks.empty()||message.blocks.size()>maxNetworkBlocks||message.blocks[0]!="voxelcraft:air") throw std::runtime_error("Invalid server block palette");
    std::set<std::string> names;
    for(const auto& name:message.blocks) if(name.size()>256||!voxel::BlockRegistry::validIdentifier(name)||!names.insert(name).second)
        throw std::runtime_error("Invalid or duplicate server block identifier");
}
void validInput(const Input& input) {
    if(!input.sequence||input.slot>=9||(input.flags&~std::uint32_t{127})||!std::isfinite(input.yaw)||std::abs(input.yaw)>6.283186||
        !std::isfinite(input.pitch)||std::abs(input.pitch)>1.553344) throw std::runtime_error("Invalid player input command");
    for(const auto value:{input.forward,input.right,input.up}) if(!std::isfinite(value)||std::abs(value)>1) throw std::runtime_error("Invalid movement intent");
}
void validState(const State& state) {
    validateHotbar(state.hotbar,state.selectedSlot);
    if(state.permission>commands::Permission::Administrator) throw std::runtime_error("Invalid client permission snapshot");
    time(state.worldTime);if(state.players.size()>maxPlayers) throw std::runtime_error("Too many connected player records");
    std::set<std::uint64_t> ids;std::set<std::string> names;
    for(const auto& peer:state.players) {
        if(!peer.id||!validPlayerName(peer.name)||!ids.insert(peer.id).second||!names.insert(playerNameKey(peer.name)).second)
            throw std::runtime_error("Invalid or duplicate connected player");
        validatePlayerPose(peer.camera);if(peer.health>20) throw std::runtime_error("Invalid player health snapshot");
    }
    validateActors(state.actors);
    if(state.survival) {
        const auto& s=*state.survival;
        if(s.health>20||s.selectedSlot>=9||!std::isfinite(s.miningProgress)||s.miningProgress<0||s.miningProgress>1||(!s.mining&&s.miningProgress!=0))
            throw std::runtime_error("Invalid survival snapshot");
        for(const auto& item:s.items) validateSavedItem(item);
    }
}
void validInventory(const InventoryAction& action) {
    if(action.from>=36||action.to>=36||action.recipe>=8192) throw std::runtime_error("Inventory action exceeds bounds");
    switch(action.kind) {
    case InventoryActionKind::Swap: case InventoryActionKind::TransferOne: case InventoryActionKind::TransferHalf: case InventoryActionKind::TransferAll:
        if(action.recipe) throw std::runtime_error("Unexpected recipe in slot action");
        break;
    case InventoryActionKind::QuickTransfer:
        if(action.to||action.recipe) throw std::runtime_error("Invalid quick transfer action");
        break;
    case InventoryActionKind::MoveToBackpack:
        if(action.from>=9||action.to||action.recipe) throw std::runtime_error("Invalid backpack action");
        break;
    case InventoryActionKind::Craft:
        if(action.from||action.to) throw std::runtime_error("Unexpected slots in craft action");
        break;
    default:throw std::runtime_error("Unknown inventory action kind");
    }
}
void actor(binary::Writer& out,const SavedActor& a) {
    out.u64(a.id);out.text(a.kind);for(int axis=0;axis<3;++axis) out.f64(a.position[axis]);
    for(int axis=0;axis<3;++axis) out.f64(a.velocity[axis]);
    out.f64(a.yaw);out.f64(a.age);out.f64(a.phase);out.u32(a.health);
    out.text(a.item.identifier);out.u16(a.item.count);out.u16(a.item.durability);
}
SavedActor actor(binary::Reader& in) {
    SavedActor a;a.id=in.u64();a.kind=in.text();for(int axis=0;axis<3;++axis) a.position[axis]=in.f64();
    for(int axis=0;axis<3;++axis) a.velocity[axis]=in.f64();
    a.yaw=in.f64();a.age=in.f64();a.phase=in.f64();a.health=in.u32();
    a.item.identifier=in.text();a.item.count=in.u16();a.item.durability=in.u16();return a;
}
}
Message messageType(std::span<const std::uint8_t> bytes) {
    if(bytes.size()>net::maxFrameBytes) throw std::runtime_error("Gameplay packet exceeds limit");
    binary::Reader in(bytes);const auto value=in.u32();
    if(value<1||value>static_cast<std::uint32_t>(Message::CommandReply)) throw std::runtime_error("Unknown gameplay packet type");
    return static_cast<Message>(value);
}
binary::Bytes encode(const CommandRequest& message) {
    if(!message.id||message.text.size()>commands::maxCommandBytes||!utf8::valid(message.text)) throw std::invalid_argument("Invalid command request");
    auto out=packet(Message::Command);out.u64(message.id);out.text(message.text);return out.bytes;
}
CommandRequest decodeCommand(std::span<const std::uint8_t> bytes) {
    if(bytes.size()>commands::maxCommandBytes+16) throw std::runtime_error("Command packet exceeds limit");
    auto in=reader(bytes,Message::Command);CommandRequest message;message.id=in.u64();message.text=in.text();in.finish();
    if(!message.id||message.text.size()>commands::maxCommandBytes||!utf8::valid(message.text)) throw std::runtime_error("Invalid command request");
    return message;
}
binary::Bytes encode(const CommandReply& message) {
    if(!message.id||message.result.code>commands::Code::InternalFailure||message.result.lines.size()>commands::maxResultLines) throw std::invalid_argument("Invalid command result");
    auto out=packet(Message::CommandReply);out.u64(message.id);out.u32(static_cast<std::uint32_t>(message.result.code));out.u32(static_cast<std::uint32_t>(message.result.lines.size()));
    for(const auto& line:message.result.lines) { if(line.size()>commands::maxResultBytes||!utf8::valid(line)) throw std::invalid_argument("Invalid command output");out.text(line); }return out.bytes;
}
CommandReply decodeCommandReply(std::span<const std::uint8_t> bytes) {
    auto in=reader(bytes,Message::CommandReply);CommandReply message;message.id=in.u64();message.result.code=static_cast<commands::Code>(in.u32());
    const auto count=in.u32();if(!message.id||message.result.code>commands::Code::InternalFailure||count>commands::maxResultLines) throw std::runtime_error("Invalid command result");
    for(std::uint32_t i=0;i<count;++i) { auto line=in.text();if(line.size()>commands::maxResultBytes||!utf8::valid(line)) throw std::runtime_error("Invalid command output");message.result.lines.push_back(std::move(line)); }
    in.finish();return message;
}
binary::Bytes encode(const Hello& message) {
    if(!validPlayerName(message.name)) throw std::invalid_argument("Invalid joining player name");
    auto out=packet(Message::Hello);out.raw("VCNET");out.u32(protocolVersion);out.u64(message.fingerprint);out.text(message.name);
    if(!message.identityToken.empty()&&!validIdentityToken(message.identityToken)) throw std::invalid_argument("Invalid identity token format");
    out.text(message.identityToken);return out.bytes;
}
Hello decodeHello(std::span<const std::uint8_t> bytes) {
    auto in=reader(bytes,Message::Hello);in.magic("VCNET");if(in.u32()!=protocolVersion) throw std::runtime_error("Unsupported multiplayer protocol version");
    Hello message;message.fingerprint=in.u64();message.name=in.text();message.identityToken=in.text();in.finish();
    if(!message.identityToken.empty()&&!validIdentityToken(message.identityToken)) throw std::runtime_error("Invalid identity token format");
    if(!validPlayerName(message.name)) throw std::runtime_error("Invalid joining player name");
    return message;
}
binary::Bytes encode(const Welcome& message) {
    validWelcome(message);auto out=packet(Message::Welcome);out.raw("VCNET");out.u32(protocolVersion);out.u64(message.id);out.u64(message.tick);
    out.u64(message.metadata.seed);out.i64(message.metadata.bounds.minY);out.i64(message.metadata.bounds.maxY);out.u32(message.metadata.generatorVersion);
    out.u32(message.radius);out.f64(message.worldTime);writeProfile(out,message.profile);out.u32(message.survival?1:0);out.u32(static_cast<std::uint32_t>(message.blocks.size()));
    for(const auto& name:message.blocks) out.text(name);
    return out.bytes;
}
Welcome decodeWelcome(std::span<const std::uint8_t> bytes) {
    auto in=reader(bytes,Message::Welcome);in.magic("VCNET");if(in.u32()!=protocolVersion) throw std::runtime_error("Unsupported multiplayer protocol version");
    Welcome message;message.id=in.u64();message.tick=in.u64();message.metadata.seed=in.u64();message.metadata.bounds.minY=in.i64();message.metadata.bounds.maxY=in.i64();
    message.metadata.generatorVersion=in.u32();message.radius=in.u32();message.worldTime=in.f64();message.profile=readProfile(in,true,true);
    const auto survival=in.u32();if(survival>1) throw std::runtime_error("Invalid server game mode");message.survival=survival==1;
    const auto count=in.u32();if(!count||count>maxNetworkBlocks) throw std::runtime_error("Server block palette exceeds limit");
    message.blocks.reserve(count);for(std::uint32_t i=0;i<count;++i) message.blocks.push_back(in.text());in.finish();validWelcome(message);return message;
}
binary::Bytes encode(const Input& message) {
    validInput(message);auto out=packet(Message::Input);out.u64(message.sequence);out.f64(message.yaw);out.f64(message.pitch);
    out.f64(message.forward);out.f64(message.right);out.f64(message.up);out.u32(message.flags);out.u32(message.slot);return out.bytes;
}
Input decodeInput(std::span<const std::uint8_t> bytes) {
    auto in=reader(bytes,Message::Input);Input message;message.sequence=in.u64();message.yaw=in.f64();message.pitch=in.f64();
    message.forward=in.f64();message.right=in.f64();message.up=in.f64();message.flags=in.u32();message.slot=in.u32();in.finish();validInput(message);return message;
}
binary::Bytes encode(const Hotbar& message) {
    validateHotbar(message.names,message.slot);auto out=packet(Message::Hotbar);out.u32(message.slot);for(const auto& name:message.names) out.text(name);return out.bytes;
}
Hotbar decodeHotbar(std::span<const std::uint8_t> bytes) {
    auto in=reader(bytes,Message::Hotbar);Hotbar message;message.slot=in.u32();for(auto& name:message.names) name=in.text();in.finish();validateHotbar(message.names,message.slot);return message;
}
binary::Bytes encode(const State& message) {
    validState(message);auto out=packet(Message::State);out.u64(message.tick);out.f64(message.worldTime);out.u32(static_cast<std::uint32_t>(message.players.size()));
    for(const auto& peer:message.players) { out.u64(peer.id);out.text(peer.name);pose(out,peer.camera);out.u32(peer.flying?1:0);out.u32(peer.health); }
    out.u32(static_cast<std::uint32_t>(message.actors.size()));for(const auto& a:message.actors) actor(out,a);
    out.u32(message.survival?1:0);
    if(message.survival) {
        const auto& s=*message.survival;out.u32(s.health);out.u32(s.selectedSlot);out.f64(s.miningProgress);out.u32(s.mining?1:0);
        for(const auto& item:s.items) { out.text(item.identifier);out.u16(item.count);out.u16(item.durability); }
    }
    for(const auto& name:message.hotbar) out.text(name);
    out.u32(message.selectedSlot);out.u64(message.poseRevision);out.u32(static_cast<std::uint32_t>(message.permission));
    return out.bytes;
}
State decodeState(std::span<const std::uint8_t> bytes) {
    auto in=reader(bytes,Message::State);State message;message.tick=in.u64();message.worldTime=in.f64();const auto count=in.u32();
    if(count>maxPlayers) throw std::runtime_error("Too many connected players");
    message.players.reserve(count);
    for(std::uint32_t i=0;i<count;++i) {
        Peer peer;peer.id=in.u64();peer.name=in.text();peer.camera=pose(in);const auto flying=in.u32();if(flying>1) throw std::runtime_error("Invalid network movement mode");
        peer.flying=flying==1;peer.health=in.u32();message.players.push_back(std::move(peer));
    }
    const auto actors=in.u32();if(actors>maxSavedActors) throw std::runtime_error("Too many network actors");
    message.actors.reserve(actors);for(std::uint32_t i=0;i<actors;++i) message.actors.push_back(actor(in));
    const auto survival=in.u32();if(survival>1) throw std::runtime_error("Invalid survival snapshot flag");
    if(survival) {
        auto& s=message.survival.emplace();s.health=in.u32();s.selectedSlot=in.u32();s.miningProgress=in.f64();
        const auto mining=in.u32();if(mining>1) throw std::runtime_error("Invalid mining flag");s.mining=mining==1;
        for(auto& item:s.items) { item.identifier=in.text();item.count=in.u16();item.durability=in.u16(); }
    }
    for(auto& name:message.hotbar) name=in.text();
    message.selectedSlot=in.u32();message.poseRevision=in.u64();message.permission=static_cast<commands::Permission>(in.u32());
    in.finish();validState(message);return message;
}
binary::Bytes encode(const InventoryAction& message) {
    validInventory(message);auto out=packet(Message::Inventory);out.u32(static_cast<std::uint32_t>(message.kind));
    out.u32(message.from);out.u32(message.to);out.u32(message.recipe);return out.bytes;
}
InventoryAction decodeInventory(std::span<const std::uint8_t> bytes) {
    auto in=reader(bytes,Message::Inventory);InventoryAction message;message.kind=static_cast<InventoryActionKind>(in.u32());
    message.from=in.u32();message.to=in.u32();message.recipe=in.u32();in.finish();validInventory(message);return message;
}
binary::Bytes encodeChunk(const voxel::Chunk& chunk,std::uint64_t revision,std::size_t blockCount) {
    if(!revision||!blockCount||blockCount>maxNetworkBlocks) throw std::invalid_argument("Invalid network chunk metadata");
    std::map<voxel::BlockId,std::uint16_t> indices;std::vector<voxel::BlockId> palette;
    struct Run { std::uint16_t index,length; };std::vector<Run> runs;
    for(std::size_t i=0;i<voxel::chunkVolume;++i) {
        const auto block=chunk.get(local(i));if(block>=blockCount) throw std::runtime_error("Chunk references an unknown network block");
        auto [found,added]=indices.try_emplace(block,static_cast<std::uint16_t>(palette.size()));if(added) palette.push_back(block);
        if(!runs.empty()&&runs.back().index==found->second) ++runs.back().length;else runs.push_back({found->second,1});
    }
    auto out=packet(Message::Chunk);position(out,chunk.position());out.u64(revision);out.u16(static_cast<std::uint16_t>(palette.size()));
    for(const auto block:palette) out.u16(block);
    out.u32(static_cast<std::uint32_t>(runs.size()));
    for(const auto& run:runs) { out.u16(run.index);out.u16(run.length); }return out.bytes;
}
ChunkUpdate decodeChunk(std::span<const std::uint8_t> bytes,std::span<const voxel::BlockId> mapping) {
    if(mapping.empty()||mapping.size()>maxNetworkBlocks||mapping[0]!=voxel::air) throw std::invalid_argument("Invalid client block mapping");
    auto in=reader(bytes,Message::Chunk);const auto p=position(in);const auto revision=in.u64();if(!revision) throw std::runtime_error("Invalid chunk revision");
    const auto count=in.u16();if(!count||count>mapping.size()) throw std::runtime_error("Invalid chunk palette size");
    std::vector<voxel::BlockId> palette;palette.reserve(count);std::set<std::uint16_t> seen;
    for(std::uint16_t i=0;i<count;++i) { const auto id=in.u16();if(id>=mapping.size()||!seen.insert(id).second) throw std::runtime_error("Invalid chunk palette index");palette.push_back(mapping[id]); }
    const auto runCount=in.u32();if(!runCount||runCount>voxel::chunkVolume) throw std::runtime_error("Invalid chunk run count");
    voxel::Chunk chunk(p);std::size_t offset=0;
    for(std::uint32_t i=0;i<runCount;++i) {
        const auto index=in.u16(),length=in.u16();if(index>=palette.size()||!length||length>voxel::chunkVolume-offset) throw std::runtime_error("Invalid chunk run");
        for(std::uint16_t n=0;n<length;++n) chunk.set(local(offset++),palette[index]);
    }
    if(offset!=voxel::chunkVolume) throw std::runtime_error("Incomplete chunk payload");
    in.finish();return {std::move(chunk),revision};
}
binary::Bytes encodeUnload(voxel::Position chunk) { auto out=packet(Message::Unload);position(out,chunk);return out.bytes; }
voxel::Position decodeUnload(std::span<const std::uint8_t> bytes) { auto in=reader(bytes,Message::Unload);const auto p=position(in);in.finish();return p; }
binary::Bytes encodeText(Message type,std::string_view text) {
    if((type!=Message::Notice&&type!=Message::Reject&&type!=Message::Goodbye)||text.size()>240) throw std::invalid_argument("Invalid network notice");
    auto out=packet(type);out.text(text);return out.bytes;
}
std::string decodeText(std::span<const std::uint8_t> bytes,Message type) {
    if(type!=Message::Notice&&type!=Message::Reject&&type!=Message::Goodbye) throw std::invalid_argument("Invalid network notice type");
    auto in=reader(bytes,type);auto text=in.text();in.finish();if(text.size()>240) throw std::runtime_error("Network notice exceeds limit");return text;
}
binary::Bytes encodeControl(Message type) {
    if(type!=Message::Save&&type!=Message::Goodbye) throw std::invalid_argument("Invalid network control type");
    return packet(type).bytes;
}
void decodeControl(std::span<const std::uint8_t> bytes,Message type) {
    if(type!=Message::Save&&type!=Message::Goodbye) throw std::invalid_argument("Invalid network control type");
    auto in=reader(bytes,type);in.finish();
}
}
