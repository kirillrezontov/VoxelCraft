#pragma once
#include "engine/voxel/snapshot_writer.hpp"

namespace vc::voxel {
// Session-owned persistence. Call on the owning thread; submitted snapshots are
// values. The worker finishes accepted writes before the directory lock dies.
class WorldPersistence {
public:
    explicit WorldPersistence(std::filesystem::path directory);
    ~WorldPersistence() = default;
    WorldPersistence(const WorldPersistence&) = delete;
    WorldPersistence& operator=(const WorldPersistence&) = delete;
    const std::filesystem::path& directory() const { return store_.directory(); }
    // Load before gameplay restoration. Reject while a save/result is pending;
    // only the worker may touch storage during that interval.
    std::optional<WorldArchive> load(BlockRegistry& registry) const;
    bool requestSave(World world, BlockRegistry registry, WorldMetadata metadata, std::string gameData = {});
    bool saving() const { return writer_.busy(); }
    std::optional<SnapshotResult> takeSaveResult() { return writer_.takeReady(); }
    SnapshotResult waitForSave() { return writer_.wait(); }
private:
    WorldStore store_;
    SnapshotWriter writer_; // Destroy/join first, including constructor unwinding.
};
}
