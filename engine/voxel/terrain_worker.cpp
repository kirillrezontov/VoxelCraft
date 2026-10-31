#include "engine/voxel/terrain_worker.hpp"

// Keep one compiled default specialization for the existing client/server.
template class vc::voxel::BasicTerrainWorker<vc::voxel::TerrainGenerator>;
