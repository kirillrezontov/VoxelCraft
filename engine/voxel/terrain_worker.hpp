#pragma once
#include "engine/core/work_metrics.hpp"
#include "engine/voxel/generation.hpp"
#include <condition_variable>
#include <concepts>
#include <stdexcept>
#include <utility>
#include <deque>
#include <exception>
#include <mutex>
#include <optional>
#include <set>
#include <thread>

namespace vc::voxel {
// A generator is owned by value and called only on the worker thread. It returns
// the requested chunk (including its position), without borrowing mutable World.
template<class Generator>
concept ChunkGenerator = std::move_constructible<Generator> && requires(const Generator& generator, Position p) {
    { generator.generate(p) } -> std::same_as<Chunk>;
};
// One bounded CPU worker. World ownership and all Vulkan work stay on the caller.
template<ChunkGenerator Generator>
class BasicTerrainWorker {
public:
    explicit BasicTerrainWorker(Generator generator, std::size_t capacity = 8);
    ~BasicTerrainWorker() = default;
    BasicTerrainWorker(const BasicTerrainWorker&) = delete;
    BasicTerrainWorker& operator=(const BasicTerrainWorker&) = delete;
    // False for duplicates or a full queue; capacity includes ready/in-flight work.
    bool request(Position chunk);
    std::optional<Chunk> takeReady();
    std::size_t pendingCount() const;
    WorkMetrics metrics() const;
private:
    void run(std::stop_token stop);
    Generator generator_;
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<Position> requests_;
    std::deque<Chunk> ready_;
    std::set<Position> pending_;
    std::exception_ptr failure_;
    WorkMetrics metrics_;
    // Destroy first: request stop, wake via stop-aware wait, and join before data.
    std::jthread thread_;
};
// Existing game composition keeps the deterministic versioned generator.
using TerrainWorker = BasicTerrainWorker<TerrainGenerator>;
extern template class BasicTerrainWorker<TerrainGenerator>;
}
#include "engine/voxel/terrain_worker.inl"
