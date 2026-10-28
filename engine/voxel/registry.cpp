#include "engine/voxel/registry.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <cmath>

namespace vc::voxel {
BlockRegistry::BlockRegistry() { add({"voxelcraft:air", false, {0, 0, 0}, {}, false}); }
bool BlockRegistry::validIdentifier(std::string_view name) {
    const auto colon = name.find(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 1 == name.size()) return false;
    const auto valid = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'; };
    return std::all_of(name.begin(), name.begin() + static_cast<std::ptrdiff_t>(colon), valid) &&
        std::all_of(name.begin() + static_cast<std::ptrdiff_t>(colon + 1), name.end(), [&](char c) { return valid(c) || c == '/'; });
}
BlockId BlockRegistry::add(BlockDefinition definition) {
    if(!std::isfinite(definition.opacity)||definition.opacity<=0||definition.opacity>1||definition.emission>15||
        (definition.opacity<1&&definition.opaque)) throw std::invalid_argument("Invalid block opacity/light definition");
    if (!validIdentifier(definition.identifier)) throw std::invalid_argument("Invalid block identifier: " + definition.identifier);
    if (ids_.contains(definition.identifier)) throw std::invalid_argument("Duplicate block identifier: " + definition.identifier);
    if (definitions_.size() > std::numeric_limits<BlockId>::max()) throw std::overflow_error("Block registry is full");
    const auto id = static_cast<BlockId>(definitions_.size());
    definitions_.push_back(std::move(definition));
    try { ids_.emplace(definitions_.back().identifier, id); }
    catch (...) { definitions_.pop_back(); throw; }
    return id;
}
std::optional<BlockId> BlockRegistry::find(std::string_view name) const {
    auto it = ids_.find(name); return it == ids_.end() ? std::nullopt : std::optional(it->second);
}
BlockId BlockRegistry::require(std::string_view name) const {
    const auto id = find(name);
    if (!id) throw std::invalid_argument("Unknown block identifier: " + std::string(name));
    return *id;
}
const BlockDefinition& BlockRegistry::definition(BlockId id) const { return definitions_.at(id); }
}
