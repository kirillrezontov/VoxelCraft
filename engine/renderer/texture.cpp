#include "engine/renderer/texture.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace vc {
void ImageRgba::validate() const {
    if (!width || !height || static_cast<std::size_t>(width) > std::numeric_limits<std::size_t>::max() / height / 4)
        throw std::invalid_argument("Invalid RGBA image dimensions");
    if (pixels.size() != static_cast<std::size_t>(width) * height * 4)
        throw std::invalid_argument("RGBA image data does not match its dimensions");
}
AtlasLayout::AtlasLayout(std::uint32_t tileSize, std::uint32_t columns, std::uint32_t rows, std::uint32_t gutter)
    : tileSize_(tileSize), columns_(columns), rows_(rows), gutter_(gutter) {
    // Bound CPU content allocation; GPU-specific dimension checks belong to the renderer.
    if (!tileSize || !columns || !rows || !gutter || tileSize > 4096 || gutter > 4096 ||
        static_cast<std::uint64_t>(columns) * rows > 65536 ||
        static_cast<std::uint64_t>(tileSize + 2 * gutter) * columns > 4096 ||
        static_cast<std::uint64_t>(tileSize + 2 * gutter) * rows > 4096)
        throw std::invalid_argument("Invalid atlas layout (maximum extent 4096, maximum 65536 tiles)");
}
glm::uvec2 AtlasLayout::tileOrigin(TextureTile tile) const {
    if (tile >= tileCount()) throw std::out_of_range("Texture tile outside atlas");
    return {static_cast<std::uint32_t>(tile) % columns_ * stride() + gutter_,
        static_cast<std::uint32_t>(tile) / columns_ * stride() + gutter_};
}
glm::vec2 AtlasLayout::uv(TextureTile tile, glm::vec2 faceUv) const {
    if (!std::isfinite(faceUv.x) || !std::isfinite(faceUv.y) || faceUv.x < 0 || faceUv.x > 1 || faceUv.y < 0 || faceUv.y > 1)
        throw std::invalid_argument("Face UV must be finite and inside [0,1]");
    return (glm::vec2(tileOrigin(tile)) + faceUv * static_cast<float>(tileSize_)) / glm::vec2(width(), height());
}
}
