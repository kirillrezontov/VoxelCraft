#pragma once
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace vc::voxel {
namespace mesh_detail {
inline constexpr std::array<LocalPosition,6> directions{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
}
template<ChunkMesher Mesher>
BasicMeshWorker<Mesher>::BasicMeshWorker(BlockRegistry registry,AtlasLayout atlas,std::size_t capacity)
    :registry_(std::move(registry)),atlas_(atlas),capacity_(capacity) {
    if(!capacity||capacity>4) throw std::invalid_argument("Mesh worker capacity must be 1..4");
    thread_=std::jthread([this](std::stop_token stop){run(stop);});
}
template<ChunkMesher Mesher>
bool BasicMeshWorker<Mesher>::request(const World& world,Position position,std::uint64_t ticket,const Lighting* lighting) {
    (void)chunkOrigin(position);if(!ticket) throw std::invalid_argument("Mesh job ticket must be nonzero");
    std::lock_guard lock(mutex_);if(failure_) std::rethrow_exception(failure_);
    if(pending_.size()>=capacity_||pending_.contains(position)||!world.findChunk(position)) return false;
    Job job{position,ticket,World(world.bounds()),std::nullopt};job.world.insert(*world.findChunk(position));
    for(const auto direction:mesh_detail::directions) if(const auto adjacent=offsetPosition(position,direction))
        if(const auto* chunk=world.findChunk(*adjacent)) job.world.insert(*chunk);
    if(lighting) job.lighting=lighting->snapshotForMesh(position);
    pending_.insert(position);
    try { requests_.push_back(std::move(job)); } catch(...) { pending_.erase(position);throw; }
    wake_.notify_one();return true;
}
template<ChunkMesher Mesher>
std::optional<CompletedMesh> BasicMeshWorker<Mesher>::takeReady() {
    std::lock_guard lock(mutex_);if(failure_) std::rethrow_exception(failure_);
    if(ready_.empty()) return {};
    auto result=std::move(ready_.front());ready_.pop_front();pending_.erase(result.position);return result;
}
template<ChunkMesher Mesher>
std::size_t BasicMeshWorker<Mesher>::pendingCount() const { std::lock_guard lock(mutex_);return pending_.size(); }
template<ChunkMesher Mesher>
WorkMetrics BasicMeshWorker<Mesher>::metrics() const { std::lock_guard lock(mutex_);return metrics_; }
template<ChunkMesher Mesher>
void BasicMeshWorker<Mesher>::run(std::stop_token stop) {
    try {
        while(!stop.stop_requested()) {
            std::optional<Job> job;
            {
                std::unique_lock lock(mutex_);if(!wake_.wait(lock,stop,[this]{return !requests_.empty();})) return;
                job.emplace(std::move(requests_.front()));requests_.pop_front();
            }
            const auto started=std::chrono::steady_clock::now();
            auto mesh=Mesher::build(*job->world.findChunk(job->position),job->world,registry_,atlas_,job->lighting?&*job->lighting:nullptr);
            if(stop.stop_requested()) return;
            std::lock_guard lock(mutex_);ready_.push_back({job->position,job->ticket,std::move(mesh)});metrics_.record(started);
        }
    } catch(...) { std::lock_guard lock(mutex_);failure_=std::current_exception(); }
}
template<ChunkMesher Mesher>
BasicBackgroundMeshQueue<Mesher>::BasicBackgroundMeshQueue(const BlockRegistry& registry,const AtlasLayout& atlas,std::size_t capacity)
    :worker_(registry,atlas,capacity) {}
template<ChunkMesher Mesher>
void BasicBackgroundMeshQueue<Mesher>::collect(World& world) { invalidate(world.takeDirtyChunks()); }
template<ChunkMesher Mesher>
void BasicBackgroundMeshQueue<Mesher>::invalidate(std::span<const Position> chunks) {
    for(const auto p:chunks) {
        (void)chunkOrigin(p);
        if(nextTicket_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Mesh job ticket exhausted");
        tickets_[p]=++nextTicket_;pending_.insert(p);
    }
}
template<ChunkMesher Mesher>
std::size_t BasicBackgroundMeshQueue<Mesher>::pendingCount() const {
    auto count=pending_.size();for(const auto& [p,ticket]:inFlight_) { (void)ticket;if(!pending_.contains(p)) ++count; }return count;
}
template<ChunkMesher Mesher>
std::vector<Mesh> BasicBackgroundMeshQueue<Mesher>::update(const World& world,Position player,std::size_t budget,const Lighting* lighting) {
    if(!budget||budget>128) throw std::invalid_argument("Mesh snapshot budget must be 1..128");
    std::vector<Mesh> result;
    while(auto ready=worker_.takeReady()) {
        inFlight_.erase(ready->position);
        const auto current=tickets_.find(ready->position);
        if(current!=tickets_.end()&&current->second==ready->ticket&&world.findChunk(ready->position)) {
            result.push_back(std::move(ready->mesh));tickets_.erase(current);
        }
    }
    const auto center=chunkPosition(player);
    const auto distance=[&](Position p) {
        const auto x=static_cast<double>(p.x)-static_cast<double>(center.x),y=static_cast<double>(p.y)-static_cast<double>(center.y),
            z=static_cast<double>(p.z)-static_cast<double>(center.z);return x*x+y*y+z*z;
    };
    std::vector<Position> ordered(pending_.begin(),pending_.end());
    std::sort(ordered.begin(),ordered.end(),[&](Position a,Position b){const auto da=distance(a),db=distance(b);return da==db?a<b:da<db;});
    std::size_t submitted=0;
    for(const auto p:ordered) {
        if(!world.findChunk(p)) {
            const auto origin=chunkOrigin(p);Mesh empty;empty.origin={origin.x,origin.y,origin.z};result.push_back(std::move(empty));
            pending_.erase(p);tickets_.erase(p);continue;
        }
        if(submitted>=budget||inFlight_.contains(p)) continue;
        const auto ticket=tickets_.at(p);
        if(!worker_.request(world,p,ticket,lighting)) continue;
        inFlight_[p]=ticket;pending_.erase(p);++submitted;
    }
    return result;
}
}
