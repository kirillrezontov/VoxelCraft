#include "engine/voxel/mesher.hpp"
#include "engine/voxel/lighting.hpp"
#include <array>
#include <limits>
#include <memory>

namespace vc::voxel {
namespace {
struct Face { LocalPosition neighbor; std::array<glm::vec3, 4> corners; };
const std::array<Face, 6> faces{{
    {{1, 0, 0}, {{{1, 0, 0}, {1, 1, 0}, {1, 1, 1}, {1, 0, 1}}}},
    {{-1, 0, 0}, {{{0, 0, 1}, {0, 1, 1}, {0, 1, 0}, {0, 0, 0}}}},
    {{0, 1, 0}, {{{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}}}},
    {{0, -1, 0}, {{{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}}}},
    {{0, 0, 1}, {{{1, 0, 1}, {1, 1, 1}, {0, 1, 1}, {0, 0, 1}}}},
    {{0, 0, -1}, {{{0, 0, 0}, {0, 1, 0}, {1, 1, 0}, {1, 0, 0}}}}
}};
bool representable(std::int64_t origin, int local) {
    return !((local < 0 && origin == std::numeric_limits<std::int64_t>::min()) ||
        (local >= chunkSize && origin > std::numeric_limits<std::int64_t>::max() - chunkSize));
}
}
Mesh meshChunk(const Chunk& chunk,const World& world,const BlockRegistry& registry,const AtlasLayout& atlas,const Lighting* lighting,bool greedy) {
    Mesh mesh;
    const auto origin = chunkOrigin(chunk.position()); mesh.origin = {origin.x, origin.y, origin.z};
    const auto neighbor = [&](LocalPosition p) -> BlockId {
        if (!representable(origin.x, p.x) || !representable(origin.y, p.y) || !representable(origin.z, p.z)) return air;
        if (!world.bounds().contains(origin.y + p.y)) return air;
        if (p.x >= 0 && p.x < chunkSize && p.y >= 0 && p.y < chunkSize && p.z >= 0 && p.z < chunkSize) return chunk.get(p);
        return world.block({origin.x + p.x, origin.y + p.y, origin.z + p.z});
    };
    const auto lightAt=[&](LocalPosition p,const Face& face,const BlockDefinition& block) {
        glm::vec2 light{1,static_cast<float>(block.emission)/15};
        if(lighting) if(const auto cell=offsetPosition({origin.x+p.x,origin.y+p.y,origin.z+p.z},face.neighbor))
            light={lighting->sky(*cell),std::max(lighting->block(*cell),light.y)};
        return light;
    };
    const auto emit=[&](LocalPosition p,std::size_t side,const BlockDefinition& block,glm::vec2 light,int width=1,int height=1) {
        const auto& face=faces[side];const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
        const auto u=face.corners[3]-face.corners[0],v=face.corners[1]-face.corners[0];
        constexpr std::array<glm::vec2,4> faceUvs{{{0,1},{0,0},{1,0},{1,1}}};
        const bool repeated=width>1||height>1;
        const auto low=atlas.uv(block.faceTextures[side],{0,0}),high=atlas.uv(block.faceTextures[side],{1,1});
        for(std::size_t i=0;i<4;++i) {
            const auto position=glm::vec3(p.x,p.y,p.z)+face.corners[0]+u*(faceUvs[i].x*static_cast<float>(width))+
                v*((1-faceUvs[i].y)*static_cast<float>(height));
            Vertex vertex{position,block.color,repeated?faceUvs[i]*glm::vec2(width,height):atlas.uv(block.faceTextures[side],faceUvs[i]),
                glm::vec3(face.neighbor.x,face.neighbor.y,face.neighbor.z),block.opacity,light};
            if(repeated) vertex.tileBounds={low,high-low};
            mesh.vertices.push_back(vertex);
        }
        auto& indices=block.opacity<1?mesh.translucentIndices:mesh.indices;
        for(const auto index:{0u,1u,2u,2u,3u,0u}) indices.push_back(base+index);
    };
    struct Cell { BlockId id{};glm::vec2 light{};bool operator==(const Cell&) const=default; };
    using Mask=std::array<Cell,chunkSize*chunkSize>;
    // Allocate only slices with visible opaque faces. Empty/buried chunks retain
    // the reference path's single scan rather than six full-volume scans.
    std::array<std::unique_ptr<Mask>,6*chunkSize> masks;
    // Keep the original unit-face path for transparent/non-opaque cubes and as
    // a reference mode for surface equivalence tests and repeatable benchmarks.
    for (int y = 0; y < chunkSize; ++y) {
        if (!world.bounds().contains(origin.y + y)) continue;
        for (int z = 0; z < chunkSize; ++z) for (int x = 0; x < chunkSize; ++x) {
            const auto id=chunk.get({x,y,z});if(id==air) continue;
            const auto& block=registry.definition(id);
            for(std::size_t side=0;side<faces.size();++side) {
                const auto& face=faces[side];const auto adjacent=neighbor({x+face.neighbor.x,y+face.neighbor.y,z+face.neighbor.z});
                if(registry.definition(adjacent).opaque||(block.opacity<1&&adjacent==id)) continue;
                const auto light=lightAt({x,y,z},face,block);
                if(!greedy||!block.opaque) { emit({x,y,z},side,block,light);continue; }
                const int depth=side<2?x:side<4?y:z;
                const int a=side==0||side==3?z:side==1||side==2?31-z:side==4?31-x:x;
                const int b=side==2||side==3?x:y;
                auto& mask=masks[side*chunkSize+static_cast<std::size_t>(depth)];
                if(!mask) mask=std::make_unique<Mask>();
                (*mask)[static_cast<std::size_t>(b*chunkSize+a)]={id,light};
            }
        }
    }
    if(!greedy) return mesh;
    for(std::size_t side=0;side<faces.size();++side) {
        const auto& face=faces[side];const glm::ivec3 u(face.corners[3]-face.corners[0]),v(face.corners[1]-face.corners[0]);
        const glm::ivec3 n(face.neighbor.x,face.neighbor.y,face.neighbor.z);
        const auto point=[&](int depth,int a,int b) {
            glm::ivec3 p{};
            for(int axis=0;axis<3;++axis) p[axis]=n[axis]!=0?depth:
                (u[axis]<0||v[axis]<0?chunkSize-1:0)+u[axis]*a+v[axis]*b;
            return LocalPosition{p.x,p.y,p.z};
        };
        for(int depth=0;depth<chunkSize;++depth) {
            const auto& slice=masks[side*chunkSize+static_cast<std::size_t>(depth)];
            if(!slice) continue;
            auto& mask=*slice;
            for(int b=0;b<chunkSize;++b) for(int a=0;a<chunkSize;) {
                const auto key=mask[static_cast<std::size_t>(b*chunkSize+a)];if(key.id==air) { ++a;continue; }
                int width=1,height=1;
                while(a+width<chunkSize&&mask[static_cast<std::size_t>(b*chunkSize+a+width)]==key) ++width;
                while(b+height<chunkSize) {
                    bool matches=true;
                    for(int i=0;i<width;++i) if(mask[static_cast<std::size_t>((b+height)*chunkSize+a+i)]!=key) { matches=false;break; }
                    if(!matches) break;
                    ++height;
                }
                emit(point(depth,a,b),side,registry.definition(key.id),key.light,width,height);
                for(int j=0;j<height;++j) for(int i=0;i<width;++i) mask[static_cast<std::size_t>((b+j)*chunkSize+a+i)]={};
                a+=width;
            }
        }
    }
    return mesh;
}
}
