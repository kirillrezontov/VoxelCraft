#pragma once
#include "engine/voxel/mesher.hpp"
#include "engine/voxel/lighting.hpp"
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <optional>
#include <thread>

namespace vc::voxel {
struct CompletedMesh { Position position;std::uint64_t ticket{};Mesh mesh; };
// Single producer owns World/Lighting. Jobs copy only the center and six face
// neighbors plus relevant light columns. No world references cross threads.
class MeshWorker {
public:
    MeshWorker(BlockRegistry registry,AtlasLayout atlas,std::size_t capacity=4);
    ~MeshWorker()=default;
    MeshWorker(const MeshWorker&)=delete;
    MeshWorker& operator=(const MeshWorker&)=delete;
    bool request(const World& world,Position chunk,std::uint64_t ticket,const Lighting* lighting=nullptr);
    std::optional<CompletedMesh> takeReady();
    std::size_t pendingCount() const;
private:
    struct Job { Position position;std::uint64_t ticket;World world;std::optional<Lighting> lighting; };
    void run(std::stop_token stop);
    BlockRegistry registry_;
    AtlasLayout atlas_;
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<Job> requests_;
    std::deque<CompletedMesh> ready_;
    std::set<Position> pending_;
    std::exception_ptr failure_;
    std::jthread thread_; // Stops/joins before the queues and immutable content.
};

// Main-thread coordinator: near-first snapshots, version-checked results, and
// immediate GPU removals. Invalidations supersede queued/in-flight geometry.
class BackgroundMeshQueue {
public:
    BackgroundMeshQueue(const BlockRegistry& registry,const AtlasLayout& atlas,std::size_t capacity=4);
    void collect(World& world);
    void invalidate(std::span<const Position> chunks);
    std::vector<Mesh> update(const World& world,Position player,std::size_t budget=2,const Lighting* lighting=nullptr);
    std::size_t pendingCount() const;
private:
    std::set<Position> pending_;
    std::map<Position,std::uint64_t> tickets_,inFlight_;
    std::uint64_t nextTicket_{};
    MeshWorker worker_;
};
}
