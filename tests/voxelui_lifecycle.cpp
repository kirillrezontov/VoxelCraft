#include "client/declarative_ui.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "game/content/atlas.hpp"
#include "game/player/camera.hpp"
#include <voxelui/software.hpp>
#include <SDL3/SDL_main.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void resources(const vc::GpuResourceMetrics& m) {
    unsigned i=0;std::cout<<"{\"resources\":{";
    for(const auto& [name,c]:{std::pair{"buffers",m.buffers},{"images",m.images},{"image_views",m.imageViews},
        {"memory",m.memory},{"samplers",m.samplers},{"descriptor_pools",m.descriptorPools},
        {"descriptor_sets",m.descriptorSets},{"staging_buffers",m.stagingBuffers},{"atlas_pages",m.atlasPages},{"pipelines",m.pipelines},{"pipeline_layouts",m.pipelineLayouts},{"descriptor_layouts",m.descriptorLayouts}}) {
        std::cout<<(i++?",":"")<<'"'<<name<<"\":{\"live\":"<<c.live<<",\"peak\":"<<c.peak<<",\"created\":"<<c.created<<'}';
    }
    std::cout<<"}}\n";
}
void released(const vc::GpuResourceMetrics& m) {
    for(auto count:{m.buffers,m.images,m.imageViews,m.memory,m.samplers,m.descriptorPools,m.descriptorSets,m.stagingBuffers,m.atlasPages,m.pipelines,m.pipelineLayouts,m.descriptorLayouts})
        require(count.live==0,"First-party Vulkan resources remain after shutdown");
}
}
int main(int argc,char** argv) {
    try {
        require(argc>=3,"Usage: vc_voxelui_lifecycle sdl|software|vulkan|bridge|native ITERATIONS [--require-validation]");
        const std::string mode=argv[1];const int iterations=std::stoi(argv[2]);require(iterations>=1&&iterations<=10000,"Invalid iteration count");
        const bool validation=argc==4&&std::string_view(argv[3])=="--require-validation";
        require(argc==3||validation,"Unknown option");
        require(mode=="sdl"||mode=="software"||mode=="vulkan"||mode=="bridge"||mode=="native","Unsupported lifecycle mode");
        if(mode=="sdl") {
            require(SDL_Init(SDL_INIT_VIDEO),"SDL initialization failed");
            struct Quit { ~Quit() { SDL_Quit(); } } quit;
            const std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(
                SDL_CreateWindow("SDL-only lifecycle",640,480,SDL_WINDOW_RESIZABLE),SDL_DestroyWindow);
            require(bool(window),"SDL-only window creation failed");
            for(int i=0;i<iterations;++i) { SDL_Event event;while(SDL_PollEvent(&event)) {} }
        } else if(mode=="software") {
            const auto assets=std::filesystem::path(SDL_GetBasePath())/"assets";
            vc::ResourceCatalog catalog(assets/"resources");vc::client::DeclarativeUi ui(catalog,assets/"fonts","localhost","Player");
            voxelui::SoftwareRenderer renderer;
            for(int i=0;i<iterations;++i) {
                if(i%2) ui.update(vc::client::ApplicationScreen::MainMenu,"",640,480,1);
                else ui.updateGameplay(vc::UiScreen::Pause,true,static_cast<unsigned>(i%20+1),false,640,480,1);
                renderer.render(ui.draw(),{0,0,0,0});if(i%50==0) ui.reload();
            }
        } else {
            vc::Window window(640,480);vc::VulkanRenderer renderer(window,vc::builtinAtlas(),validation,false);renderer.clearSession();
            const auto assets=window.assetPath();vc::ResourceCatalog catalog(assets/"resources");
            std::unique_ptr<vc::client::DeclarativeUi> ui;
            if(mode=="bridge"||mode=="native") ui=std::make_unique<vc::client::DeclarativeUi>(catalog,assets/"fonts","localhost","Player");
            vc::GpuResourceMetrics warm;
            for(int i=0;i<iterations;++i) {
                const auto input=window.poll();if(input.resized) renderer.requestResize();
                const auto [w,h]=window.pixelSize();if(!w||!h) continue;
                if(mode=="bridge"||mode=="native") {
                    if(i%2) ui->update(vc::client::ApplicationScreen::MainMenu,"",w,h,1);
                    else ui->updateGameplay(vc::UiScreen::Pause,true,static_cast<unsigned>(i%20+1),false,w,h,1);
                    renderer.setUi(ui->draw());if(i%50==0) ui->reload();
                }
                renderer.draw(vc::Camera{});
                if(i==iterations/2) warm=renderer.resourceMetrics();
            }
            if(mode=="native") require(renderer.resourceMetrics().atlasPages.created>0,"native mode requested but no native atlas was created");
            const auto final=renderer.resourceMetrics();
            if(iterations>=100) {
                require(warm.buffers.live==final.buffers.live&&warm.images.live==final.images.live&&
                    warm.memory.live==final.memory.live&&warm.atlasPages.live==final.atlasPages.live&&
                    warm.descriptorSets.live==final.descriptorSets.live&&warm.pipelines.live==final.pipelines.live,
                    "Fixed UI workload did not stabilize after warmup");
            }
            resources(warm);resources(final);
            renderer.clearUi();renderer.draw(vc::Camera{});renderer.shutdown();
            const auto counts=renderer.resourceMetrics();resources(counts);released(counts);
            const auto report=renderer.validationReport();
            require(!validation||(report.enabled&&!report.errors&&!report.warnings),"Vulkan lifecycle validation failed");
            std::cout<<"Validation enabled: "<<report.enabled<<'\n';
        }
        std::cout<<mode<<" lifecycle complete ("<<iterations<<" iterations)\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
