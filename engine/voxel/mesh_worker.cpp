#include "engine/voxel/mesh_worker.hpp"

namespace vc::voxel {
template class BasicMeshWorker<GreedyChunkMesher>;
template class BasicMeshWorker<CulledChunkMesher>;
template class BasicBackgroundMeshQueue<GreedyChunkMesher>;
template class BasicBackgroundMeshQueue<CulledChunkMesher>;
}
