#include "engine/voxel/lighting.hpp"
#include <algorithm>
#include <deque>
#include <cmath>

namespace vc::voxel {
namespace {
constexpr std::array<LocalPosition,6> directions{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
}
std::vector<Position> Lighting::refresh(const World& world,const BlockRegistry& registry,std::span<const Position> changed) {
    if(changed.empty()) return {};
    std::set<Position> dirty,columns;
    for(const auto p:changed) {
        columns.insert({p.x,0,p.z});
        const auto* chunk=world.findChunk(p);
        if(!chunk) { sources_.erase(p);continue; }
        std::vector<Source> sources;const auto origin=chunkOrigin(p);
        for(int y=0;y<32;++y) {
            if(!world.bounds().contains(origin.y+y)) continue;
            for(int z=0;z<32;++z) for(int x=0;x<32;++x) {
                const auto id=chunk->get({x,y,z});if(id==air) continue;
                const auto emission=registry.definition(id).emission;
                if(emission) sources.push_back({{origin.x+x,origin.y+y,origin.z+z},emission});
            }
        }
        sources_.insert_or_assign(p,std::move(sources));
    }
    for(const auto column:columns) {
        std::vector<const Chunk*> chunks;
        for(const auto& [p,chunk]:world.chunks()) if(p.x==column.x&&p.z==column.z) chunks.push_back(&chunk);
        std::sort(chunks.begin(),chunks.end(),[](const auto* a,const auto* b){return a->position().y>b->position().y;});
        SkyColumn next{};
        for(const auto* chunk:chunks) {
            const auto origin=chunkOrigin(chunk->position());
            for(int y=31;y>=0;--y) {
                if(!world.bounds().contains(origin.y+y)) continue;
                for(int z=0;z<32;++z) for(int x=0;x<32;++x) {
                    auto& cell=next[static_cast<std::size_t>(x+32*z)];
                    if(!cell.present&&registry.definition(chunk->get({x,y,z})).opaque) cell={origin.y+y,true};
                }
            }
        }
        const auto old=sky_.find(column);
        const bool altered=old==sky_.end()?!chunks.empty():old->second!=next;
        if(chunks.empty()) sky_.erase(column);else sky_.insert_or_assign(column,next);
        if(altered) {
            for(const auto& [p,chunk]:world.chunks()) {
                (void)chunk;
                // Face-neighbor sampling also uses the immediately adjacent column.
                if(std::abs(p.x-column.x)<=1&&std::abs(p.z-column.z)<=1) dirty.insert(p);
            }
        }
    }
    std::map<Position,BlockLight> next;
    std::deque<Source> frontier;
    const auto set=[&](Position p,std::uint8_t level) {
        auto& value=next[chunkPosition(p)].levels[blockIndex(localPosition(p))];
        if(value>=level) return false;
        value=level;return true;
    };
    for(const auto& [p,sources]:sources_) {
        (void)p;for(const auto& source:sources) if(set(source.position,source.level)) frontier.push_back(source);
    }
    while(!frontier.empty()) {
        const auto source=frontier.front();frontier.pop_front();if(source.level<=1) continue;
        const auto level=static_cast<std::uint8_t>(source.level-1);
        for(const auto direction:directions) {
            const auto adjacent=offsetPosition(source.position,direction);
            if(!adjacent||!world.loaded(*adjacent)||registry.definition(world.block(*adjacent)).opaque) continue;
            if(set(*adjacent,level)) frontier.push_back({*adjacent,level});
        }
    }
    const auto changedLight=[&](Position p) {
        if(world.findChunk(p)) dirty.insert(p);
        for(const auto direction:directions) if(const auto adjacent=offsetPosition(p,direction);adjacent&&world.findChunk(*adjacent)) dirty.insert(*adjacent);
    };
    for(const auto& [p,light]:blocks_) {
        const auto current=next.find(p);if(current==next.end()||current->second!=light) changedLight(p);
    }
    for(const auto& [p,light]:next) {
        const auto previous=blocks_.find(p);if(previous==blocks_.end()||previous->second!=light) changedLight(p);
    }
    blocks_=std::move(next);return {dirty.begin(),dirty.end()};
}
float Lighting::sky(Position cell) const {
    const auto p=chunkPosition(cell);const auto found=sky_.find({p.x,0,p.z});if(found==sky_.end()) return 1;
    const auto local=localPosition(cell);const auto& top=found->second[static_cast<std::size_t>(local.x+32*local.z)];
    return !top.present||cell.y>top.top?1.0f:0.0f;
}
float Lighting::block(Position cell) const {
    const auto found=blocks_.find(chunkPosition(cell));return found==blocks_.end()?0:static_cast<float>(found->second.levels[blockIndex(localPosition(cell))])/15;
}
Lighting Lighting::snapshotForMesh(Position chunk) const {
    (void)chunkOrigin(chunk);Lighting snapshot;
    const auto copy=[&](Position p) {
        if(const auto light=blocks_.find(p);light!=blocks_.end()) snapshot.blocks_.insert(*light);
        if(const auto column=sky_.find({p.x,0,p.z});column!=sky_.end()) snapshot.sky_.insert(*column);
    };
    copy(chunk);
    for(const auto direction:directions) if(const auto adjacent=offsetPosition(chunk,direction)) copy(*adjacent);
    return snapshot;
}
}
