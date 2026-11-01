#pragma once
#include "engine/voxel/mesh_queue.hpp"
#include "engine/voxel/mesh_worker.hpp"
#if VC_CORE_MESHER_EXAMPLE
#include "examples/core_modules/warm_reference_mesher.hpp"
#endif

// Selection belongs to host composition, never to individual voxels/vertices.
namespace vc::client {
#if VC_CORE_MESHER_EXAMPLE
using CoreMesher = examples::WarmReferenceMesher;
#elif VC_CORE_MESHER_CULLED
using CoreMesher = voxel::CulledChunkMesher;
#else
using CoreMesher = voxel::GreedyChunkMesher;
#endif
using CoreMeshQueue = voxel::BasicChunkMeshQueue<CoreMesher>;
using CoreBackgroundMeshQueue = voxel::BasicBackgroundMeshQueue<CoreMesher>;
}
