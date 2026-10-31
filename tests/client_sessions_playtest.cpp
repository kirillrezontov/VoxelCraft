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
        require(argc==3,"Expected capture and example-mod directories");
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

        // Cancel before the first terrain job is accepted; the store lock must go.
        app.startLocal(config);tick();app.requestLeave();
        until([&] { return !app.hasSession(); },tick);
        { vc::voxel::WorldStore released(config.worldDirectory); }

        for(int cycle=0;cycle<3;++cycle) {
            app.startLocal(config);
            until([&] { return app.screen()==vc::client::ApplicationScreen::Gameplay; },tick);
            require(&app.renderer()==renderer,"Renderer owner changed between sessions");
            for(int i=0;i<8;++i) tick();
            if(cycle==1) {
                const auto snapshots=config.worldDirectory/"snapshots";
                const auto backup=config.worldDirectory/"snapshots-backup";
                std::filesystem::rename(snapshots,backup);std::ofstream(snapshots)<<"block save I/O";
                app.requestLeave();tick();
                until([&] { return app.screen()==vc::client::ApplicationScreen::Gameplay; },tick);
                require(app.hasSession()&&app.renderer().meshCount()>0,"Save failure discarded the live world");
                std::filesystem::remove(snapshots);std::filesystem::rename(backup,snapshots);
            }
            // Exercise the actual fourth legacy pause control on the first cycle.
            if(cycle==0) {
                vc::Input pause;pause.pause=true;app.frame(pause);
                const vc::SandboxUiLayout layout(config.width,config.height);
                auto click=[&](vc::UiRect rect) { vc::Input input;input.primaryClick=true;input.cursorX=rect.x+rect.width/2;input.cursorY=rect.y+rect.height/2;app.frame(input); };
                click(layout.pauseButton(1));click(layout.settingsButton(1));
                require(app.settings().fieldOfView==80,"Application did not receive session settings");
                const auto settings=vc::loadSettings(config.settingsFile);
                require(settings&&settings->settings==app.settings(),"Application settings did not persist");
                vc::Input back;back.closeUi=true;app.frame(back);
                const auto button=layout.pauseButton(3);vc::Input leave;
                leave.primaryClick=true;leave.cursorX=button.x+button.width/2;leave.cursorY=button.y+button.height/2;
                app.frame(leave);
            } else app.requestLeave();
            until([&] { return !app.hasSession(); },tick);
            require(app.running()&&app.screen()==vc::client::ApplicationScreen::MainMenu,"Did not return to usable menu");
            require(app.renderer().meshCount()==0,"Prior session geometry survived teardown");
            vc::voxel::WorldStore reopened(config.worldDirectory);vc::voxel::BlockRegistry blocks;
            const auto archive=reopened.load(blocks);require(archive&&archive->revision==static_cast<unsigned>(cycle+1),"Session did not save/release/reload world");
        }
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
            app.requestLeave();until([&] { return !app.hasSession()&&server.playerCount()==0; },networkTick);
            require(app.renderer().meshCount()==0,"Remote geometry survived disconnect");
        }
        // Same remote lifecycle can be cancelled while waiting for the handshake.
        app.startRemote(remote);networkTick();app.requestLeave();
        until([&] { return !app.hasSession()&&server.playerCount()==0; },networkTick);
        tick();until([&] { return app.renderer().draw(vc::Camera{},captures/"returned-menu.ppm"); },tick);
        app.shutdown();const auto validation=app.validationReport();
        require(validation.enabled&&validation.errors==0&&validation.warnings==0,"Session lifecycle Vulkan validation failed");
        std::cout<<"Application menu, cancel, repeated local/save retry, failed load, remote reconnect and shutdown passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
