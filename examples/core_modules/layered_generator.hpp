#pragma once
#include "engine/voxel/world.hpp"

namespace vc::examples {
// Independent compile-time generation policy: no built-in game identifiers,
// virtual calls, callbacks, or allocations per voxel. Intended for new worlds.
class LayeredGenerator {
public:
    LayeredGenerator(voxel::WorldBounds bounds, voxel::BlockId material, std::int64_t surface)
        : bounds_(bounds), material_(material), surface_(surface) { bounds_.validate(); }
    voxel::Chunk generate(voxel::Position position) const {
        voxel::Chunk chunk(position);
        const auto origin = voxel::chunkOrigin(position);
        for (int y = 0; y < voxel::chunkSize; ++y) {
            const auto worldY = origin.y + y;
            if (!bounds_.contains(worldY) || worldY > surface_) continue;
            for (int z = 0; z < voxel::chunkSize; ++z)
                for (int x = 0; x < voxel::chunkSize; ++x) chunk.set({x, y, z}, material_);
        }
        return chunk;
    }
private:
    voxel::WorldBounds bounds_;
    voxel::BlockId material_;
    std::int64_t surface_;
};
}
