#include "game/content/blocks.hpp"
#include "game/content/atlas.hpp"

namespace vc {
voxel::BlockRegistry builtinBlocks() {
    voxel::BlockRegistry registry;
    registry.add({"voxelcraft:stone", true, {1, 1, 1}, {tiles::stone, tiles::stone, tiles::stone, tiles::stone, tiles::stone, tiles::stone}});
    registry.add({"voxelcraft:dirt", true, {1, 1, 1}, {tiles::dirt, tiles::dirt, tiles::dirt, tiles::dirt, tiles::dirt, tiles::dirt}});
    registry.add({"voxelcraft:grass", true, {1, 1, 1}, {tiles::grassSide, tiles::grassSide, tiles::grassTop, tiles::dirt, tiles::grassSide, tiles::grassSide}});
    registry.add({"voxelcraft:log",true,{1,1,1},{tiles::logSide,tiles::logSide,tiles::logTop,tiles::logTop,tiles::logSide,tiles::logSide}});
    registry.add({"voxelcraft:planks",true,{1,1,1},{tiles::planks,tiles::planks,tiles::planks,tiles::planks,tiles::planks,tiles::planks}});
    registry.add({"voxelcraft:leaves",true,{1,1,1},{tiles::leaves,tiles::leaves,tiles::leaves,tiles::leaves,tiles::leaves,tiles::leaves}});
    registry.add({"voxelcraft:cobblestone",true,{1,1,1},{tiles::cobblestone,tiles::cobblestone,tiles::cobblestone,tiles::cobblestone,tiles::cobblestone,tiles::cobblestone}});
    registry.add({"voxelcraft:coal_ore",true,{1,1,1},{tiles::coalOre,tiles::coalOre,tiles::coalOre,tiles::coalOre,tiles::coalOre,tiles::coalOre}});
    registry.add({"voxelcraft:iron_ore",true,{1,1,1},{tiles::ironOre,tiles::ironOre,tiles::ironOre,tiles::ironOre,tiles::ironOre,tiles::ironOre}});
    registry.add({"voxelcraft:sand",true,{1,1,1},{tiles::sand,tiles::sand,tiles::sand,tiles::sand,tiles::sand,tiles::sand}});
    registry.add({"voxelcraft:sandstone",true,{1,1,1},{tiles::sandstone,tiles::sandstone,tiles::sandstone,tiles::sandstone,tiles::sandstone,tiles::sandstone}});
    registry.add({"voxelcraft:snow",true,{1,1,1},{tiles::snow,tiles::snow,tiles::snow,tiles::snow,tiles::snow,tiles::snow}});
    registry.add({"voxelcraft:water",false,{1,1,1},{tiles::water,tiles::water,tiles::water,tiles::water,tiles::water,tiles::water},false,true,0,0.65f});
    registry.add({"voxelcraft:lamp",true,{1,1,1},{tiles::lamp,tiles::lamp,tiles::lamp,tiles::lamp,tiles::lamp,tiles::lamp},true,false,15});
    return registry;
}
}
