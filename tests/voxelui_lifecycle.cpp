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
        {"descriptor_sets",m.descriptorSets},{"staging_buffers",m.stagingBuffers}}) {
        std::cout<<(i++?",":"")<<'"'<<name<<"\":{\"live\":"<<c.live<<",\"peak\":"<<c.peak<<",\"created\":"<<c.created<<'}';
    }
    std::cout<<"}}\n";
}
void released(const vc::GpuResourceMetrics& m) {
    for(auto count:{m.buffers,m.images,m.imageViews,m.memory,m.samplers,m.descriptorPools,m.descriptorSets,m.stagingBuffers})
        require(count.live==0,"First-party Vulkan resources remain after shutdown");
}
}
int main(int argc,char** argv) {
    try {
        require(argc>=3,"Usage: vc_voxelui_lifecycle sdl|software|vulkan|bridge ITERATIONS [--require-validation]");
        const std::string mode=argv[1];const int iterations=std::stoi(argv[2]);require(iterations>=1&&iterations<=10000,"Invalid iteration count");
        const bool validation=argc==4&&std::string_view(argv[3])=="--require-validation";
        require(argc==3||validation,"Unknown option");
        require(mode=="sdl"||mode=="software"||mode=="vulkan"||mode=="bridge","Unsupported lifecycle mode (native backend is not implemented)");
        if(mode=="sdl") {
            vc::Window window(640,480);for(int i=0;i<iterations;++i) { SDL_PumpEvents();(void)window.poll(); }
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
            vc::client::DeclarativeUi ui(catalog,assets/"fonts","localhost","Player");
            vc::GpuResourceMetrics warm;
            for(int i=0;i<iterations;++i) {
                const auto input=window.poll();if(input.resized) renderer.requestResize();
                const auto [w,h]=window.pixelSize();if(!w||!h) continue;
                if(mode=="bridge") {
                    if(i%2) ui.update(vc::client::ApplicationScreen::MainMenu,"",w,h,1);
                    else ui.updateGameplay(vc::UiScreen::Pause,true,static_cast<unsigned>(i%20+1),false,w,h,1);
                    renderer.setUi(ui.draw());if(i%50==0) ui.reload();
                }
                renderer.draw(vc::Camera{});
                if(i==iterations/2) warm=renderer.resourceMetrics();
            }
            resources(warm);resources(renderer.resourceMetrics());
            renderer.clearUi();renderer.draw(vc::Camera{});renderer.shutdown();
            const auto counts=renderer.resourceMetrics();resources(counts);released(counts);
            const auto report=renderer.validationReport();
            require(!validation||(report.enabled&&!report.errors&&!report.warnings),"Vulkan lifecycle validation failed");
            std::cout<<"Validation enabled: "<<report.enabled<<'\n';
        }
        std::cout<<mode<<" lifecycle complete ("<<iterations<<" iterations)\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
