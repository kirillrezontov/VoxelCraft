#include "engine/core/paths.hpp"
#include "engine/voxel/storage.hpp"
#include "engine/core/binary.hpp"
#include "engine/platform/file_lock.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace vc::voxel {
namespace {
constexpr std::uint32_t maxChunks=8192, maxRegions=maxChunks;
constexpr std::size_t maxRegionBytes=32*1024*1024, maxManifestBytes=1024*1024;
void position(binary::Writer& out,Position p) { out.i64(p.x);out.i64(p.y);out.i64(p.z); }
Position position(binary::Reader& in) { const auto x=in.i64(),y=in.i64(),z=in.i64(); return {x,y,z}; }
LocalPosition local(std::size_t index) {
    return {static_cast<int>(index%32),static_cast<int>(index/1024),static_cast<int>((index/32)%32)};
}
std::string regionName(Position p) { return "r."+std::to_string(p.x)+'.'+std::to_string(p.y)+'.'+std::to_string(p.z)+".vcr"; }
std::vector<std::uint64_t> revisions(const std::filesystem::path& root) {
    std::vector<std::uint64_t> result;
    if(!std::filesystem::exists(root)) return result;
    for(const auto& entry:std::filesystem::directory_iterator(root)) {
        if(!entry.is_directory()) continue;
        const auto name=vc::pathText(entry.path().filename());
        std::uint64_t revision{};
        const auto parsed=std::from_chars(name.data(),name.data()+name.size(),revision);
        if(parsed.ec==std::errc{}&&parsed.ptr==name.data()+name.size()&&name==std::to_string(revision)) result.push_back(revision);
    }
    std::sort(result.begin(),result.end());
    return result;
}
binary::Bytes readFile(const std::filesystem::path& path,std::size_t limit) {
    const auto size=std::filesystem::file_size(path);
    if(size>limit) throw std::runtime_error("Save file exceeds size limit: "+vc::pathText(path));
    binary::Bytes bytes(static_cast<std::size_t>(size));
    std::ifstream file(path,std::ios::binary);
    if(!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read save file: "+vc::pathText(path));
    return bytes;
}
void writeFile(const std::filesystem::path& path,binary::Writer& out) {
    out.seal();
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    file.write(reinterpret_cast<const char*>(out.bytes.data()),static_cast<std::streamsize>(out.bytes.size()));
    file.close();
    if(!file) throw std::runtime_error("Cannot write save file: "+vc::pathText(path));
}
void version(binary::Reader& in) { if(in.u32()!=worldFormatVersion) throw std::runtime_error("Unsupported world format version"); }
void validate(const WorldMetadata& metadata) {
    metadata.bounds.validate();
    if(metadata.generatorVersion<1||metadata.generatorVersion>terrainGenerationVersion) throw std::runtime_error("Unsupported terrain generator version");
}
binary::Writer encodeRegion(Position region,const std::vector<const Chunk*>& chunks,const BlockRegistry& registry) {
    std::map<BlockId,std::uint16_t> palette;
    for(const auto* chunk:chunks) for(std::size_t i=0;i<chunkVolume;++i) palette.try_emplace(chunk->get(local(i)),0);
    binary::Writer out;out.raw("VCREGION");out.u32(worldFormatVersion);position(out,region);
    out.u32(static_cast<std::uint32_t>(palette.size()));
    std::uint32_t next=0;
    for(auto& [id,index]:palette) {
        index=static_cast<std::uint16_t>(next++);
        const auto& name=registry.definition(id).identifier;
        if(name.size()>256) throw std::runtime_error("Saved block identifier exceeds 256 bytes");
        out.text(name);
    }
    out.u32(static_cast<std::uint32_t>(chunks.size()));
    for(const auto* chunk:chunks) {
        position(out,chunk->position());
        std::vector<std::pair<std::uint16_t,std::uint16_t>> runs;
        for(std::size_t i=0;i<chunkVolume;++i) {
            const auto value=palette.at(chunk->get(local(i)));
            if(!runs.empty()&&runs.back().first==value) ++runs.back().second;
            else runs.emplace_back(value,1);
        }
        out.u32(static_cast<std::uint32_t>(runs.size()));
        for(const auto& [value,length]:runs) { out.u16(value);out.u16(length); }
    }
    return out;
}
void decodeRegion(std::span<const std::uint8_t> bytes,Position region,World& world,BlockRegistry& registry) {
    auto in=binary::Reader::checked(bytes);in.magic("VCREGION");version(in);
    if(position(in)!=region) throw std::runtime_error("Save region coordinate mismatch");
    const auto paletteSize=in.u32();
    if(!paletteSize||paletteSize>65536) throw std::runtime_error("Invalid saved block palette size");
    std::vector<BlockId> palette;palette.reserve(paletteSize);
    std::set<std::string> names;
    for(std::uint32_t i=0;i<paletteSize;++i) {
        auto name=in.text();
        if(name.size()>256||!BlockRegistry::validIdentifier(name)||!names.insert(name).second)
            throw std::runtime_error("Invalid or duplicate saved block identifier");
        const auto id=registry.find(name);
        // Unknown content remains solid and visibly magenta, retaining its exact key
        // when saved again. It never silently changes to air or an unrelated ID.
        palette.push_back(id?*id:registry.add({std::move(name),true,{1,0,1},{},true}));
    }
    const auto count=in.u32();
    if(!count||count>64||count>maxChunks-world.chunks().size()) throw std::runtime_error("Invalid saved region chunk count");
    for(std::uint32_t n=0;n<count;++n) {
        const auto p=position(in);
        if(regionPosition(p)!=region||world.findChunk(p)) throw std::runtime_error("Duplicate or misplaced saved chunk");
        Chunk chunk(p);
        const auto runs=in.u32();
        if(!runs||runs>chunkVolume) throw std::runtime_error("Invalid chunk run count");
        std::size_t index=0;
        for(std::uint32_t run=0;run<runs;++run) {
            const auto value=in.u16(),length=in.u16();
            if(value>=palette.size()||!length||length>chunkVolume-index) throw std::runtime_error("Invalid chunk palette run");
            for(std::uint16_t i=0;i<length;++i) chunk.set(local(index++),palette[value]);
        }
        if(index!=chunkVolume) throw std::runtime_error("Saved chunk is incomplete");
        world.insert(std::move(chunk));
    }
    in.finish();
}
}
Position regionPosition(Position chunk) {
    const auto axis=[](std::int64_t v){return v/4-(v%4<0?1:0);};
    return {axis(chunk.x),axis(chunk.y),axis(chunk.z)};
}
WorldStore::WorldStore(std::filesystem::path directory):directory_(std::move(directory)) {
    if(directory_.empty()) throw std::invalid_argument("World directory must not be empty");
    std::filesystem::create_directories(directory_);
    lock_=std::make_unique<FileLock>(directory_/".session.lock");
}
WorldStore::~WorldStore()=default;
std::optional<WorldArchive> WorldStore::load(BlockRegistry& registry) const {
    const auto available=revisions(directory_/"snapshots");
    if(available.empty()) return std::nullopt;
    const auto revision=available.back();
    const auto root=directory_/"snapshots"/std::to_string(revision);
    const auto bytes=readFile(root/"world.vcw",maxManifestBytes);
    auto in=binary::Reader::checked(bytes);in.magic("VCWORLD1");version(in);
    WorldMetadata metadata;
    metadata.generatorVersion=in.u32();metadata.seed=in.u64();
    metadata.bounds.minY=in.i64();metadata.bounds.maxY=in.i64();validate(metadata);
    const auto count=in.u32(),regionCount=in.u32();
    if(count>maxChunks||regionCount>maxRegions||regionCount>count||(count>0&&regionCount==0)) throw std::runtime_error("Invalid world manifest counts");
    std::set<Position> regions;
    for(std::uint32_t i=0;i<regionCount;++i)
        if(!regions.insert(position(in)).second) throw std::runtime_error("Duplicate save region");
    const auto gameSize=in.u32();
    if(gameSize>65536) throw std::runtime_error("Saved game data exceeds 64 KiB");
    auto gameData=in.raw(gameSize);in.finish();
    World world(metadata.bounds);
    auto resolved=registry;
    for(const auto p:regions) {
        const auto regionBytes=readFile(root/regionName(p),maxRegionBytes);
        decodeRegion(regionBytes,p,world,resolved);
    }
    if(world.chunks().size()!=count) throw std::runtime_error("Save chunk count does not match manifest");
    registry=std::move(resolved);
    return WorldArchive{metadata,std::move(world),std::move(gameData),revision};
}
std::uint64_t WorldStore::save(const World& world,const BlockRegistry& registry,
        const WorldMetadata& metadata,std::string_view gameData) {
    validate(metadata);
    if(metadata.bounds.minY!=world.bounds().minY||metadata.bounds.maxY!=world.bounds().maxY)
        throw std::invalid_argument("Save metadata height bounds do not match world");
    if(world.chunks().size()>maxChunks||gameData.size()>65536) throw std::runtime_error("World exceeds snapshot format limits");
    const auto root=directory_/"snapshots";std::filesystem::create_directories(root);
    auto available=revisions(root);
    if(!available.empty()&&available.back()==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("World save revision exhausted");
    const auto revision=available.empty()?1:available.back()+1;
    const auto staging=root/(".writing-"+std::to_string(revision));
    std::filesystem::remove_all(staging);
    std::filesystem::create_directory(staging);
    try {
        std::map<Position,std::vector<const Chunk*>> regions;
        for(const auto& [p,chunk]:world.chunks()) regions[regionPosition(p)].push_back(&chunk);
        for(const auto& [p,chunks]:regions) {
            auto out=encodeRegion(p,chunks,registry);writeFile(staging/regionName(p),out);
        }
        binary::Writer out;out.raw("VCWORLD1");out.u32(worldFormatVersion);out.u32(metadata.generatorVersion);
        out.u64(metadata.seed);out.i64(metadata.bounds.minY);out.i64(metadata.bounds.maxY);
        out.u32(static_cast<std::uint32_t>(world.chunks().size()));out.u32(static_cast<std::uint32_t>(regions.size()));
        for(const auto& [p,chunks]:regions) { (void)chunks;position(out,p); }
        out.u32(static_cast<std::uint32_t>(gameData.size()));out.raw(gameData);writeFile(staging/"world.vcw",out);
        // A new destination name works on Windows as well as Linux. Never replace
        // individual live files or expose a manifest referring to half-written data.
        std::filesystem::rename(staging,root/std::to_string(revision));
    } catch(...) {
        std::error_code ignored;std::filesystem::remove_all(staging,ignored);throw;
    }
    available.push_back(revision);
    while(available.size()>2) {
        std::error_code ignored;std::filesystem::remove_all(root/std::to_string(available.front()),ignored);
        available.erase(available.begin());
    }
    return revision;
}
}
