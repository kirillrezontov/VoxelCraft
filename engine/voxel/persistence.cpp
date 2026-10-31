#include "engine/voxel/persistence.hpp"
#include <stdexcept>
#include <utility>

namespace vc::voxel {
WorldPersistence::WorldPersistence(std::filesystem::path directory)
    : store_(std::move(directory)), writer_(store_) {}
std::optional<WorldArchive> WorldPersistence::load(BlockRegistry& registry) const {
    if (saving()) throw std::logic_error("Cannot load while a save result is pending");
    return store_.load(registry);
}
bool WorldPersistence::requestSave(World world, BlockRegistry registry, WorldMetadata metadata, std::string gameData) {
    return writer_.request(std::move(world), std::move(registry), metadata, std::move(gameData));
}
}
