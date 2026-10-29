#include "engine/core/log.hpp"
#include "engine/voxel/mesh_queue.hpp"
#include "engine/voxel/mesh_worker.hpp"
#include "engine/voxel/streaming.hpp"
#include "game/content/blocks.hpp"
#include "game/content/atlas.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

namespace {
using Clock=std::chrono::steady_clock;
double milliseconds(Clock::duration elapsed) { return std::chrono::duration<double,std::milli>(elapsed).count(); }
struct Measurement {
    std::vector<double> calls;
    double wall{};
    std::size_t chunks{},vertices{},indices{};
    void add(std::span<const vc::Mesh> meshes) {
        chunks+=meshes.size();for(const auto& mesh:meshes) { vertices+=mesh.vertices.size();indices+=mesh.indices.size()+mesh.translucentIndices.size(); }
    }
    void report(const char* name) {
        std::sort(calls.begin(),calls.end());double total=0;for(const auto value:calls) total+=value;
        const auto percentile=[&](double p){return calls[static_cast<std::size_t>(p*static_cast<double>(calls.size()-1))];};
        std::cout<<name<<": "<<chunks<<" chunks, "<<vertices<<" vertices, "<<indices/3<<" triangles; "
            <<"main-thread calls median "<<percentile(0.5)<<" ms, p95 "<<percentile(0.95)<<" ms, max "<<calls.back()
            <<" ms; main CPU "<<total<<" ms; wall "<<wall<<" ms\n";
    }
};
}
int main() {
    try {
        using namespace vc::voxel;
        const auto blocks=vc::builtinBlocks();const auto atlas=vc::builtinAtlasLayout();const TerrainGenerator generator(42,{},blocks,3);
        World world;const Position player{0,40,0};const auto positions=streamingRegion(player,{},2,3);
        for(const auto p:positions) world.insert(generator.generate(p));
        Lighting lighting;const auto dirty=world.takeDirtyChunks();(void)lighting.refresh(world,blocks,dirty);
        Measurement culled,greedy;
        for(const bool merge:{false,true}) {
            auto& measured=merge?greedy:culled;const auto runStart=Clock::now();
            for(const auto& [position,chunk]:world.chunks()) {
                (void)position;const auto before=Clock::now();
                const auto mesh=meshChunk(chunk,world,blocks,atlas,&lighting,merge);
                measured.calls.push_back(milliseconds(Clock::now()-before));measured.add(std::span(&mesh,1));
            }
            measured.wall=milliseconds(Clock::now()-runStart);
        }
        std::cout<<std::fixed<<std::setprecision(3);culled.report("Unit faces (reference)");greedy.report("Greedy opaque faces");
        std::cout<<"Chunk cell payload "<<chunkVolume*sizeof(BlockId)<<" bytes; Vertex "<<sizeof(vc::Vertex)<<" bytes.\n";
        Measurement synchronous,background;ChunkMeshQueue direct;direct.invalidate(positions);auto start=Clock::now();
        while(direct.pendingCount()) {
            const auto before=Clock::now();const auto meshes=direct.rebuild(world,blocks,atlas,player,2,&lighting);
            synchronous.calls.push_back(milliseconds(Clock::now()-before));synchronous.add(meshes);
        }
        synchronous.wall=milliseconds(Clock::now()-start);
        BackgroundMeshQueue worker(blocks,atlas);worker.invalidate(positions);start=Clock::now();
        while(worker.pendingCount()) {
            const auto before=Clock::now();const auto meshes=worker.update(world,player,2,&lighting);
            background.calls.push_back(milliseconds(Clock::now()-before));background.add(meshes);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        background.wall=milliseconds(Clock::now()-start);
        std::cout<<std::fixed<<std::setprecision(3);synchronous.report("Synchronous (budget 2)");background.report("Background (snapshot budget 2)");
        if(synchronous.chunks!=background.chunks||synchronous.vertices!=background.vertices||synchronous.indices!=background.indices)
            throw std::runtime_error("Meshing benchmark produced different geometry counts");
        std::cout<<"CPU-only fixed-seed Debug/Release comparison; excludes rendering and GPU upload. Background wall time includes 1 ms polling sleeps.\n";return 0;
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
