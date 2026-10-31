#include "engine/voxel/mesh_queue.hpp"

namespace vc::voxel {
template class BasicChunkMeshQueue<GreedyChunkMesher>;
template class BasicChunkMeshQueue<CulledChunkMesher>;
}
