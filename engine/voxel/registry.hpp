#pragma once
#include <glm/vec3.hpp>
#include "engine/renderer/texture.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vc::voxel {
using BlockId = std::uint16_t;
inline constexpr BlockId air = 0;
struct BlockDefinition {
    std::string identifier;
    bool opaque{};
    glm::vec3 color{1};
    // Cube face order: +X, -X, +Y, -Y, +Z, -Z. Zero is the atlas white tile.
    std::array<TextureTile, 6> faceTextures{};
    bool solid{true}; // Independent of opacity: future transparent cubes can still collide.
    bool fluid{};
    std::uint8_t emission{};
    float opacity{1};
};
class BlockRegistry {
public:
    BlockRegistry();
    BlockId add(BlockDefinition definition);
    std::optional<BlockId> find(std::string_view name) const;
    BlockId require(std::string_view name) const;
    const BlockDefinition& definition(BlockId id) const;
    std::size_t size() const { return definitions_.size(); }
    static bool validIdentifier(std::string_view identifier);
private:
    std::vector<BlockDefinition> definitions_;
    std::map<std::string, BlockId, std::less<>> ids_;
};
}
