#include "client/application.hpp"
#include "engine/core/paths.hpp"
#include "engine/platform/input.hpp"
#include "engine/platform/window.hpp"
#include "engine/renderer/vulkan_renderer.hpp"
#include "game/player/camera.hpp"
#include "game/player/save_state.hpp"
#include "game/ui/sandbox_ui.hpp"
#include "engine/voxel/storage.hpp"
#include "server/host.hpp"
#include "tests/temp_directory.hpp"
#include <SDL3/SDL_main.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
template<class Condition,class Pump>
void until(Condition condition,Pump pump) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    do {
        pump();if(condition()) return;
        vc::Window::idle();
    } while(std::chrono::steady_clock::now()<deadline);
    throw std::runtime_error("Session transition timed out");
}
}
int main(int argc,char** argv) {
    try {
        const bool windowLifecycle=argc==4&&std::string_view(argv[3])=="--window-lifecycle";
        require(argc==3||windowLifecycle,"Expected capture and example-mod directories [--window-lifecycle]");
        const auto captures=vc::utf8Path(argv[1]);std::filesystem::create_directories(captures);
        vc::test::TempDirectory temporary;
        vc::client::Options config;config.settingsFile=temporary.path()/"settings.vcs";config.noMods=true;config.requireValidation=true;
        config.vsync=false;config.radius=1;config.generatorVersion=1;config.seed=42;
        config.worldDirectory=temporary.path()/"saved-world";
        vc::client::ClientApplication app(config);auto* renderer=&app.renderer();
        const auto tick=[&] { app.frame({}); };
        tick();require(!app.hasSession(),"Main menu created a world/session");
        require(!std::filesystem::exists(config.worldDirectory),"Main menu touched a world directory");
        until([&] { return app.renderer().draw(vc::Camera{},captures/"main-menu.ppm"); },tick);
        auto* nativeWindow=SDL_GetWindowFromID(app.windowId());
        // Platform minimize/restore is a separate mandatory graphics case: a
        // compositor lacking minimize must not prevent session/network coverage.
        if(windowLifecycle) {
        for(auto [width,height]:{std::pair{800,600},std::pair{1120,720}}) {
            require(SDL_SetWindowSize(nativeWindow,width,height),"Could not request window resize");
            until([&] {int actualWidth{},actualHeight{};return SDL_GetWindowSize(nativeWindow,&actualWidth,&actualHeight)&&actualWidth==width&&actualHeight==height;},
                [&] {SDL_PumpEvents();vc::Input changed;changed.resized=true;app.frame(changed);});
            std::cout<<"Acknowledged resize "<<width<<'x'<<height<<'\n';
            until([&] {return app.renderer().draw(vc::Camera{},captures/(std::to_string(width)+".ppm"));},tick);
        }
        // Settle the initial Wayland configure/focus before requesting minimize.
        for(int i=0;i<8;++i) { SDL_PumpEvents();tick();vc::Window::idle(); }
        require(nativeWindow&&SDL_MinimizeWindow(nativeWindow),"Could not minimize menu window");
        until([&] { return (SDL_GetWindowFlags(nativeWindow)&SDL_WINDOW_MINIMIZED)!=0; },[&] { SDL_PumpEvents();tick(); });
        std::cout<<"Menu minimized\n"<<std::flush;
        require(!app.renderer().draw(vc::Camera{}),"Minimized renderer submitted UI");
        tick(); // A zero-sized menu must not create invalid font/layout geometry.
        require(SDL_RestoreWindow(nativeWindow)&&SDL_RaiseWindow(nativeWindow),"Could not restore menu window");
        until([&] { return (SDL_GetWindowFlags(nativeWindow)&SDL_WINDOW_MINIMIZED)==0; },[&] { SDL_PumpEvents();tick(); });
        vc::Input resize;resize.resized=true;app.frame(resize);

            app.shutdown();const auto report=app.validationReport();
            require(report.enabled&&!report.errors&&!report.warnings,"Window lifecycle validation failed");
            std::cout<<"Application minimize/restore and shutdown passed\n";return 0;
        }

        // Cancel before the first terrain job is accepted; the store lock must go.
        app.startLocal(config);tick();app.requestLeave();
        until([&] { return !app.hasSession(); },tick);
        { vc::voxel::WorldStore released(config.worldDirectory); }

        for(int cycle=0;cycle<3;++cycle) {
            app.startLocal(config);
            until([&] { return app.screen()==vc::client::ApplicationScreen::Gameplay; },tick);
            require(&app.renderer()==renderer,"Renderer owner changed between sessions");
            for(int i=0;i<8;++i) tick();
#if VC_HAS_VOXELUI
            require(renderer->uiMetrics().drawCalls==1&&renderer->uiMetrics().uploadBytes==0,"Idle gameplay recreated the declarative HUD surface");
            if(cycle==0) until([&] { return renderer->draw(vc::Camera{},captures/"voxelui-hud.ppm"); },tick);
#endif
            if(cycle==1) {
                const auto snapshots=config.worldDirectory/"snapshots";
                const auto backup=config.worldDirectory/"snapshots-backup";
                std::filesystem::rename(snapshots,backup);std::ofstream(snapshots)<<"block save I/O";
                app.requestLeave();tick();
                until([&] { return app.screen()==vc::client::ApplicationScreen::Gameplay; },tick);
                require(app.hasSession()&&app.renderer().meshCount()>0,"Save failure discarded the live world");
                std::filesystem::remove(snapshots);std::filesystem::rename(backup,snapshots);
            }
            // Exercise the actual pause/settings/leave controls on the first cycle.
            if(cycle==0) {
                vc::Input pause;pause.pause=true;app.frame(pause);
                int actualWidth{},actualHeight{};
                require(SDL_GetWindowSizeInPixels(nativeWindow,&actualWidth,&actualHeight),"Cannot query current session viewport");
                const vc::SandboxUiLayout layout(actualWidth,actualHeight,app.settings().uiScale);
                auto click=[&](vc::UiRect rect) { vc::Input input;input.primaryClick=true;input.cursorX=rect.x+rect.width/2;input.cursorY=rect.y+rect.height/2;app.frame(input); };
#if VC_HAS_VOXELUI
                // Resume is initially focused. Navigate the authored XML controls.
                vc::Input tab;tab.text.tab=true;app.frame(tab);
                vc::Input enter;enter.text.enter=true;app.frame(enter);
                until([&] { return app.renderer().draw(vc::Camera{},captures/"pause-settings.ppm"); },tick);
#else
                click(layout.pauseButton(1));
#endif
                click(layout.settingsButton(1));
                require(app.settings().fieldOfView==80,"Application did not receive session settings");
                const auto settings=vc::loadSettings(config.settingsFile);
                require(settings&&settings->settings==app.settings(),"Application settings did not persist");
                vc::Input back;back.closeUi=true;app.frame(back);
#if VC_HAS_VOXELUI
                until([&] { return app.renderer().draw(vc::Camera{},captures/"voxelui-pause.ppm"); },tick);
                for(int step=0;step<3;++step) app.frame(tab);
                app.frame(enter);
#else
                const auto button=layout.pauseButton(3);vc::Input leave;
                leave.primaryClick=true;leave.cursorX=button.x+button.width/2;leave.cursorY=button.y+button.height/2;
                app.frame(leave);
#endif
            } else app.requestLeave();
            until([&] { return !app.hasSession(); },tick);
            require(app.running()&&app.screen()==vc::client::ApplicationScreen::MainMenu,"Did not return to usable menu");
            require(app.renderer().meshCount()==0,"Prior session geometry survived teardown");
            vc::voxel::WorldStore reopened(config.worldDirectory);vc::voxel::BlockRegistry blocks;
            const auto archive=reopened.load(blocks);require(archive&&archive->revision==static_cast<unsigned>(cycle+1),"Session did not save/release/reload world");
        }
#if VC_HAS_VOXELUI
        auto survivalConfig=config;survivalConfig.worldDirectory=temporary.path()/"survival-world";survivalConfig.survival=true;
        app.startLocal(survivalConfig);until([&] { return app.screen()==vc::client::ApplicationScreen::Gameplay; },tick);
        for(int step=0;step<4;++step) tick();
        require(renderer->uiMetrics().drawCalls==1,"Survival session did not prepare the health HUD");
        until([&] { return renderer->draw(vc::Camera{},captures/"voxelui-survival-hud.ppm"); },tick);
        app.requestLeave();until([&] { return !app.hasSession(); },tick);
#endif
        auto modded=config;modded.worldDirectory=temporary.path()/"modded-world";
        modded.noMods=false;modded.modsDirectory=vc::utf8Path(argv[2]);
        app.startLocal(modded);until([&] { return app.screen()==vc::client::ApplicationScreen::Gameplay; },tick);
        for(int i=0;i<4;++i) tick();
        app.requestLeave();until([&] { return !app.hasSession(); },tick);
        { vc::voxel::WorldStore store(modded.worldDirectory);vc::voxel::BlockRegistry blocks;
          const auto archive=store.load(blocks);require(archive&&!vc::decodePlayer(archive->gameData).mods.empty(),"Session-specific mod content was not saved"); }
        require(app.settings().fieldOfView==80,"Settings were reset by session replacement");
        auto bad=config;bad.seedSpecified=true;bad.seed=999;
        app.startLocal(bad);until([&] { return !app.hasSession(); },tick);
        require(app.screen()==vc::client::ApplicationScreen::Error,"Failed load did not reach recoverable error screen");
        until([&] { return app.renderer().draw(vc::Camera{},captures/"load-error.ppm"); },tick);

        vc::server::HostConfig hostConfig;hostConfig.port=0;hostConfig.radius=1;hostConfig.generatorVersion=1;hostConfig.seed=42;hostConfig.populate=false;
        vc::server::Host server(hostConfig);
        auto remote=config;remote.worldDirectory.clear();remote.connectHost="127.0.0.1";remote.connectPort=server.port();remote.playerName="SessionPlayer";
        const auto networkTick=[&] { server.tick(1.0/60);tick(); };
        for(int cycle=0;cycle<2;++cycle) {
            app.startRemote(remote);
            until([&] { return app.screen()==vc::client::ApplicationScreen::Gameplay&&app.renderer().meshCount()>0; },networkTick);
            require(server.playerCount()==1,"Remote session did not reach authoritative host");
#if VC_HAS_VOXELUI
            if(cycle==0) {
                vc::Input pause;pause.pause=true;app.frame(pause);
                require(!SDL_GetWindowRelativeMouseMode(nativeWindow),"Pause did not release gameplay pointer capture");
                const auto pausedAt=server.ticks();for(int step=0;step<4;++step) networkTick();
                require(server.ticks()>pausedAt&&server.playerCount()==1,"Multiplayer pause stopped the authoritative session");
                until([&] { return renderer->draw(vc::Camera{},captures/"voxelui-remote-pause.ppm"); },networkTick);
                vc::Input next;next.text.tab=true;for(int step=0;step<3;++step) app.frame(next);
                vc::Input enter;enter.text.enter=true;app.frame(enter);
            } else app.requestLeave();
#else
            app.requestLeave();
#endif
            until([&] { return !app.hasSession()&&server.playerCount()==0; },networkTick);
            require(app.renderer().meshCount()==0,"Remote geometry survived disconnect");
        }
        // Same remote lifecycle can be cancelled while waiting for the handshake.
        app.startRemote(remote);networkTick();app.requestLeave();
        until([&] { return !app.hasSession()&&server.playerCount()==0; },networkTick);
        auto mismatch=remote;mismatch.noMods=false;mismatch.modsDirectory=vc::utf8Path(argv[2]);
        app.startRemote(mismatch);until([&] { return !app.hasSession(); },networkTick);
        require(app.screen()==vc::client::ApplicationScreen::Error&&server.playerCount()==0,"Rejected handshake did not leave a recoverable error");
        vc::Input back;back.closeUi=true;app.frame(back);
        require(app.screen()==vc::client::ApplicationScreen::MainMenu,"Error back navigation failed");
        tick();until([&] { return app.renderer().draw(vc::Camera{},captures/"returned-menu.ppm"); },tick);
        app.shutdown();const auto validation=app.validationReport();
        require(validation.enabled&&validation.errors==0&&validation.warnings==0,"Session lifecycle Vulkan validation failed");
        std::cout<<"Application menu, cancel, repeated local/save retry, failed load, remote reconnect and shutdown passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
