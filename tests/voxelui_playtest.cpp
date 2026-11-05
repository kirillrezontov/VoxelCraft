#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "engine/voxel/mesher.hpp"
#include "game/content/atlas.hpp"
#include "game/content/blocks.hpp"
#include "game/player/camera.hpp"
#include "engine/core/paths.hpp"
#include <voxelui/runtime.hpp>
#include <SDL3/SDL_main.h>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char* text) { if(!value) throw std::runtime_error(text); }
void checkUi(const voxelui::Runtime& ui,bool value,const char* message) {
    if(!value) for(const auto& d:ui.diagnostics()) std::cerr<<d.source<<":"<<d.line<<": "<<d.message<<'\n';
    require(value,message);
}
}
int main(int argc,char** argv) {
    try {
        const auto output=vc::utf8Path(argc>1?argv[1]:"voxelui-captures");std::filesystem::create_directories(output);
        vc::Window window(960,640);vc::VulkanRenderer renderer(window,vc::builtinAtlas(),true,false);renderer.clearSession();
        vc::Camera camera;camera.position={18,24,42};camera.yaw=0;camera.pitch=-.45f;
        voxelui::Runtime ui;
        ui.resources().mountDirectory("ui",window.assetPath()/"fonts");
        const std::string svg=R"(<svg viewBox="0 0 90 60"><path fill="#e1b263" d="M0 0H90V60H0Z M15 15H75V45H15Z"/><path fill="#596b57" d="M20 20H70V40H20Z"/></svg>)";
        ui.resources().mount([svg](std::string_view id)->std::optional<std::vector<std::uint8_t>> {
            if(id=="host:mark.svg") return std::vector<std::uint8_t>(svg.begin(),svg.end());
            return {};
        });
        ui.data().set("status","UI without an active world");
        checkUi(ui,ui.loadXml(R"(<Screen version="1" id="gpu-host" style="background:transparent;font-family:ui:DejaVuSans.ttf;color:#f7e8cc;font-size:20dp">
          <Column id="panel" style="width:460dp;height:240dp;padding:18dp;gap:12dp;background:#383e43;border-color:#b6b1a1;border-width:3dp;border-radius:8dp">
            <Text value="{status}" style="font-size:26dp"/>
            <Vector source="host:mark.svg" style="width:60dp;height:40dp"/>
            <Text>Vulkan · Привет · owning DrawList</Text>
            <Button id="sample" style="height:48dp;background:#8a6f43;border-color:#ddba78;border-width:2dp">Shared world + UI frame</Button>
          </Column>
        </Screen>)"),"Backend fixture XML failed");
        ui.data().set("status","UI without an active world");
        auto update=[&](float scale=1.f) {
            const auto [width,height]=window.pixelSize();if(!width||!height) return;
            ui.update({1.f/60,scale,1,{float(width),float(height)}});require(!voxelui::hasErrors(ui.diagnostics()),"Fixture UI diagnostic");renderer.setUi(ui.buildDrawList());
        };
        auto frame=[&](const std::filesystem::path& capture={}) {
            const auto input=window.poll();require(!input.quit,"Playtest interrupted");if(input.resized) renderer.requestResize();update();return renderer.draw(camera,capture);
        };
        for(int i=0;i<8;++i) { SDL_PumpEvents();(void)frame();vc::Window::idle(); }
        require(frame(output/"ui-only.ppm"),"UI-only capture was not rendered");
        const auto blocks=vc::builtinBlocks();vc::voxel::World world;
        vc::voxel::Chunk chunk({0,0,0});
        for(int x=0;x<32;++x) for(int z=0;z<32;++z) for(int y=0;y<8;++y) chunk.set({x,y,z},blocks.require(y==7?"voxelcraft:grass":"voxelcraft:stone"));
        world.insert(std::move(chunk));const auto mesh=vc::voxel::meshChunk(*world.findChunk({0,0,0}),world,blocks,vc::builtinAtlasLayout());renderer.upload(std::span(&mesh,1));
        ui.data().set("status","Same device, world and UI");require(frame(output/"world-ui.ppm"),"World/UI capture failed");require(renderer.drawnMeshCount()>0,"World mesh was not composed");
        // Measure the complete host path too: mutation, input polling, runtime,
        // owning commands, CPU surface raster/upload and actual world/UI submit.
        for(bool changed:{false,true}) {
            const auto begin=std::chrono::steady_clock::now();
            for(int i=0;i<60;++i) {
                if(changed) ui.data().set("status","Measured frame "+std::to_string(i));
                require(frame(),"Measured world/UI frame was not submitted");
                if(!changed) require(renderer.uiMetrics().uploadBytes==0,"Unchanged frame uploaded a UI surface");
            }
            const auto mean=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/60;
            const auto measured=renderer.uiMetrics();
            std::cout<<(changed?"Changed":"Unchanged")<<" world/UI frame mean "<<mean<<" us (60); last surface prepare "
                <<measured.prepareMicroseconds<<" us, upload "<<measured.uploadBytes<<" bytes\n";
        }
        // Replace the CPU screen while the previous submitted frame still owns GPU data.
        for(int cycle=0;cycle<12;++cycle) { ui.data().set("status","Screen replacement "+std::to_string(cycle));ui.close();checkUi(ui,ui.loadXml(R"(<Screen version="1" id="replacement" style="font-family:ui:DejaVuSans.ttf;color:#fff"><Text value="{status}" style="font-size:30dp"/></Screen>)"),"Screen reload failed");(void)frame(); }
        renderer.clearUi();(void)renderer.draw(camera);update();
        window.resize(1180,760);renderer.requestResize();for(int i=0;i<8;++i) { (void)frame();vc::Window::idle(); }
        require(frame(output/"resized.ppm"),"Resize capture failed");
        // Minimize/restore assertions run in Client.Sessions.WindowLifecycle.
        // Keep resource reload and retirement coverage independent of compositor support.
        const std::string reloadXml=R"(<Screen version="1" id="reload" style="font-family:ui:DejaVuSans.ttf;color:#fff"><Text value="{status}" style="font-size:30dp"/></Screen>)";
        ui.resources().mount([reloadXml](std::string_view id)->std::optional<std::vector<std::uint8_t>> {
            if(id=="host:screen.xml") return std::vector<std::uint8_t>(reloadXml.begin(),reloadXml.end());
            return {};
        });
        checkUi(ui,ui.load("host:screen.xml"),"Resource screen load failed");
        ui.resources().invalidate("ui:DejaVuSans.ttf");checkUi(ui,ui.reload(),"Resource reload failed");
        require(frame(output/"reloaded.ppm"),"Resource reload capture failed");
        for(int i=0;i<3;++i) (void)frame();
        const auto metrics=renderer.uiMetrics();require(metrics.drawCalls>0,"No GPU UI draws recorded");require(metrics.uploadBytes==0&&metrics.tessellations==0,"Warm host UI frame recreated resources");
        std::cout<<"UI prepare "<<metrics.prepareMicroseconds<<" us; GPU "<<metrics.gpuMilliseconds<<" ms, timing available "<<metrics.gpuTimingAvailable<<"; draws "<<metrics.drawCalls<<"; residency "<<metrics.residentBytes<<" bytes\n";
        renderer.clearSession();renderer.shutdown();const auto validation=renderer.validationReport();require(validation.enabled&&!validation.errors&&!validation.warnings,"Host Vulkan validation errors/warnings including shutdown");
        std::cout<<"Shared Vulkan UI-only/world composition, repeated screen replacement, resize request, resource reload and shutdown passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
