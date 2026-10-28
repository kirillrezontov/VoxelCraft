#include "engine/voxel/mesher.hpp"
#include "engine/voxel/lighting.hpp"
#include <array>
#include <limits>

namespace vc::voxel {
namespace {
struct Face { LocalPosition neighbor; std::array<glm::vec3, 4> corners; };
const std::array<Face, 6> faces{{
    {{1, 0, 0}, {{{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}}},
    {{-1, 0, 0}, {{{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}}}},
    {{0, 1, 0}, {{{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}}},
    {{0, -1, 0}, {{{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}}},
    {{0, 0, 1}, {{{1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 0, 1}}}},
    {{0, 0, -1}, {{{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}}}
}};
bool representable(std::int64_t origin, int local) {
    return !((local < 0 && origin == std::numeric_limits<std::int64_t>::min()) ||
        (local >= chunkSize && origin > std::numeric_limits<std::int64_t>::max() - chunkSize));
}
}
Mesh meshChunk(const Chunk& chunk,const World& world,const BlockRegistry& registry,const AtlasLayout& atlas,const Lighting* lighting) {
    Mesh mesh;
    const auto origin = chunkOrigin(chunk.position()); mesh.origin = {origin.x, origin.y, origin.z};
    const auto neighbor = [&](LocalPosition p) -> BlockId {
        if (!representable(origin.x, p.x) || !representable(origin.y, p.y) || !representable(origin.z, p.z)) return air;
        if (!world.bounds().contains(origin.y + p.y)) return air;
        if (p.x >= 0 && p.x < chunkSize && p.y >= 0 && p.y < chunkSize && p.z >= 0 && p.z < chunkSize) return chunk.get(p);
        return world.block({origin.x + p.x, origin.y + p.y, origin.z + p.z});
    };
    for (int y = 0; y < chunkSize; ++y) {
        if (!world.bounds().contains(origin.y + y)) continue;
        for (int z = 0; z < chunkSize; ++z) for (int x = 0; x < chunkSize; ++x) {
            const auto id = chunk.get({x, y, z});
            if (id == air) continue;
            const auto& block = registry.definition(id);
            for (std::size_t side = 0; side < faces.size(); ++side) {
                const auto& face = faces[side];
                const auto adjacent=neighbor({x+face.neighbor.x,y+face.neighbor.y,z+face.neighbor.z});
                if (registry.definition(adjacent).opaque||(block.opacity<1&&adjacent==id)) continue;
                const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
                glm::vec2 light{1,static_cast<float>(block.emission)/15};
                if(lighting) {
                    if(const auto cell=offsetPosition({origin.x+x,origin.y+y,origin.z+z},face.neighbor))
                        light={lighting->sky(*cell),std::max(lighting->block(*cell),light.y)};
                }
                // On vertical faces, V=0 is the upper edge, preserving the grass fringe.
                constexpr std::array<glm::vec2, 4> faceUvs{{{0, 1}, {0, 0}, {1, 0}, {1, 1}}};
                for (std::size_t i = 0; i < face.corners.size(); ++i)
                    mesh.vertices.push_back({glm::vec3(x, y, z) + face.corners[i], block.color,
                        atlas.uv(block.faceTextures[side], faceUvs[i]),glm::vec3(face.neighbor.x,face.neighbor.y,face.neighbor.z),block.opacity,light});
                auto& indices=block.opacity<1?mesh.translucentIndices:mesh.indices;
                for (auto index : {0u, 1u, 2u, 2u, 3u, 0u}) indices.push_back(base + index);
            }
        }
    }
    return mesh;
}
}
