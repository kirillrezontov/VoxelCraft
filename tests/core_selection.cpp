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
    const std::size_t expected = VC_CORE_MESHER_CULLED ? 6u*32*32*4 : 24u;
    return meshes.size()==1 && meshes[0].vertices.size()==expected ? 0 : 1;
}
