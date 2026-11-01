#include "client/core_modules.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"

int main() {
    const auto blocks = vc::builtinBlocks();
    vc::voxel::World world;
    world.insert(vc::voxel::Chunk({0,0,0},blocks.require("voxelcraft:stone")));
    vc::client::CoreMeshQueue queue;
    queue.collect(world);
    const auto meshes = queue.rebuild(world,blocks,vc::builtinAtlasLayout(),{0,0,0});
    const std::size_t expected = (VC_CORE_MESHER_CULLED||VC_CORE_MESHER_EXAMPLE) ? 6u*32*32*4 : 24u;
#if VC_CORE_MESHER_EXAMPLE
    const auto reference=vc::voxel::CulledChunkMesher::build(*world.findChunk({0,0,0}),world,blocks,vc::builtinAtlasLayout(),nullptr);
    if(meshes.size()!=1||meshes[0].vertices.empty()||meshes[0].vertices[0].color!=reference.vertices[0].color*glm::vec3(1,0.72f,0.46f)) return 2;
#endif
    return meshes.size()==1 && meshes[0].vertices.size()==expected ? 0 : 1;
}
