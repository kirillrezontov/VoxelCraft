#include "game/simulation/entity_data.hpp"
#include <cmath>
#include <set>
#include <stdexcept>

namespace vc {
namespace { bool finite(glm::dvec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); } }
void validateActors(const std::vector<SavedActor>& actors) {
    if(actors.size()>maxSavedActors) throw std::runtime_error("Too many saved actors");
    std::set<std::uint64_t> ids;unsigned drops=0;
    for(const auto& actor:actors) {
        if(!actor.id||actor.id==UINT64_MAX||!ids.insert(actor.id).second) throw std::runtime_error("Invalid/duplicate actor identity");
        if(!finite(actor.position)||glm::any(glm::greaterThan(glm::abs(actor.position),glm::dvec3(1e15)))||
           !finite(actor.velocity)||glm::length(actor.velocity)>100||!std::isfinite(actor.yaw)||std::abs(actor.yaw)>6.283186||
           !std::isfinite(actor.age)||actor.age<0||actor.age>1e12||!std::isfinite(actor.phase)||std::abs(actor.phase)>6.283186)
            throw std::runtime_error("Invalid saved actor motion");
        if(actor.kind=="voxelcraft:dropped_item") {
            if(++drops>32||actor.health||actor.item.identifier.size()>256||!voxel::BlockRegistry::validIdentifier(actor.item.identifier)||
                !actor.item.count||actor.item.count>64) throw std::runtime_error("Invalid saved dropped item");
        } else if(actor.kind=="voxelcraft:grazer"||actor.kind=="voxelcraft:stalker") {
            if(!actor.health||actor.health>(actor.kind=="voxelcraft:grazer"?8u:12u)||!actor.item.identifier.empty()||actor.item.count||actor.item.durability)
                throw std::runtime_error("Invalid saved creature");
        } else throw std::runtime_error("Unknown saved creature type: "+actor.kind);
    }
    if(actors.size()-drops>32) throw std::runtime_error("Too many saved creatures");
}
}
