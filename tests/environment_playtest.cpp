#include "engine/core/log.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/lighting.hpp"
#include "engine/voxel/mesh_queue.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include "game/player/camera.hpp"
#include "game/simulation/day_cycle.hpp"
#include <SDL3/SDL_main.h>
#include <fstream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
double brightness(const std::filesystem::path& path,bool center=false) {
    std::ifstream file(path,std::ios::binary);std::string magic;unsigned width{},height{},maximum{};
    file>>magic>>width>>height>>maximum;file.get();require(magic=="P6"&&maximum==255&&width&&height,"Invalid captured PPM");
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*3);
    require(static_cast<bool>(file.read(reinterpret_cast<char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()))),"Truncated capture");
    double total=0;std::size_t samples=0;
    for(unsigned y=center?height/4:0;y<(center?height*3/4:height);++y) for(unsigned x=center?width/4:0;x<(center?width*3/4:width);++x) {
        const auto p=(static_cast<std::size_t>(y)*width+x)*3;total+=pixels[p]+pixels[p+1]+pixels[p+2];samples+=3;
    }
    return total/static_cast<double>(samples);
}
vc::Atmosphere atmosphere(const vc::DayCycle& time) {
    const auto daylight=time.daylight();vc::Atmosphere result;
    result.skyColor=daylight.skyColor;result.sunDirection=daylight.sunDirection;result.sunlight=daylight.sunlight;result.ambient=daylight.ambient;
    result.fogStart=20;result.fogEnd=60;return result;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==5,"Environment test requires four capture paths");
        vc::Window window;vc::VulkanRenderer renderer(window,vc::builtinAtlas(),true);const auto registry=vc::builtinBlocks();
        vc::voxel::World world;const auto stone=registry.require("voxelcraft:stone"),water=registry.require("voxelcraft:water"),lamp=registry.require("voxelcraft:lamp");
        for(int x=-1;x<=1;++x) for(int z=-1;z<=0;++z) world.insert(vc::voxel::Chunk({x,0,z}));
        for(int x=-8;x<=8;++x) for(int z=-8;z<=8;++z) { world.setBlock({x,0,z},stone);world.setBlock({x,5,z},stone); }
        for(int y=1;y<5;++y) for(int x=-8;x<=8;++x) world.setBlock({x,y,-8},stone);
        for(int y=1;y<5;++y) for(int z=-8;z<=8;++z) { world.setBlock({-8,y,z},stone);world.setBlock({8,y,z},stone); }
        for(int x=3;x<=6;++x) for(int z=1;z<=5;++z) world.setBlock({x,1,z},water);
        world.setBlock({33,1,-3},water);world.setBlock({0,1,0},lamp);
        vc::voxel::Lighting light;vc::voxel::ChunkMeshQueue queue;
        const auto remesh=[&] {
            const auto dirty=world.takeDirtyChunks();const auto relight=light.refresh(world,registry,dirty);
            queue.invalidate(dirty);queue.invalidate(relight);
            const auto meshes=queue.rebuild(world,registry,vc::builtinAtlasLayout(),{0,2,0},128,&light);renderer.updateMeshes(meshes);
            require(!queue.pendingCount(),"Environment mesh work was not drained");
        };
        remesh();renderer.setHud({}, {},false);vc::Camera camera;camera.position={12.5,12.5,14.5};camera.yaw=-0.65;camera.pitch=-0.5;
        const auto frame=[&](const std::filesystem::path& screenshot=std::filesystem::path{}) {
            const auto input=window.poll();require(!input.quit,"Environment test interrupted");if(input.resized) renderer.requestResize();
            return renderer.draw(camera,screenshot);
        };
        for(int i=0;i<8;++i) frame();
        const auto capture=[&](const std::filesystem::path& path) {
            for(int i=0;i<8;++i) if(frame(path)) return;
            throw std::runtime_error("Environment capture was never presented");
        };
        const auto day=vc::utf8Path(argv[1]),night=vc::utf8Path(argv[2]),lit=vc::utf8Path(argv[3]),waterImage=vc::utf8Path(argv[4]);
        renderer.setAtmosphere(atmosphere(vc::DayCycle(300)));capture(day);
        renderer.setAtmosphere(atmosphere(vc::DayCycle(900)));capture(night);
        require(brightness(day)>brightness(night)*1.5,"Day/night did not materially change rendered brightness");
        camera.position={0.5,2.62,4.5};camera.yaw=0;camera.pitch=-0.12;
        world.setBlock({0,1,0},vc::voxel::air);remesh();const auto unlit=lit.parent_path()/"environment-unlit.ppm";capture(unlit);
        world.setBlock({0,1,0},lamp);remesh();capture(lit);
        require(brightness(lit,true)>brightness(unlit,true)*1.2,"Propagated lamp light did not illuminate the enclosed room");
        camera.position={33.5,3.5,2.5};camera.yaw=0;camera.pitch=-0.3;renderer.setAtmosphere(atmosphere(vc::DayCycle(300)));capture(waterImage);
        require(renderer.drawnMeshCount()==1&&renderer.drawnTriangleCount()==12,"Transparent-only chunk did not draw independently");
        const auto previous=renderer.meshCount();world.setBlock({33,1,-3},vc::voxel::air);remesh();require(renderer.meshCount()+1==previous,"Transparent-only GPU mesh was not removed");
        renderer.shutdown();const auto validation=renderer.validationReport();require(validation.enabled&&!validation.errors&&!validation.warnings,"Environment rendering reported Vulkan diagnostics");
        vc::log("Environment playtest passed: day/night brightness, blocked-sky lamp propagation, alpha water, transparent-only draw/removal; zero Vulkan diagnostics");return 0;
    } catch(const std::exception& error) { vc::log(error.what());return 1; }
}
