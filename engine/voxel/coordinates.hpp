#pragma once
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace vc::voxel {
inline constexpr int chunkSize = 32;
inline constexpr int chunkVolume = chunkSize * chunkSize * chunkSize;
struct Position {
    std::int64_t x{}, y{}, z{};
    auto operator<=>(const Position&) const = default;
};
struct LocalPosition {
    int x{}, y{}, z{};
    auto operator<=>(const LocalPosition&) const = default;
};
constexpr std::optional<Position> offsetPosition(Position p, LocalPosition delta) {
    const auto safe = [](std::int64_t value, int change) {
        return !((change > 0 && value > std::numeric_limits<std::int64_t>::max() - change) ||
            (change < 0 && value < std::numeric_limits<std::int64_t>::min() - change));
    };
    if (!safe(p.x, delta.x) || !safe(p.y, delta.y) || !safe(p.z, delta.z)) return std::nullopt;
    return Position{p.x + delta.x, p.y + delta.y, p.z + delta.z};
}
// Division is mathematical floor, including INT64_MIN; no negation of the dividend.
constexpr std::int64_t chunkAxis(std::int64_t value) {
    return value / chunkSize - (value % chunkSize < 0 ? 1 : 0);
}
constexpr int localAxis(std::int64_t value) {
    const auto remainder = value % chunkSize;
    return static_cast<int>(remainder < 0 ? remainder + chunkSize : remainder);
}
constexpr Position chunkPosition(Position p) { return {chunkAxis(p.x), chunkAxis(p.y), chunkAxis(p.z)}; }
constexpr LocalPosition localPosition(Position p) { return {localAxis(p.x), localAxis(p.y), localAxis(p.z)}; }
constexpr std::int64_t originAxis(std::int64_t chunk) {
    if (chunk < std::numeric_limits<std::int64_t>::min() / chunkSize || chunk > std::numeric_limits<std::int64_t>::max() / chunkSize)
        throw std::out_of_range("Chunk origin exceeds 64-bit world coordinates");
    return chunk * chunkSize;
}
constexpr Position chunkOrigin(Position p) { return {originAxis(p.x), originAxis(p.y), originAxis(p.z)}; }
constexpr std::size_t blockIndex(LocalPosition p) {
    if (p.x < 0 || p.x >= chunkSize || p.y < 0 || p.y >= chunkSize || p.z < 0 || p.z >= chunkSize)
        throw std::out_of_range("Local block coordinate outside 32-cubed chunk");
    return static_cast<std::size_t>(p.x + chunkSize * (p.z + chunkSize * p.y));
}
struct WorldBounds {
    std::int64_t minY{-512}, maxY{511};
    void validate() const { if (minY > maxY) throw std::invalid_argument("World minimum Y exceeds maximum Y"); }
    bool contains(std::int64_t y) const { return y >= minY && y <= maxY; }
};
}
