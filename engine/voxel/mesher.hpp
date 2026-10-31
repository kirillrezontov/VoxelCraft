#pragma once
#include "engine/renderer/mesh.hpp"
#include "engine/renderer/texture.hpp"
#include "engine/voxel/world.hpp"

namespace vc::voxel {
class Lighting;
// All chunks must be inserted before meshing, so boundary faces see their neighbors.
Mesh meshChunk(const Chunk& chunk,const World& world,const BlockRegistry& registry,const AtlasLayout& atlas,const Lighting* lighting=nullptr,bool greedy=true);
}
