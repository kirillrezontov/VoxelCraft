#pragma once
#include "engine/renderer/texture.hpp"

namespace vc {
// Tile zero preserves vertex-color rendering for untextured/custom definitions.
namespace tiles {
inline constexpr TextureTile white = 0, stone = 1, dirt = 2, grassTop = 3, grassSide = 4;
inline constexpr TextureTile logSide=5,logTop=6,planks=7,leaves=8,cobblestone=9,coalOre=10,ironOre=11;
inline constexpr TextureTile sand=12,sandstone=13,snow=14,water=15,lamp=16;
}
AtlasLayout builtinAtlasLayout();
ImageRgba builtinAtlas();
}
