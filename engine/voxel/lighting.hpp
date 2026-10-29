#pragma once
#include "engine/voxel/world.hpp"
#include <span>

namespace vc::voxel {
// Derived CPU data, never saved: direct overhead sky visibility and bounded
// voxel-propagated block light. Main-thread ownership matches World/meshing.
class Lighting {
public:
    std::vector<Position> refresh(const World& world,const BlockRegistry& registry,std::span<const Position> changed);
    float sky(Position cell) const;
    float block(Position cell) const;
    // Immutable face-neighborhood data for one mesh job. Does not copy emitters
    // or unrelated resident lighting, and is safe to read on a worker.
    Lighting snapshotForMesh(Position chunk) const;
private:
    struct SkyCell { std::int64_t top{};bool present{};bool operator==(const SkyCell&) const=default; };
    using SkyColumn=std::array<SkyCell,chunkSize*chunkSize>;
    struct Source { Position position;std::uint8_t level;bool operator==(const Source&) const=default; };
    struct BlockLight { std::array<std::uint8_t,chunkVolume> levels{};bool operator==(const BlockLight&) const=default; };
    std::map<Position,SkyColumn> sky_;
    std::map<Position,std::vector<Source>> sources_;
    std::map<Position,BlockLight> blocks_;
};
}
