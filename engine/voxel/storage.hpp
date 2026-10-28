#pragma once
#include "engine/voxel/world.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace vc { class FileLock; }
namespace vc::voxel {
inline constexpr std::uint32_t worldFormatVersion=1, terrainGenerationVersion=3;
struct WorldMetadata {
    std::uint64_t seed{12345};
    WorldBounds bounds;
    std::uint32_t generatorVersion{terrainGenerationVersion};
};
struct WorldArchive {
    WorldMetadata metadata;
    World world;
    // Versioned game-owned player/inventory data, bounded to 64 KiB.
    std::string gameData;
    std::uint64_t revision{};
};
// Immutable checksummed snapshots contain 4x4x4-chunk region files. A directory
// rename commits the entire world at once and preserves the previous snapshot.
// An OS-owned lock prevents simultaneous writers and is released after a crash.
class WorldStore {
public:
    explicit WorldStore(std::filesystem::path directory);
    ~WorldStore();
    WorldStore(const WorldStore&)=delete;
    WorldStore& operator=(const WorldStore&)=delete;
    const std::filesystem::path& directory() const { return directory_; }
    std::optional<WorldArchive> load(BlockRegistry& registry) const;
    std::uint64_t save(const World& world,const BlockRegistry& registry,
        const WorldMetadata& metadata,std::string_view gameData={});
private:
    std::filesystem::path directory_;
    std::unique_ptr<FileLock> lock_;
};
Position regionPosition(Position chunk);
}
