#include "game/player/spawn.hpp"
#include "game/player/controller.hpp"
#include "engine/voxel/streaming.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vc {
namespace {
template<class Sample> glm::dvec3 spawn(const voxel::TerrainGenerator& generator,const voxel::BlockRegistry& blocks,
        Sample sample,std::span<const voxel::Aabb> occupied) {
    const auto definition=[&](voxel::Position p)->const voxel::BlockDefinition& { return blocks.definition(sample(p)); };
    const auto available=[&](glm::dvec3 eye) {
        PlayerController player;player.camera.position=eye;const auto body=player.bounds();
        return std::none_of(occupied.begin(),occupied.end(),[&](const auto& other){return body.intersects(other);});
    };
    std::optional<glm::dvec3> waterFallback;
    for(int radius=0;radius<=32;++radius) for(int z=-radius;z<=radius;++z) for(int x=-radius;x<=radius;++x) {
        if(std::max(std::abs(x),std::abs(z))!=radius) continue;
        const auto y=generator.surfaceY(x,z);
        if(std::abs(static_cast<double>(y))>1e15-4) throw std::runtime_error("World surface is outside supported player coordinates");
        const auto& foot=definition({x,y+1,z});const auto& head=definition({x,y+2,z});
        const glm::dvec3 candidate{static_cast<double>(x)+0.5,static_cast<double>(y)+1+PlayerController::eyeHeight,static_cast<double>(z)+0.5};
        if(definition({x,y,z}).solid&&!foot.solid&&!head.solid&&!foot.fluid&&!head.fluid&&available(candidate)) return candidate;
        if(!waterFallback&&foot.fluid) {
            auto surface=y+1;
            for(int depth=0;depth<64&&definition({x,surface,z}).fluid;++depth) ++surface;
            if(!definition({x,surface,z}).solid&&!definition({x,surface,z}).fluid&&
                !definition({x,surface+1,z}).solid&&!definition({x,surface+1,z}).fluid)
                if(const glm::dvec3 water{static_cast<double>(x)+0.5,static_cast<double>(surface)-0.2+PlayerController::eyeHeight,static_cast<double>(z)+0.5};available(water)) waterFallback=water;
        }
    }
    if(waterFallback) return *waterFallback;
    throw std::runtime_error("No clear spawn found within 32 blocks of the origin");
}
}
glm::dvec3 findSpawn(const voxel::TerrainGenerator& generator,const voxel::BlockRegistry& blocks,
        const voxel::World* edits,std::span<const voxel::Aabb> occupied) {
    return spawn(generator,blocks,[&](voxel::Position p){return edits&&edits->loaded(p)?edits->block(p):generator.sample(p);},occupied);
}
glm::dvec3 findSpawn(const voxel::TerrainGenerator& generator,const voxel::BlockRegistry& blocks,
        const voxel::ChunkStreamer& streamer,std::span<const voxel::Aabb> occupied) {
    return spawn(generator,blocks,[&](voxel::Position p){return streamer.sample(p);},occupied);
}
}
