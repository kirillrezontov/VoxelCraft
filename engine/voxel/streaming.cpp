#include "engine/voxel/streaming.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace vc::voxel {
namespace {
LocalPosition local(std::size_t index) { return {static_cast<int>(index%32),static_cast<int>(index/1024),static_cast<int>((index/32)%32)}; }
Position regionCenter(Position player,WorldBounds bounds) {
    auto center=chunkPosition(player);center.y=std::clamp(center.y,chunkAxis(bounds.minY),chunkAxis(bounds.maxY));return center;
}
}
PackedChunk::PackedChunk(const Chunk& chunk):position_(chunk.position()) {
    for(std::size_t i=0;i<chunkVolume;++i) {
        const auto block=chunk.get(local(i));
        if(!runs_.empty()&&runs_.back().block==block) ++runs_.back().length;
        else runs_.push_back({block,1});
    }
    if(runs_.size()*sizeof(Run)>chunkVolume*sizeof(BlockId)) {
        raw_.reserve(chunkVolume);
        for(std::size_t i=0;i<chunkVolume;++i) raw_.push_back(chunk.get(local(i)));
        std::vector<Run>{}.swap(runs_);
    }
}
Chunk PackedChunk::unpack() const {
    Chunk chunk(position_);
    if(!raw_.empty()) {
        for(std::size_t i=0;i<raw_.size();++i) chunk.set(local(i),raw_[i]);
    } else {
        std::size_t index=0;
        for(const auto& run:runs_) for(std::uint16_t i=0;i<run.length;++i) chunk.set(local(index++),run.block);
    }
    return chunk;
}
std::vector<Position> streamingRegion(Position player,WorldBounds bounds,int radius,std::uint32_t generationVersion) {
    bounds.validate();
    if(radius<1||radius>6) throw std::invalid_argument("Streaming radius must be 1..6 chunks");
    const auto center=regionCenter(player,bounds);
    std::set<std::int64_t> levels;
    const auto minChunk=chunkAxis(bounds.minY),maxChunk=chunkAxis(bounds.maxY);
    const auto low=chunkAxis(std::clamp<std::int64_t>(-32,bounds.minY,bounds.maxY));
    const auto high=chunkAxis(std::clamp<std::int64_t>(generationVersion>=3?159:63,bounds.minY,bounds.maxY));
    for(auto y=low;y<=high;++y) levels.insert(y);
    for(int offset=-1;offset<=1;++offset) {
        const auto y=center.y+offset;
        if(y>=minChunk&&y<=maxChunk) levels.insert(y);
    }
    std::vector<Position> result;
    constexpr auto minimum=std::numeric_limits<std::int64_t>::min()/chunkSize;
    constexpr auto maximum=std::numeric_limits<std::int64_t>::max()/chunkSize;
    for(int z=-radius;z<=radius;++z) for(int x=-radius;x<=radius;++x) {
        if(center.x+x<minimum||center.x+x>maximum||center.z+z<minimum||center.z+z>maximum) continue;
        for(const auto y:levels) result.push_back({center.x+x,y,center.z+z});
    }
    const auto distance=[&](Position p) {
        const auto x=static_cast<double>(p.x)-static_cast<double>(center.x),y=static_cast<double>(p.y)-static_cast<double>(center.y),
            z=static_cast<double>(p.z)-static_cast<double>(center.z);
        return x*x+y*y+z*z;
    };
    std::sort(result.begin(),result.end(),[&](Position a,Position b){
        const auto da=distance(a),db=distance(b);return da==db?a<b:da<db;
    });
    return result;
}
ChunkStreamer::ChunkStreamer(World& world,TerrainGenerator generator,int radius)
    :world_(world),radius_(radius),generationVersion_(generator.version()),worker_(std::move(generator)) {
    if(radius<1||radius>6) throw std::invalid_argument("Streaming radius must be 1..6 chunks");
}
void ChunkStreamer::retain(std::span<const Position> chunks) {
    for(const auto p:chunks) if(const auto* chunk=world_.findChunk(p)) retained_.insert_or_assign(p,PackedChunk(*chunk));
}
void ChunkStreamer::tick(Position player,std::size_t integrationBudget) {
    if(!integrationBudget||integrationBudget>128) throw std::invalid_argument("Streaming integration budget must be 1..128");
    const auto center=regionCenter(player,world_.bounds());
    if(!center_||*center_!=center) {
        ordered_=streamingRegion(player,world_.bounds(),radius_,generationVersion_);wanted_={ordered_.begin(),ordered_.end()};center_=center;
        std::vector<Position> evict;
        for(const auto& [p,chunk]:world_.chunks()) {
            if(wanted_.contains(p)) continue;
            if(world_.modified(p)||retained_.contains(p)) retained_.insert_or_assign(p,PackedChunk(chunk));
            evict.push_back(p);
        }
        for(const auto p:evict) (void)world_.remove(p);
    }
    std::size_t integrated=0;
    while(integrated<integrationBudget) {
        auto chunk=worker_.takeReady();if(!chunk) break;
        const auto p=chunk->position();
        if(wanted_.contains(p)&&!world_.findChunk(p)) {
            const auto retained=retained_.find(p);
            world_.insert(retained==retained_.end()?std::move(*chunk):retained->second.unpack());++integrated;
        }
    }
    for(const auto p:ordered_) {
        if(world_.findChunk(p)) continue;
        const auto retained=retained_.find(p);
        if(retained!=retained_.end()) {
            if(integrated<integrationBudget) { world_.insert(retained->second.unpack());++integrated; }
        } else if(worker_.pendingCount()<8) (void)worker_.request(p);
    }
}
World ChunkStreamer::snapshot() const {
    World snapshot(world_.bounds());
    for(const auto& [p,packed]:retained_) { (void)p;snapshot.insert(packed.unpack()); }
    for(const auto& [p,chunk]:world_.chunks())
        if(world_.modified(p)||retained_.contains(p)) snapshot.insert(chunk);
    (void)snapshot.takeDirtyChunks();
    return snapshot;
}
bool ChunkStreamer::settled() const {
    return center_.has_value()&&worker_.pendingCount()==0&&std::all_of(ordered_.begin(),ordered_.end(),[&](Position p){return world_.findChunk(p)!=nullptr;});
}
std::size_t ChunkStreamer::cachedBytes() const {
    std::size_t bytes=0;for(const auto& [p,chunk]:retained_) { (void)p;bytes+=chunk.bytes(); }return bytes;
}
}
