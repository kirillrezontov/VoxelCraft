#pragma once
#include "engine/voxel/generation.hpp"
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <optional>
#include <set>
#include <thread>

namespace vc::voxel {
// One bounded CPU worker. World ownership and all Vulkan work stay on the caller.
class TerrainWorker {
public:
    explicit TerrainWorker(TerrainGenerator generator, std::size_t capacity = 8);
    ~TerrainWorker() = default;
    TerrainWorker(const TerrainWorker&) = delete;
    TerrainWorker& operator=(const TerrainWorker&) = delete;
    // False for duplicates or a full queue; capacity includes ready/in-flight work.
    bool request(Position chunk);
    std::optional<Chunk> takeReady();
    std::size_t pendingCount() const;
private:
    void run(std::stop_token stop);
    TerrainGenerator generator_;
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::deque<Position> requests_;
    std::deque<Chunk> ready_;
    std::set<Position> pending_;
    std::exception_ptr failure_;
    // Destroy first: request stop, wake via stop-aware wait, and join before data.
    std::jthread thread_;
};
}
