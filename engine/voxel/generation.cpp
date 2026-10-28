#include "engine/voxel/generation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vc::voxel {
namespace {
std::uint64_t mix(std::uint64_t value) {
    value ^= value >> 30; value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27; value *= 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
double smooth(double x) { return x * x * (3 - 2 * x); }
double lerp(double a, double b, double t) { return a + (b - a) * t; }
}
TerrainGenerator::TerrainGenerator(std::uint64_t seed, WorldBounds bounds, const BlockRegistry& registry,std::uint32_t version)
    : seed_(seed), bounds_(bounds), stone_(registry.require("voxelcraft:stone")),
      dirt_(registry.require("voxelcraft:dirt")), grass_(registry.require("voxelcraft:grass")),version_(version) {
    bounds_.validate();
    if(version<1||version>3) throw std::invalid_argument("Unsupported terrain generator version");
    if(version>=2) {
        log_=registry.require("voxelcraft:log");leaves_=registry.require("voxelcraft:leaves");
        coalOre_=registry.require("voxelcraft:coal_ore");ironOre_=registry.require("voxelcraft:iron_ore");
    }
    if(version>=3) {
        sand_=registry.require("voxelcraft:sand");sandstone_=registry.require("voxelcraft:sandstone");snow_=registry.require("voxelcraft:snow");
        water_=registry.require("voxelcraft:water");cobblestone_=registry.require("voxelcraft:cobblestone");lamp_=registry.require("voxelcraft:lamp");
    }
}
double TerrainGenerator::noise(std::int64_t x,std::int64_t z,int scale,std::uint64_t salt) const {
    const auto gx = x / scale - (x % scale < 0 ? 1 : 0);
    const auto gz = z / scale - (z % scale < 0 ? 1 : 0);
    const double tx = smooth(static_cast<double>((x % scale + scale) % scale) / scale);
    const double tz = smooth(static_cast<double>((z % scale + scale) % scale) / scale);
    const auto value = [&](std::int64_t a, std::int64_t b) {
        const auto hash = mix(mix(static_cast<std::uint64_t>(a)) ^ mix(static_cast<std::uint64_t>(b) + 0x9e3779b97f4a7c15ULL) ^ seed_^salt);
        return static_cast<double>(hash >> 40) / 8388607.5 - 1.0;
    };
    return lerp(lerp(value(gx, gz), value(gx + 1, gz), tx), lerp(value(gx, gz + 1), value(gx + 1, gz + 1), tx), tz);
}
std::int64_t TerrainGenerator::surfaceY(std::int64_t x, std::int64_t z) const {
    double value=12+18*noise(x,z,64)+8*noise(x,z,32)+3*noise(x,z,16);
    if(version_>=3) {
        const auto mountain=std::clamp((noise(x,z,192,0x6d6f756e74ULL)+0.05)/0.9,0.0,1.0);
        value+=80*mountain*mountain;
        const auto river=std::abs(noise(x,z,256,0x7269766572ULL));
        const auto bank=smooth(std::clamp((river-0.03)/0.07,0.0,1.0));
        value=lerp(2+noise(x,z,32,0x626564ULL),value,bank);
    }
    const auto height=static_cast<std::int64_t>(std::floor(value));
    return std::clamp(height, bounds_.minY, bounds_.maxY);
}
BlockId TerrainGenerator::columnBlock(std::int64_t y, std::int64_t surface) const {
    if (!bounds_.contains(y) || y > surface) return air;
    if (y == surface) return grass_;
    // Unsigned subtraction keeps the depth valid even at the limits of int64.
    const auto depth = static_cast<std::uint64_t>(surface) - static_cast<std::uint64_t>(y);
    return depth <= 3 ? dirt_ : stone_;
}
TerrainGenerator::TreeColumn TerrainGenerator::treeColumn(std::int64_t x,std::int64_t z) const {
    const auto axis=[](std::int64_t v){return v/12-(v%12<0?1:0);};
    const auto remainder=[](std::int64_t v){const auto r=v%12;return static_cast<int>(r<0?r+12:r);};
    const auto hash=mix(mix(static_cast<std::uint64_t>(axis(x)))^mix(static_cast<std::uint64_t>(axis(z))+0x9e3779b97f4a7c15ULL)^seed_^0x74726565732ULL);
    if(version_<3&&hash%3!=0) return {};
    const int localX=2+static_cast<int>((hash>>8)%8),localZ=2+static_cast<int>((hash>>12)%8);
    const int dx=localX-remainder(x),dz=localZ-remainder(z),height=4+static_cast<int>((hash>>16)%3);
    if(std::abs(dx)>2||std::abs(dz)>2) return {};
    const auto base=offsetPosition({x,0,z},{dx,0,dz});if(!base) return {};
    const auto ground=surfaceY(base->x,base->z);
    if(version_>=3) {
        const auto biome=biomeAt(base->x,base->z);const auto chance=biome==Biome::Forest?2u:biome==Biome::Plains?5u:4u;
        if(biome==Biome::Desert||hash%chance!=0||ground<5||caveEntrance(base->x,base->z)) return {};
    }
    const auto headroom=static_cast<std::uint64_t>(bounds_.maxY)-static_cast<std::uint64_t>(ground);
    if(headroom<static_cast<std::uint64_t>(height+1)) return {};
    return {ground,dx,dz,height,true};
}
BlockId TerrainGenerator::treeBlock(std::int64_t y,const TreeColumn& tree) const {
    if(!tree.present||y<=tree.ground) return air;
    const auto level=static_cast<std::uint64_t>(y)-static_cast<std::uint64_t>(tree.ground);
    if(tree.dx==0&&tree.dz==0&&level<=static_cast<std::uint64_t>(tree.height)) return log_;
    if(level<static_cast<std::uint64_t>(tree.height-2)||level>static_cast<std::uint64_t>(tree.height+1)) return air;
    const int radius=level>=static_cast<std::uint64_t>(tree.height)?1:2;
    return std::abs(tree.dx)<=radius&&std::abs(tree.dz)<=radius&&std::abs(tree.dx)+std::abs(tree.dz)<=3?leaves_:air;
}
BlockId TerrainGenerator::resourceBlock(Position p) const {
    const auto axis=[](std::int64_t v){return v/8-(v%8<0?1:0);};
    const auto remainder=[](std::int64_t v){const auto r=v%8;return static_cast<int>(r<0?r+8:r);};
    const auto hash=mix(mix(static_cast<std::uint64_t>(axis(p.x)))^mix(static_cast<std::uint64_t>(axis(p.y))+0x12345678ULL)^
        mix(static_cast<std::uint64_t>(axis(p.z))+0x9abcdefULL)^seed_);
    const int dx=remainder(p.x)-(2+static_cast<int>((hash>>8)%4)),dy=remainder(p.y)-(2+static_cast<int>((hash>>12)%4)),
        dz=remainder(p.z)-(2+static_cast<int>((hash>>16)%4));
    if(dx*dx+dy*dy+dz*dz>5) return stone_;
    if(p.y<0&&hash%13==0) return ironOre_;
    if(hash%7==0) return coalOre_;
    return stone_;
}
BlockId TerrainGenerator::sample(Position p) const {
    if(!bounds_.contains(p.y)) return air;
    const auto surface=surfaceY(p.x,p.z);
    if(version_>=3) return expandedSample(p,surface,biomeAt(p.x,p.z),caveEntrance(p.x,p.z),treeColumn(p.x,p.z),ruinColumn(p.x,p.z));
    const auto base=columnBlock(p.y,surface);
    if(version_==1) return base;
    if(base==stone_) return resourceBlock(p);
    if(base!=air) return base;
    return treeBlock(p.y,treeColumn(p.x,p.z));
}
Chunk TerrainGenerator::generate(Position position) const {
    Chunk chunk(position);
    const auto origin = chunkOrigin(position);
    for (int z = 0; z < chunkSize; ++z) for (int x = 0; x < chunkSize; ++x) {
        const auto height = surfaceY(origin.x + x, origin.z + z);
        const auto tree=version_>=2?treeColumn(origin.x+x,origin.z+z):TreeColumn{};
        const auto biome=version_>=3?biomeAt(origin.x+x,origin.z+z):Biome::Plains;
        const auto entrance=version_>=3&&caveEntrance(origin.x+x,origin.z+z);
        const auto ruin=version_>=3?ruinColumn(origin.x+x,origin.z+z):RuinColumn{};
        for (int y = 0; y < chunkSize; ++y) {
            const Position p{origin.x+x,origin.y+y,origin.z+z};auto block=columnBlock(p.y,height);
            if(version_>=3&&bounds_.contains(p.y)) block=expandedSample(p,height,biome,entrance,tree,ruin);
            else if(version_==2&&bounds_.contains(p.y)) {
                if(block==stone_) block=resourceBlock(p);
                else if(block==air) block=treeBlock(p.y,tree);
            }
            chunk.set({x,y,z},block);
        }
    }
    return chunk;
}
const char* biomeName(Biome biome) {
    switch(biome) { case Biome::Plains:return "plains";case Biome::Forest:return "forest";case Biome::Desert:return "desert";case Biome::Mountains:return "mountains"; }
    return "unknown";
}
Biome TerrainGenerator::biomeAt(std::int64_t x,std::int64_t z) const {
    if(version_<3) return Biome::Plains;
    if(noise(x,z,192,0x6d6f756e74ULL)>0.35) return Biome::Mountains;
    const auto moisture=noise(x,z,256,0x6d6f697374ULL),temperature=noise(x,z,384,0x74656d70ULL);
    if(temperature>0.2&&moisture<-0.05) return Biome::Desert;
    return moisture>0.02?Biome::Forest:Biome::Plains;
}
double TerrainGenerator::noise3(Position p,int scale,std::uint64_t salt) const {
    const auto axis=[&](std::int64_t v){return v/scale-(v%scale<0?1:0);};
    const auto fraction=[&](std::int64_t v){const auto r=v%scale;return smooth(static_cast<double>(r<0?r+scale:r)/scale);};
    const auto x=axis(p.x),y=axis(p.y),z=axis(p.z);const auto tx=fraction(p.x),ty=fraction(p.y),tz=fraction(p.z);
    const auto value=[&](std::int64_t a,std::int64_t b,std::int64_t c) {
        const auto hash=mix(mix(static_cast<std::uint64_t>(a))^mix(static_cast<std::uint64_t>(b)+0x12345678ULL)^
            mix(static_cast<std::uint64_t>(c)+0x9abcdefULL)^seed_^salt);
        return static_cast<double>(hash>>40)/8388607.5-1;
    };
    return lerp(lerp(lerp(value(x,y,z),value(x+1,y,z),tx),lerp(value(x,y+1,z),value(x+1,y+1,z),tx),ty),
        lerp(lerp(value(x,y,z+1),value(x+1,y,z+1),tx),lerp(value(x,y+1,z+1),value(x+1,y+1,z+1),tx),ty),tz);
}
bool TerrainGenerator::caveEntrance(std::int64_t x,std::int64_t z) const {
    return std::abs(noise(x,z,64,0x656e7472616e63ULL))<0.025&&noise(x,z,128,0x656e7472616e32ULL)>0.15;
}
bool TerrainGenerator::caveAir(Position p,std::int64_t surface,bool entrance) const {
    if(p.y>surface||static_cast<std::uint64_t>(p.y)-static_cast<std::uint64_t>(bounds_.minY)<2) return false;
    const auto depth=static_cast<std::uint64_t>(surface)-static_cast<std::uint64_t>(p.y);
    if(entrance&&surface>=5&&depth<12) return true;
    return depth>=5&&std::abs(noise3(p,24,0x6361766531ULL))<0.10&&std::abs(noise3(p,32,0x6361766532ULL))<0.16;
}
TerrainGenerator::RuinColumn TerrainGenerator::ruinColumn(std::int64_t x,std::int64_t z) const {
    const auto axis=[](std::int64_t v){return v/64-(v%64<0?1:0);};
    const auto local=[](std::int64_t v){const auto r=v%64;return static_cast<int>(r<0?r+64:r);};
    const auto hash=mix(mix(static_cast<std::uint64_t>(axis(x)))^mix(static_cast<std::uint64_t>(axis(z))+0xabcdefULL)^seed_^0x7275696eULL);
    if(hash%11!=0) return {};
    const int dx=16+static_cast<int>((hash>>8)%32)-local(x),dz=16+static_cast<int>((hash>>16)%32)-local(z);
    if(std::abs(dx)>2||std::abs(dz)>2) return {};
    const auto base=offsetPosition({x,0,z},{dx,0,dz});if(!base) return {};
    const auto biome=biomeAt(base->x,base->z);const auto ground=surfaceY(base->x,base->z);
    if((biome!=Biome::Plains&&biome!=Biome::Forest)||ground<5||caveEntrance(base->x,base->z)||
        static_cast<std::uint64_t>(bounds_.maxY)-static_cast<std::uint64_t>(ground)<6) return {};
    for(const LocalPosition corner:{LocalPosition{-2,0,-2},{-2,0,2},{2,0,-2},{2,0,2}}) {
        const auto adjacent=offsetPosition(*base,corner);if(!adjacent) return {};
        if(std::abs(static_cast<double>(surfaceY(adjacent->x,adjacent->z))-static_cast<double>(ground))>2) return {};
    }
    return {ground,dx,dz,hash,true};
}
BlockId TerrainGenerator::ruinBlock(std::int64_t y,std::int64_t surface,const RuinColumn& ruin) const {
    if(!ruin.present||y<=surface) return air;
    if(y<=ruin.ground+1) return cobblestone_;
    const auto level=static_cast<std::uint64_t>(y)-static_cast<std::uint64_t>(ruin.ground);
    if(ruin.dx==0&&ruin.dz==0&&level==2) return lamp_;
    if(std::abs(ruin.dx)==2&&std::abs(ruin.dz)==2&&level<=3+(ruin.style&1)) return cobblestone_;
    if((std::abs(ruin.dx)==2||std::abs(ruin.dz)==2)&&level==2&&(ruin.dx+ruin.dz)%2==0) return cobblestone_;
    if(level==5&&(std::abs(ruin.dx)==2||std::abs(ruin.dz)==2)&&(ruin.dx+ruin.dz)%3!=0) return cobblestone_;
    return air;
}
BlockId TerrainGenerator::expandedSample(Position p,std::int64_t surface,Biome biome,bool entrance,const TreeColumn& tree,const RuinColumn& ruin) const {
    if(p.y>surface) {
        if(p.y<=4) return water_;
        if(const auto structure=ruinBlock(p.y,surface,ruin);structure!=air) return structure;
        return treeBlock(p.y,tree);
    }
    if(caveAir(p,surface,entrance)) return air;
    const auto depth=static_cast<std::uint64_t>(surface)-static_cast<std::uint64_t>(p.y);
    if(depth==0&&surface<=4) return sand_;
    if(biome==Biome::Desert&&depth<=4) return depth==0?sand_:sandstone_;
    if(depth==0) {
        if(biome==Biome::Mountains&&surface>72) return snow_;
        return biome==Biome::Mountains&&surface>50?stone_:grass_;
    }
    return depth<=3?dirt_:resourceBlock(p);
}
}
