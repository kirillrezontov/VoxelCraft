#include "game/player/spawn.hpp"
#include "game/player/controller.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vc {
glm::dvec3 findSpawn(const voxel::TerrainGenerator& generator,const voxel::BlockRegistry& blocks,const voxel::World* edits) {
    const auto definition=[&](voxel::Position p)->const voxel::BlockDefinition& {
        const auto block=edits&&edits->loaded(p)?edits->block(p):generator.sample(p);return blocks.definition(block);
    };
    std::optional<glm::dvec3> waterFallback;
    for(int radius=0;radius<=32;++radius) for(int z=-radius;z<=radius;++z) for(int x=-radius;x<=radius;++x) {
        if(std::max(std::abs(x),std::abs(z))!=radius) continue;
        const auto y=generator.surfaceY(x,z);
        if(std::abs(static_cast<double>(y))>1e15-4) throw std::runtime_error("World surface is outside supported player coordinates");
        const auto& foot=definition({x,y+1,z});const auto& head=definition({x,y+2,z});
        if(definition({x,y,z}).solid&&!foot.solid&&!head.solid&&!foot.fluid&&!head.fluid)
            return {static_cast<double>(x)+0.5,static_cast<double>(y)+1+PlayerController::eyeHeight,static_cast<double>(z)+0.5};
        if(!waterFallback&&foot.fluid) {
            auto surface=y+1;
            for(int depth=0;depth<64&&definition({x,surface,z}).fluid;++depth) ++surface;
            if(!definition({x,surface,z}).solid&&!definition({x,surface,z}).fluid&&
                !definition({x,surface+1,z}).solid&&!definition({x,surface+1,z}).fluid)
                waterFallback=glm::dvec3(static_cast<double>(x)+0.5,static_cast<double>(surface)-0.2+PlayerController::eyeHeight,static_cast<double>(z)+0.5);
        }
    }
    if(waterFallback) return *waterFallback;
    throw std::runtime_error("No clear spawn found within 32 blocks of the origin");
}
}
