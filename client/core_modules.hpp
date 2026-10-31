#pragma once
#include "engine/voxel/mesh_queue.hpp"
#include "engine/voxel/mesh_worker.hpp"

// Selection belongs to host composition, never to individual voxels/vertices.
namespace vc::client {
#if VC_CORE_MESHER_CULLED
using CoreMesher = voxel::CulledChunkMesher;
#else
using CoreMesher = voxel::GreedyChunkMesher;
#endif
using CoreMeshQueue = voxel::BasicChunkMeshQueue<CoreMesher>;
using CoreBackgroundMeshQueue = voxel::BasicBackgroundMeshQueue<CoreMesher>;
}
