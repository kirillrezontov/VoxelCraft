#pragma once
#include "engine/core/resource_types.hpp"
#include <glm/vec2.hpp>
#include <cstdint>
#include <vector>

namespace vc {
struct ImageRgba {
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> pixels;
    void validate() const;
};

// CPU-only description shared by content creation and meshing. Gutters prevent
// sampling an adjacent material at a cube edge with the nearest-filter sampler.
class AtlasLayout {
public:
    AtlasLayout(std::uint32_t tileSize, std::uint32_t columns, std::uint32_t rows, std::uint32_t gutter = 1);
    std::uint32_t width() const { return columns_ * stride(); }
    std::uint32_t height() const { return rows_ * stride(); }
    std::uint32_t tileCount() const { return columns_ * rows_; }
    std::uint32_t tileSize() const { return tileSize_; }
    std::uint32_t gutter() const { return gutter_; }
    std::uint32_t stride() const { return tileSize_ + 2 * gutter_; }
    glm::uvec2 tileOrigin(TextureTile tile) const;
    glm::vec2 uv(TextureTile tile, glm::vec2 faceUv) const;
private:
    std::uint32_t tileSize_, columns_, rows_, gutter_;
};
}
