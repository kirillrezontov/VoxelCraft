#pragma once
#include "engine/voxel/storage.hpp"
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>

namespace vc::voxel {
struct SnapshotResult {
    std::uint64_t revision{};
    std::exception_ptr failure;
    void rethrowIfFailed() const { if(failure) std::rethrow_exception(failure); }
};
// One accepted snapshot, including a completed result, bounds retained memory.
// The caller owns/captures World on its thread. Only the worker touches storage
// until the result is taken. Store must outlive this owner; destruction completes
// an accepted save before releasing the worker. No game/Vulkan references enter.
class SnapshotWriter {
public:
    explicit SnapshotWriter(WorldStore& store);
    ~SnapshotWriter()=default;
    SnapshotWriter(const SnapshotWriter&)=delete;
    SnapshotWriter& operator=(const SnapshotWriter&)=delete;
    bool request(World world,BlockRegistry registry,WorldMetadata metadata,std::string gameData={});
    bool busy() const;
    std::optional<SnapshotResult> takeReady();
    SnapshotResult wait(); // Explicit headless/shutdown use; UI can poll instead.
private:
    struct Job { World world;BlockRegistry registry;WorldMetadata metadata;std::string gameData; };
    void run(std::stop_token stop);
    WorldStore& store_;
    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::optional<Job> request_;
    std::optional<SnapshotResult> ready_;
    bool busy_{};
    std::jthread thread_;
};
}
