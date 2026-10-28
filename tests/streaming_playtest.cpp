#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/streaming.hpp"
#include "engine/voxel/mesh_queue.hpp"
#include "engine/voxel/storage.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include "game/player/camera.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "tests/temp_directory.hpp"
#include <SDL3/SDL_main.h>
#include <cmath>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
vc::voxel::Position block(const vc::Camera& camera) {
    return {static_cast<std::int64_t>(std::floor(camera.position.x)),static_cast<std::int64_t>(std::floor(camera.position.y)),
        static_cast<std::int64_t>(std::floor(camera.position.z))};
}
}
int main(int argc,char** argv) {
    try {
        vc::Window window;vc::VulkanRenderer renderer(window,vc::builtinAtlas(),true);
        auto registry=vc::builtinBlocks();vc::voxel::World world;
        const vc::voxel::TerrainGenerator generator(42,{},registry);
        auto streamer=std::make_unique<vc::voxel::ChunkStreamer>(world,generator,1);
        vc::voxel::ChunkMeshQueue queue;vc::Camera camera;camera.position={-0.5,66.5,6.5};camera.yaw=0;camera.pitch=-0.65;
        const auto stone=registry.require("voxelcraft:stone");
        const auto settle=[&] {
            const auto deadline=vc::Window::seconds()+15;
            do {
                const auto input=window.poll();require(!input.quit,"Streaming playtest interrupted");
                if(input.resized) renderer.requestResize();
                streamer->tick(block(camera));queue.collect(world);
                const auto meshes=queue.rebuild(world,registry,vc::builtinAtlasLayout(),block(camera),2);renderer.updateMeshes(meshes);
                renderer.draw(camera);
                require(world.chunks().size()<=54,"Streaming resident count exceeded configured bound");
                if(streamer->settled()&&!queue.pendingCount()) {
                    std::size_t expectedMeshes=0,expectedTriangles=0;
                    for(const auto& [p,chunk]:world.chunks()) {
                        (void)p;const auto mesh=vc::voxel::meshChunk(chunk,world,registry,vc::builtinAtlasLayout());
                        if(!mesh.indices.empty()) { ++expectedMeshes;expectedTriangles+=mesh.indices.size()/3; }
                    }
                    require(renderer.meshCount()==expectedMeshes&&renderer.triangleCount()==expectedTriangles,
                        "Streamed GPU resources differ from fully rebuilt resident-world meshes");
                    return;
                }
            } while(vc::Window::seconds()<deadline);
            throw std::runtime_error("Streaming playtest did not settle within 15 seconds");
        };
        settle();const vc::voxel::Position editA{-1,60,-1};
        require(world.setBlock(editA,stone),"First streaming edit failed");settle();
        camera.position={1024.5,66.5,-1024.5};settle();require(!world.loaded(editA),"Distant chunk was not evicted");
        require(streamer->cachedCount()==1,"Evicted edit was not retained");
        const vc::voxel::Position editB{1024,60,-1025};require(world.setBlock(editB,stone),"Second streaming edit failed");settle();
        camera.position={-512.5,-200,512.5};settle();
        require(world.loaded({-513,-200,512}),"Vertical streaming did not load underground camera region");
        const auto snapshot=streamer->snapshot();require(snapshot.chunks().size()==2,"Snapshot did not retain exactly edited chunks");
        vc::test::TempDirectory temp;
        {
            vc::voxel::WorldStore store(temp.path());store.save(snapshot,registry,{42,{},1});
            auto archive=store.load(registry);require(archive.has_value(),"Streamed snapshot did not load");
            require(archive->world.block(editA)==stone&&archive->world.block(editB)==stone,"Evicted edits were not persisted");
            streamer.reset();world=std::move(archive->world);renderer.upload({});queue={};
        }
        streamer=std::make_unique<vc::voxel::ChunkStreamer>(world,generator,1);
        std::vector<vc::voxel::Position> retained;
        for(const auto& [p,chunk]:world.chunks()) { (void)chunk;retained.push_back(p); }
        streamer->retain(retained);camera.position={-0.5,66.5,6.5};settle();
        require(world.block(editA)==stone,"Edited terrain did not reappear on revisit after reload");
        require(streamer->snapshot().block(editB)==stone,"Distant retained edit was lost on revisit");
        vc::CreativeInventory inventory(registry);const auto [width,height]=window.pixelSize();
        auto hud=vc::sandboxHud(inventory,registry,vc::builtinAtlasLayout(),width,height,false,0,"STREAMING AND PERSISTENT REVISIT VERIFIED");
        renderer.setHud(hud.colored,hud.textured,true,hud.foreground);
        if(argc>1) require(renderer.draw(camera,vc::utf8Path(argv[1])),"Streaming screenshot could not be presented");
        renderer.shutdown();const auto validation=renderer.validationReport();
        require(validation.enabled&&validation.errors==0&&validation.warnings==0,"Streaming reported Vulkan diagnostics");
        vc::log("Streaming playtest passed: bounded horizontal/vertical residency, edit eviction, GPU removal/replacement, save/reload/revisit; zero Vulkan diagnostics");
        return 0;
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
