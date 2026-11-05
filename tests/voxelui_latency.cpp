#include "client/application.hpp"
#include "client/declarative_ui.hpp"
#include "engine/core/frame_profile.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "game/player/camera.hpp"
#include "tests/temp_directory.hpp"
#include <SDL3/SDL_main.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void push(SDL_Event event) { event.common.timestamp=1;require(SDL_PushEvent(&event),"SDL event injection failed"); }
void key(SDL_WindowID window,SDL_Scancode code,SDL_Keymod mod=SDL_KMOD_NONE) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.windowID=window;e.key.scancode=code;e.key.mod=mod;push(e);
    e.type=SDL_EVENT_KEY_UP;push(e);
}
void motion(SDL_WindowID window,float x,float y) {
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;e.motion.windowID=window;e.motion.x=x;e.motion.y=y;push(e);
}
void button(SDL_WindowID window,bool down,float x,float y) {
    SDL_Event e{};e.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.windowID=window;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;push(e);
}
}
int main(int argc,char** argv) {
    try {
        require(argc>=4,"Usage: vc_voxelui_latency OUTPUT.jsonl on|off FRAMES [--require-validation]");
        const bool vsync=std::string_view(argv[2])=="on";
        require(vsync||std::string_view(argv[2])=="off","VSync must be on or off");
        const int frames=std::stoi(argv[3]);require(frames>=30&&frames<=10000,"FRAMES must be 30..10000");
        const bool validation=argc==5&&std::string_view(argv[4])=="--require-validation";
        require(argc==4||validation,"Unknown option");
        std::ofstream output(argv[1]);require(bool(output),"Cannot open benchmark output");
        vc::profile::enable(true); // Enable before constructing the timestamp pool.
        vc::test::TempDirectory temporary;
        vc::client::Options options;options.width=960;options.height=700;options.vsync=vsync;
        options.requireValidation=validation;options.noMods=true;options.noSettings=true;
        options.radius=1;options.generatorVersion=1;options.seed=42;options.survival=true;
        options.worldDirectory=temporary.path()/"world";
        vc::client::ClientApplication app(options);const auto id=app.windowId();
        auto* window=SDL_GetWindowFromID(id);require(window,"No application window");
        SDL_SetEventFilter([](void*,SDL_Event* event) {
            const bool input=(event->type>=SDL_EVENT_KEY_DOWN&&event->type<=SDL_EVENT_TEXT_INPUT)
                ||(event->type>=SDL_EVENT_MOUSE_MOTION&&event->type<=SDL_EVENT_MOUSE_WHEEL);
            return !input||event->common.timestamp==1;
        },nullptr);
        voxelui::Runtime geometry;
        auto geometryResources=app.resources();
        geometry.resources().mountDirectory("ui",std::filesystem::path(SDL_GetBasePath())/"assets/fonts");
        geometry.resources().mount([&](std::string_view resource) { return geometryResources.load(resource); });
        geometry.data().set("menu.address","localhost");geometry.data().set("menu.name","Player");
        for(const auto* action:{"voxelcraft:play","voxelcraft:connect","voxelcraft:quit"})
            geometry.actions().registerAction(action,{{},{},[](const auto&)->std::optional<std::string>{ return {}; }});
        if(!geometry.load("voxelcraft:ui/main-menu.xml")) {
            for(const auto& diagnostic:geometry.diagnostics()) std::cerr<<diagnostic.source<<": "<<diagnostic.message<<'\n';
            throw std::runtime_error("Benchmark hit-test geometry failed");
        }
        const auto point=[&](std::string_view control) {
            int w{},h{},lw{},lh{};require(SDL_GetWindowSizeInPixels(window,&w,&h)&&SDL_GetWindowSize(window,&lw,&lh),"Window size unavailable");
            voxelui::FrameInfo frame;frame.viewport={float(w),float(h)};frame.uiScale=std::min({1.f,float(w)/720.f,float(h)/700.f});
            geometry.update(frame);const auto* node=geometry.find(control);require(node,"Missing benchmark control");
            return voxelui::Vec2{(node->bounds.x+node->bounds.w/2)*frame.uiScale*float(lw)/float(w),
                (node->bounds.y+node->bounds.h/2)*frame.uiScale*float(lh)/float(h)};
        };
        const auto tick=[&] { app.pollFrame();require(app.running(),"Application closed during benchmark"); };
        const auto capture=[&](const char* name) {
            const auto path=std::filesystem::path(argv[1]).parent_path()/(std::string(name)+(vsync?"-on.ppm":"-off.ppm"));
            for(int attempt=0;attempt<20;++attempt) { if(app.renderer().draw(vc::Camera{},path)) return;tick(); }
            throw std::runtime_error("Screenshot did not render");
        };
        const auto sample=[&](const char* scenario,const auto& mutate) {
            for(int i=0;i<20;++i) { mutate(i);tick(); }
            for(int i=0;i<frames;++i) {
                mutate(i);tick();int w{},h{};SDL_GetWindowSizeInPixels(window,&w,&h);
                output<<"{\"scenario\":\""<<scenario<<"\",\"width\":"<<w<<",\"height\":"<<h<<",\"vsync\":"<<(vsync?"true":"false")
                    <<",\"sample\":"<<i<<",\"timing\":";
                vc::profile::writeSample(output,vc::profile::lastFrame());output<<"}\n";
            }
            std::cout<<scenario<<" complete\n"<<std::flush;
        };
        const auto idle=[](int){};
        sample("idle_main_menu",idle);capture("main-menu");
        sample("pointer_movement",[&](int i) { motion(id,20+float(i%80),20); });
        sample("button_hover",[&](int i) { const auto p=point("play");motion(id,i%2?p.x:20.f,i%2?p.y:20.f); });
        sample("pointer_click_text_focus",[&](int i) { const auto p=point("address");button(id,i%2==0,p.x,p.y); });
        // Start from the initially focused Play button again, then focus address.
        app.activateResourcePacks({});tick();key(id,SDL_SCANCODE_TAB);tick();
        sample("text_editing",[&](int i) { key(id,SDL_SCANCODE_A,SDL_KMOD_CTRL);SDL_Event e{};
            e.type=SDL_EVENT_TEXT_INPUT;e.text.windowID=id;e.text.text=i%2?"Игрок1":"Игрок2";push(e); });
        sample("rapid_typing",[&](int i) { key(id,SDL_SCANCODE_A,SDL_KMOD_CTRL);
            for(int n=0;n<8;++n) { SDL_Event e{};e.type=SDL_EVENT_TEXT_INPUT;e.text.windowID=id;e.text.text=i%2?"a":"b";push(e); } });
        sample("screen_replacement_reload",[&](int) { app.activateResourcePacks({}); });
        require(!app.hasSession(),"Menu benchmark unexpectedly started a session");
        app.startLocal(options);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        while(app.screen()!=vc::client::ApplicationScreen::Gameplay) {
            require(std::chrono::steady_clock::now()<deadline,"Local session loading timed out");tick();
        }
        sample("ui_over_active_world",idle);capture("world-hud");
        sample("pause_resume",[&](int i) {
            if(i%2==0) key(id,SDL_SCANCODE_ESCAPE);else key(id,SDL_SCANCODE_RETURN);
        });
        vc::Input pause;pause.pause=true;app.frame(pause);capture("pause");
        sample("resize",[&](int i) {
            require(SDL_SetWindowSize(window,i%2?960:1000,i%2?700:740),"Resize request failed");
        });
        app.requestLeave();
        for(int i=0;app.hasSession()&&i<300;++i) tick();
        require(!app.hasSession(),"Session teardown failed");
        // Deterministic health changes through the same host adapter/renderer,
        // isolated from simulation. This row is explicitly a host fixture.
        vc::ResourceCatalog resources(std::filesystem::path(SDL_GetBasePath())/"assets/resources");
        vc::client::DeclarativeUi hud(resources,std::filesystem::path(SDL_GetBasePath())/"assets/fonts","localhost","Player");
        app.renderer().clearSession();
        for(int i=-20;i<frames;++i) {
            {
                vc::profile::Frame frame;
                hud.updateGameplay(vc::UiScreen::Gameplay,true,static_cast<unsigned>((i+40)%20+1),false,960,700,1);
                app.renderer().setUi(hud.draw());require(app.renderer().draw(vc::Camera{}),"Health fixture did not render");
            }
            if(i>=0) { output<<"{\"scenario\":\"health_updates_host_fixture\",\"vsync\":"<<(vsync?"true":"false")
                <<",\"sample\":"<<i<<",\"timing\":";vc::profile::writeSample(output,vc::profile::lastFrame());output<<"}\n"; }
        }
        app.shutdown();const auto report=app.validationReport();
        require(!validation||(report.enabled&&!report.errors&&!report.warnings),"Vulkan validation failed");
        std::cout<<"Validation enabled: "<<report.enabled<<", errors "<<report.errors<<", warnings "<<report.warnings<<'\n';
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
